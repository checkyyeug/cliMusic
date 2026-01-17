/**
 * @file ConfigWatcher.cpp
 * @brief Configuration watcher implementation
 */

#include "ConfigWatcher.h"
#include "utils/Logger.h"
#include "utils/ConfigValidator.h"
#include <chrono>
#include <thread>
#include <sys/stat.h>

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include <cstring>

using namespace xpu;

namespace xpu {
namespace daemon {

/**
 * @brief Implementation class
 */
class ConfigWatcher::Impl {
public:
    std::string config_file;
    std::map<std::string, utils::ConfigValue> current_config;
    ConfigChangeCallback callback;
    std::thread watch_thread;
    std::atomic<bool> running;
    time_t last_mod_time;
    int check_interval_ms;

    Impl()
        : running(false)
        , last_mod_time(0)
        , check_interval_ms(1000) {}
};

ConfigWatcher::ConfigWatcher()
    : impl_(std::make_unique<Impl>()) {}

ConfigWatcher::~ConfigWatcher() {
    stop();
}

ErrorCode ConfigWatcher::initialize(const std::string& config_file) {
    impl_->config_file = config_file;

    // Load initial config
    ErrorCode ret = loadConfig();
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to load initial config");
        return ret;
    }

    // Get initial modification time
    impl_->last_mod_time = getFileModTime();

    LOG_INFO("Config watcher initialized for: {}", config_file);
    return ErrorCode::Success;
}

ErrorCode ConfigWatcher::start() {
    if (impl_->running) {
        LOG_WARNING("Config watcher already running");
        return ErrorCode::Success;
    }

    impl_->running = true;
    impl_->watch_thread = std::thread([this]() { watchThreadFunc(); });

    LOG_INFO("Config watcher started");
    return ErrorCode::Success;
}

ErrorCode ConfigWatcher::stop() {
    if (!impl_->running) {
        return ErrorCode::Success;
    }

    impl_->running = false;

    if (impl_->watch_thread.joinable()) {
        impl_->watch_thread.join();
    }

    LOG_INFO("Config watcher stopped");
    return ErrorCode::Success;
}

void ConfigWatcher::setCallback(ConfigChangeCallback callback) {
    impl_->callback = callback;
}

std::map<std::string, utils::ConfigValue> ConfigWatcher::getCurrentConfig() const {
    return impl_->current_config;
}

ErrorCode ConfigWatcher::reloadConfig() {
    // Backup current config
    std::map<std::string, utils::ConfigValue> backup = impl_->current_config;

    // Load new config
    ErrorCode ret = loadConfig();
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to reload config, restoring backup");
        impl_->current_config = backup;
        return ret;
    }

    // Validate new config (basic validation without custom rules for now)
    // TODO: Add proper validation rules
    std::string error_message;
    std::vector<utils::ValidationRule> rules;  // Empty rules = basic validation
    utils::ConfigValidator validator;
    ret = validator.validate(impl_->current_config, rules, error_message);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Config validation failed: {}, restoring backup", error_message);
        impl_->current_config = backup;
        return ret;
    }

    // Notify callback
    if (impl_->callback) {
        impl_->callback(impl_->current_config);
    }

    LOG_INFO("Config reloaded successfully");
    return ErrorCode::Success;
}

void ConfigWatcher::watchThreadFunc() {
    while (impl_->running) {
        if (hasConfigChanged()) {
            LOG_INFO("Config file changed, reloading");

            // Reload config
            ErrorCode ret = reloadConfig();
            if (ret != ErrorCode::Success) {
                LOG_ERROR("Failed to reload config: {}", static_cast<int>(ret));
            }

            // Update modification time
            impl_->last_mod_time = getFileModTime();
        }

        // Sleep for check interval
        std::this_thread::sleep_for(std::chrono::milliseconds(impl_->check_interval_ms));
    }
}

bool ConfigWatcher::hasConfigChanged() const {
    time_t current_mod_time = getFileModTime();
    return (current_mod_time > impl_->last_mod_time);
}

ErrorCode ConfigWatcher::loadConfig() {
    ErrorCode ret = utils::ConfigLoader::loadFromFile(impl_->config_file, impl_->current_config);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to load config: {}", impl_->config_file);
        return ret;
    }

    return ErrorCode::Success;
}

time_t ConfigWatcher::getFileModTime() const {
#ifdef PLATFORM_WINDOWS
    struct _stat file_stat;
    if (_stat(impl_->config_file.c_str(), &file_stat) != 0) {
#else
    struct stat file_stat;
    if (stat(impl_->config_file.c_str(), &file_stat) != 0) {
#endif
        return 0;
    }
    return file_stat.st_mtime;
}

} // namespace daemon
} // namespace xpu
