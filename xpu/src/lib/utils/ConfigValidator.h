#ifndef XPU_CONFIG_VALIDATOR_H
#define XPU_CONFIG_VALIDATOR_H

#include "ConfigLoader.h"
#include "protocol/ErrorCode.h"
#include <string>
#include <vector>

namespace xpu {
namespace utils {

/**
 * @brief Configuration validation rule
 */
struct ValidationRule {
    std::string key;
    bool required;
    ConfigType type;
    ConfigValue min_value;
    ConfigValue max_value;
    std::vector<ConfigValue> allowed_values;

    ValidationRule()
        : required(false)
        , type(ConfigType::String) {}

    ValidationRule(const std::string& k, bool r, ConfigType t)
        : key(k)
        , required(r)
        , type(t) {}
};

/**
 * @brief Configuration validator
 */
class ConfigValidator {
public:
    /**
     * @brief Validate configuration against rules
     */
    static ErrorCode validate(const std::map<std::string, ConfigValue>& config,
                              const std::vector<ValidationRule>& rules,
                              std::string& error_message) {
        // Check required fields
        for (const auto& rule : rules) {
            if (rule.required) {
                auto it = config.find(rule.key);
                if (it == config.end()) {
                    error_message = "Required field missing: " + rule.key;
                    LOG_ERROR("Config validation failed: {}", error_message);
                    return ErrorCode::InvalidArgument;
                }

                // Check type
                if (it->second.type != rule.type) {
                    error_message = "Type mismatch for field: " + rule.key;
                    LOG_ERROR("Config validation failed: {}", error_message);
                    return ErrorCode::InvalidArgument;
                }

                // Check range
                if (!rule.min_value.string_value.empty() || !rule.max_value.string_value.empty()) {
                    if (!checkRange(it->second, rule.min_value, rule.max_value)) {
                        error_message = "Value out of range for field: " + rule.key;
                        LOG_ERROR("Config validation failed: {}", error_message);
                        return ErrorCode::InvalidArgument;
                    }
                }

                // Check allowed values
                if (!rule.allowed_values.empty()) {
                    bool found = false;
                    for (const auto& allowed : rule.allowed_values) {
                        if (it->second.string_value == allowed.string_value) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        error_message = "Value not allowed for field: " + rule.key;
                        LOG_ERROR("Config validation failed: {}", error_message);
                        return ErrorCode::InvalidArgument;
                    }
                }
            }
        }

        LOG_INFO("Configuration validation passed");
        return ErrorCode::Success;
    }

    /**
     * @brief Get default validation rules for XPU
     */
    static std::vector<ValidationRule> getDefaultRules() {
        std::vector<ValidationRule> rules;

        // Playback section
        rules.push_back(ValidationRule("playback.device", false, ConfigType::String));
        rules.push_back(ValidationRule("playback.sample_rate", false, ConfigType::Integer));
        rules.push_back(ValidationRule("playback.channels", false, ConfigType::Integer));
        rules.push_back(ValidationRule("playback.buffer_size", false, ConfigType::Integer));
        rules.push_back(ValidationRule("playback.latency_ms", false, ConfigType::Integer));

        // FFT Cache section
        rules.push_back(ValidationRule("fft_cache.enabled", false, ConfigType::Boolean));
        rules.push_back(ValidationRule("fft_cache.cache_dir", false, ConfigType::String));
        rules.push_back(ValidationRule("fft_cache.max_size_mb", false, ConfigType::Integer));
        rules.push_back(ValidationRule("fft_cache.fft_size", false, ConfigType::Integer));

        // Queue section
        rules.push_back(ValidationRule("queue.persistent", false, ConfigType::Boolean));
        rules.push_back(ValidationRule("queue.queue_file", false, ConfigType::String));
        rules.push_back(ValidationRule("queue.max_items", false, ConfigType::Integer));

        // Logging section
        rules.push_back(ValidationRule("logging.level", false, ConfigType::String));
        rules.push_back(ValidationRule("logging.file", false, ConfigType::String));
        rules.push_back(ValidationRule("logging.rotation", false, ConfigType::Boolean));

        // Audio processing section
        rules.push_back(ValidationRule("audio_processing.resample_quality", false, ConfigType::String));

        return rules;
    }

    /**
     * @brief Validate specific configuration values
     */
    static ErrorCode validateSampleRate(int sample_rate) {
        const int valid_rates[] = {44100, 48000, 96000, 192000, 384000, 768000};

        for (int rate : valid_rates) {
            if (sample_rate == rate) {
                return ErrorCode::Success;
            }
        }

        LOG_ERROR("Invalid sample rate: {}", sample_rate);
        return ErrorCode::SampleRateNotSupported;
    }

    static ErrorCode validateBitDepth(int bit_depth) {
        if (bit_depth != 16 && bit_depth != 24 && bit_depth != 32) {
            LOG_ERROR("Invalid bit depth: {}", bit_depth);
            return ErrorCode::BitDepthNotSupported;
        }
        return ErrorCode::Success;
    }

    static ErrorCode validateChannels(int channels) {
        if (channels < 1 || channels > 8) {
            LOG_ERROR("Invalid channel count: {}", channels);
            return ErrorCode::ChannelConfigurationError;
        }
        return ErrorCode::Success;
    }

    static ErrorCode validateBufferSize(int buffer_size) {
        if (buffer_size < 256 || buffer_size > 16384) {
            LOG_ERROR("Invalid buffer size: {}", buffer_size);
            return ErrorCode::InvalidArgument;
        }
        return ErrorCode::Success;
    }

private:
    static bool checkRange(const ConfigValue& value,
                           const ConfigValue& min_value,
                           const ConfigValue& max_value) {
        switch (value.type) {
            case ConfigType::Integer:
                if (!min_value.string_value.empty() && value.int_value < min_value.int_value) {
                    return false;
                }
                if (!max_value.string_value.empty() && value.int_value > max_value.int_value) {
                    return false;
                }
                break;

            case ConfigType::Float:
                if (!min_value.string_value.empty() && value.float_value < min_value.float_value) {
                    return false;
                }
                if (!max_value.string_value.empty() && value.float_value > max_value.float_value) {
                    return false;
                }
                break;

            default:
                break;
        }

        return true;
    }
};

} // namespace utils
} // namespace xpu

#endif // XPU_CONFIG_VALIDATOR_H
