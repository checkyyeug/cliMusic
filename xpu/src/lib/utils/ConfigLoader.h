#ifndef XPU_CONFIG_LOADER_H
#define XPU_CONFIG_LOADER_H

#include <string>
#include <map>
#include <vector>
#include <fstream>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#include "protocol/ErrorCode.h"
#include "utils/Logger.h"

namespace xpu {
namespace utils {

/**
 * @brief Configuration value type
 */
enum class ConfigType {
    String,
    Integer,
    Float,
    Boolean,
    StringList
};

/**
 * @brief Configuration value wrapper
 */
struct ConfigValue {
    ConfigType type;
    std::string string_value;
    int int_value;
    float float_value;
    bool bool_value;
    std::vector<std::string> string_list_value;

    ConfigValue()
        : type(ConfigType::String)
        , int_value(0)
        , float_value(0.0f)
        , bool_value(false) {}

    explicit ConfigValue(const std::string& s)
        : type(ConfigType::String)
        , string_value(s)
        , int_value(0)
        , float_value(0.0f)
        , bool_value(false) {}

    explicit ConfigValue(int i)
        : type(ConfigType::Integer)
        , int_value(i)
        , float_value(static_cast<float>(i))
        , bool_value(i != 0) {}

    explicit ConfigValue(float f)
        : type(ConfigType::Float)
        , float_value(f)
        , int_value(static_cast<int>(f))
        , bool_value(f != 0.0f) {}

    explicit ConfigValue(bool b)
        : type(ConfigType::Boolean)
        , bool_value(b)
        , int_value(b ? 1 : 0)
        , float_value(b ? 1.0f : 0.0f) {}

    explicit ConfigValue(const std::vector<std::string>& sl)
        : type(ConfigType::StringList)
        , string_list_value(sl)
        , int_value(0)
        , float_value(0.0f)
        , bool_value(false) {}

    std::string asString() const { return string_value; }
    int asInt() const { return int_value; }
    float asFloat() const { return float_value; }
    bool asBool() const { return bool_value; }
    std::vector<std::string> asStringList() const { return string_list_value; }
};

/**
 * @brief Configuration loader and manager
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from file
     */
    static ErrorCode loadFromFile(const std::string& filepath,
                                   std::map<std::string, ConfigValue>& config) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR_CODE(ErrorCode::FileNotFound, "ConfigLoader",
                          "Config file not found: " + filepath);
            return ErrorCode::FileNotFound;
        }

        std::string line;
        std::string current_section;
        int line_number = 0;

        while (std::getline(file, line)) {
            line_number++;

            // Trim whitespace
            trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }

            // Section header
            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.length() - 2);
                trim(current_section);
                continue;
            }

            // Key-value pair
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                LOG_WARN("Invalid config line {}: {}", line_number, line);
                continue;
            }

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            trim(key);
            trim(value);

            // Remove quotes
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            // Build full key with section
            std::string full_key;
            if (!current_section.empty()) {
                full_key = current_section + "." + key;
            } else {
                full_key = key;
            }

            // Parse value type
            ConfigValue config_value = parseValue(value);
            config[full_key] = config_value;

            LOG_DEBUG("Config: {} = {}", full_key, value);
        }

        file.close();

        LOG_INFO("Loaded configuration from {}", filepath);
        return ErrorCode::Success;
    }

    /**
     * @brief Save configuration to file
     */
    static ErrorCode saveToFile(const std::string& filepath,
                                 const std::map<std::string, ConfigValue>& config) {
        // Write to temp file first, then rename (atomic operation)
        std::string temp_filepath = filepath + ".tmp";

        std::ofstream file(temp_filepath);
        if (!file.is_open()) {
            LOG_ERROR_CODE(ErrorCode::FileWriteError, "ConfigLoader",
                          "Failed to write config file: " + temp_filepath);
            return ErrorCode::FileWriteError;
        }

        // Group by section
        std::map<std::string, std::map<std::string, ConfigValue>> sections;
        for (const auto& [key, value] : config) {
            size_t dot_pos = key.find('.');
            std::string section;
            std::string entry_key;

            if (dot_pos != std::string::npos) {
                section = key.substr(0, dot_pos);
                entry_key = key.substr(dot_pos + 1);
            } else {
                section = "";
                entry_key = key;
            }

            sections[section][entry_key] = value;
        }

        // Write sections
        for (const auto& [section, entries] : sections) {
            if (!section.empty()) {
                file << "[" << section << "]" << std::endl;
            }

            for (const auto& [key, value] : entries) {
                file << key << " = ";

                switch (value.type) {
                    case ConfigType::String:
                        file << "\"" << value.string_value << "\"";
                        break;
                    case ConfigType::Integer:
                        file << value.int_value;
                        break;
                    case ConfigType::Float:
                        file << value.float_value;
                        break;
                    case ConfigType::Boolean:
                        file << (value.bool_value ? "true" : "false");
                        break;
                    case ConfigType::StringList:
                        file << "[";
                        for (size_t i = 0; i < value.string_list_value.size(); ++i) {
                            if (i > 0) file << ", ";
                            file << "\"" << value.string_list_value[i] << "\"";
                        }
                        file << "]";
                        break;
                }

                file << std::endl;
            }

            file << std::endl;
        }

        file.close();

        // Atomic rename
#ifdef PLATFORM_WINDOWS
        if (!MoveFileExA(temp_filepath.c_str(), filepath.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return ErrorCode::FileWriteError;
        }
#else
        if (std::rename(temp_filepath.c_str(), filepath.c_str()) != 0) {
            return ErrorCode::FileWriteError;
        }
#endif

        LOG_INFO("Saved configuration to {}", filepath);
        return ErrorCode::Success;
    }

    /**
     * @brief Get configuration value
     */
    static ConfigValue getValue(const std::map<std::string, ConfigValue>& config,
                                 const std::string& key,
                                 const ConfigValue& default_value = ConfigValue()) {
        auto it = config.find(key);
        if (it != config.end()) {
            return it->second;
        }
        return default_value;
    }

private:
    static void trim(std::string& str) {
        // Left trim
        str.erase(str.begin(),
                  std::find_if(str.begin(), str.end(),
                              [](int ch) { return !std::isspace(ch); }));

        // Right trim
        str.erase(std::find_if(str.rbegin(), str.rend(),
                              [](int ch) { return !std::isspace(ch); }).base(),
                  str.end());
    }

    static ConfigValue parseValue(const std::string& str) {
        // Boolean
        if (str == "true" || str == "yes" || str == "on") {
            return ConfigValue(true);
        }
        if (str == "false" || str == "no" || str == "off") {
            return ConfigValue(false);
        }

        // Integer
        try {
            size_t pos;
            int int_val = std::stoi(str, &pos);
            if (pos == str.length()) {
                return ConfigValue(int_val);
            }
        } catch (...) {
            // Not an integer
        }

        // Float
        try {
            size_t pos;
            float float_val = std::stof(str, &pos);
            if (pos == str.length()) {
                return ConfigValue(float_val);
            }
        } catch (...) {
            // Not a float
        }

        // List (simplified)
        if (!str.empty() && str.front() == '[' && str.back() == ']') {
            std::vector<std::string> list;
            std::string content = str.substr(1, str.length() - 2);
            // TODO: Parse list properly
            return ConfigValue(list);
        }

        // String (default)
        return ConfigValue(str);
    }
};

} // namespace utils
} // namespace xpu

#endif // XPU_CONFIG_LOADER_H
