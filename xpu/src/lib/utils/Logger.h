#ifndef XPU_LOGGER_H
#define XPU_LOGGER_H

#include "protocol/ErrorCode.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>
#include <iostream>

namespace xpu {
namespace utils {

/**
 * @brief Logger class for XPU system
 */
class Logger {
public:
    /**
     * @brief Get logger instance
     */
    static std::shared_ptr<spdlog::logger> getInstance() {
        static std::shared_ptr<spdlog::logger> instance = createLogger();
        return instance;
    }

    /**
     * @brief Initialize logger
     */
    static void initialize(const std::string& log_file, bool console = true) {
        (void)log_file;  // Suppress unused parameter warning
        (void)console;    // Suppress unused parameter warning
        
        auto logger = getInstance();
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_default_logger(logger);
    }

    /**
     * @brief Log error
     */
    static void logError(ErrorCode code, const std::string& module, const std::string& detail) {
        auto logger = getInstance();
        logger->error("[{}:{}] {} - {}", static_cast<int>(code), module, toString(code), detail);
    }

    /**
     * @brief Log JSON
     */
    static void logJSON(const std::string& json) {
        auto logger = getInstance();
        logger->info("JSON: {}", json);
    }

private:
    static std::shared_ptr<spdlog::logger> createLogger() {
        try {
            // Try to create colored stdout logger
            auto logger = spdlog::stdout_color_mt("xpu");
            spdlog::register_logger(logger);
            return logger;
        } catch (...) {
            // Fallback: just return the default logger
            return spdlog::default_logger();
        }
    }
};

// Convenience macros with format support
#define LOG_TRACE(fmt, ...) xpu::utils::Logger::getInstance()->trace(fmt, __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) xpu::utils::Logger::getInstance()->debug(fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...)  xpu::utils::Logger::getInstance()->info(fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  xpu::utils::Logger::getInstance()->warn(fmt, __VA_ARGS__)
#define LOG_WARNING(fmt, ...) xpu::utils::Logger::getInstance()->warn(fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) xpu::utils::Logger::getInstance()->error(fmt, __VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) xpu::utils::Logger::getInstance()->critical(fmt, __VA_ARGS__)

#define LOG_ERROR_CODE(code, module, detail) \
    xpu::utils::Logger::logError(code, module, detail)

} // namespace utils
} // namespace xpu

#endif // XPU_LOGGER_H
