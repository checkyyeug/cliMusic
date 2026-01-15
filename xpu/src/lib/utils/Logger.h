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
     * @param log_file Path to log file (not currently used)
     * @param console Whether to enable console logging
     * @param verbose Whether to enable verbose (debug) logging
     * @param program_name Program name to include in log output (e.g., "xpuLoad", "xpuPlay")
     */
    static void initialize(const std::string& log_file, bool console = true, bool verbose = false, const std::string& program_name = "xpu") {
        (void)log_file;  // Suppress unused parameter warning
        (void)console;    // Suppress unused parameter warning

        auto logger = getInstance();
        // Set log level based on verbose flag
        // verbose = true: show all logs (debug and above)
        // verbose = false: only show warnings and errors
        if (verbose) {
            spdlog::set_level(spdlog::level::debug);
        } else {
            spdlog::set_level(spdlog::level::warn);
        }

        // Set custom pattern with program name
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [" + program_name + "] [%^%l%$] %v");
        spdlog::set_default_logger(logger);
    }

    /**
     * @brief Set log level dynamically
     * @param verbose Whether to enable verbose (debug) logging
     */
    static void setVerbose(bool verbose) {
        if (verbose) {
            spdlog::set_level(spdlog::level::debug);
        } else {
            spdlog::set_level(spdlog::level::warn);
        }
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
            // Create stderr sink explicitly to avoid stdout
            auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("xpu", stderr_sink);
            spdlog::register_logger(logger);
            return logger;
        } catch (...) {
            // Fallback: try stderr_color_mt
            try {
                auto logger = spdlog::stderr_color_mt("xpu");
                spdlog::register_logger(logger);
                return logger;
            } catch (...) {
                // Last resort: return the default logger
                return spdlog::default_logger();
            }
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
