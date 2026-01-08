#ifndef XPU_ERROR_RESPONSE_H
#define XPU_ERROR_RESPONSE_H

#include "ErrorCode.h"
#include <string>
#include <chrono>
#include <cstdint>

namespace xpu {

/**
 * @brief Standardized error response format (JSON)
 */
struct ErrorResponse {
    ErrorCode code;
    std::string message;
    std::string module;
    std::string detail;
    std::string timestamp;
    int http_status_code;

    /**
     * @brief Default constructor
     */
    ErrorResponse()
        : code(ErrorCode::UnknownError)
        , http_status_code(500) {
        setTimestamp();
    }

    /**
     * @brief Constructor with error code
     */
    explicit ErrorResponse(ErrorCode err)
        : code(err)
        , message(toString(err))
        , http_status_code(getHTTPStatusCode(err)) {
        setTimestamp();
    }

    /**
     * @brief Full constructor
     */
    ErrorResponse(ErrorCode err, const std::string& mod, const std::string& det)
        : code(err)
        , message(toString(err))
        , module(mod)
        , detail(det)
        , http_status_code(getHTTPStatusCode(err)) {
        setTimestamp();
    }

    /**
     * @brief Set current timestamp in ISO 8601 format
     */
    void setTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        char buffer[64];
#ifdef _MSC_VER
        struct tm local_time;
        localtime_s(&local_time, &time_t);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local_time);
#else
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S",
                     std::localtime(&time_t));
#endif

        timestamp = std::string(buffer) + "." +
                   std::to_string(ms.count()) + "Z";
    }

    /**
     * @brief Convert to JSON string
     */
    std::string toJSON() const {
        std::string json = "{\n";
        json += "  \"error\": {\n";
        json += "    \"code\": " + std::to_string(static_cast<int>(code)) + ",\n";
        json += "    \"message\": \"" + escapeJSON(message) + "\",\n";
        if (!module.empty()) {
            json += "    \"module\": \"" + escapeJSON(module) + "\",\n";
        }
        if (!detail.empty()) {
            json += "    \"detail\": \"" + escapeJSON(detail) + "\",\n";
        }
        json += "    \"timestamp\": \"" + timestamp + "\",\n";
        json += "    \"http_status\": " + std::to_string(http_status_code) + "\n";
        json += "  }\n";
        json += "}\n";
        return json;
    }

    /**
     * @brief Create success response
     */
    static ErrorResponse success() {
        return ErrorResponse(ErrorCode::Success);
    }

    /**
     * @brief Create file not found error
     */
    static ErrorResponse fileNotFound(const std::string& filepath) {
        return ErrorResponse(ErrorCode::FileNotFound, "FileSystem",
                           "File not found: " + filepath);
    }

    /**
     * @brief Create unsupported format error
     */
    static ErrorResponse unsupportedFormat(const std::string& format) {
        return ErrorResponse(ErrorCode::UnsupportedFormat, "AudioDecoder",
                           "Unsupported audio format: " + format);
    }

    /**
     * @brief Create device error
     */
    static ErrorResponse deviceError(const std::string& deviceName) {
        return ErrorResponse(ErrorCode::DeviceUnavailable, "AudioBackend",
                           "Device unavailable: " + deviceName);
    }

    /**
     * @brief Create cache error
     */
    static ErrorResponse cacheError(const std::string& cacheId) {
        return ErrorResponse(ErrorCode::CacheMiss, "FFTCache",
                           "Cache entry not found: " + cacheId);
    }

private:
    /**
     * @brief Escape special characters for JSON
     */
    static std::string escapeJSON(const std::string& str) {
        std::string escaped;
        escaped.reserve(static_cast<size_t>(str.size() * 1.2f));

        for (char c : str) {
            switch (c) {
                case '"':  escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (c < 32) {
                        char buffer[7];
                        snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                        escaped += buffer;
                    } else {
                        escaped += c;
                    }
                    break;
            }
        }

        return escaped;
    }
};

/**
 * @brief Standard success response format
 */
struct SuccessResponse {
    std::string message;
    std::string data;
    std::string timestamp;

    SuccessResponse()
        : message("Success") {
        setErrorTimestamp();
    }

    explicit SuccessResponse(const std::string& msg)
        : message(msg) {
        setErrorTimestamp();
    }

    SuccessResponse(const std::string& msg, const std::string& dat)
        : message(msg)
        , data(dat) {
        setErrorTimestamp();
    }

    void setErrorTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        char buffer[64];
#ifdef _MSC_VER
        struct tm local_time;
        localtime_s(&local_time, &time_t);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local_time);
#else
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S",
                     std::localtime(&time_t));
#endif

        timestamp = std::string(buffer) + "." +
                   std::to_string(ms.count()) + "Z";
    }

    std::string toJSON() const {
        std::string json = "{\n";
        json += "  \"success\": {\n";
        json += "    \"message\": \"" + message + "\",\n";
        json += "    \"timestamp\": \"" + timestamp + "\"\n";
        if (!data.empty()) {
            json += "    \"data\": " + data + ",\n";
        }
        json += "  }\n";
        json += "}\n";
        return json;
    }
};

} // namespace xpu

#endif // XPU_ERROR_RESPONSE_H
