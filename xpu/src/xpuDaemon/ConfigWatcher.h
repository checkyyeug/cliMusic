/**
 * @file ConfigWatcher.h
 * @brief Configuration file watching for hot-reload
 */

#ifndef XPU_DAEMON_CONFIG_WATCHER_H
#define XPU_DAEMON_CONFIG_WATCHER_H

#include "protocol/ErrorCode.h"
#include "utils/ConfigLoader.h"
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace xpu {
namespace daemon {

/**
 * @brief Config change callback
 */
using ConfigChangeCallback = std::function<void(const std::map<std::string, utils::ConfigValue>&)>;

/**
 * @brief Configuration watcher
 */
class ConfigWatcher {
public:
    ConfigWatcher();
    ~ConfigWatcher();

    /**
     * @brief Initialize config watcher
     */
    ErrorCode initialize(const std::string& config_file);

    /**
     * @brief Start watching
     */
    ErrorCode start();

    /**
     * @brief Stop watching
     */
    ErrorCode stop();

    /**
     * @brief Set callback for config changes
     */
    void setCallback(ConfigChangeCallback callback);

    /**
     * @brief Get current config
     */
    std::map<std::string, utils::ConfigValue> getCurrentConfig() const;

    /**
     * @brief Reload config manually
     */
    ErrorCode reloadConfig();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Watch thread function
     */
    void watchThreadFunc();

    /**
     * @brief Check if config file changed
     */
    bool hasConfigChanged() const;

    /**
     * @brief Load config
     */
    ErrorCode loadConfig();

    /**
     * @brief Get file modification time
     */
    time_t getFileModTime() const;
};

} // namespace daemon
} // namespace xpu

#endif // XPU_DAEMON_CONFIG_WATCHER_H
