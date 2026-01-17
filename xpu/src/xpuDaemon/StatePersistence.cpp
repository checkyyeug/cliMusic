/**
 * @file StatePersistence.cpp
 * @brief State persistence implementation
 */

#include "StatePersistence.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <fstream>
#include <cstring>
#include <sys/stat.h>

using namespace xpu;

namespace xpu {
namespace daemon {

/**
 * @brief Current state format version
 */
static const std::string STATE_VERSION = "1.0";

StatePersistence::StatePersistence() {
    std::string config_dir = utils::PlatformUtils::getConfigDirectory();
    state_file_ = config_dir + "/state.json";
    backup_file_ = state_file_ + ".backup";
}

ErrorCode StatePersistence::initialize(const std::string& state_file) {
    state_file_ = state_file;
    backup_file_ = state_file + ".backup";

    LOG_INFO("State persistence initialized: {}", state_file_);
    return ErrorCode::Success;
}

ErrorCode StatePersistence::saveState(const AppState& state) {
    // Validate state
    ErrorCode ret = validateState(state);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Invalid state");
        return ret;
    }

    // Write to temp file first
    std::string temp_file = state_file_ + ".tmp";
    ret = writeStateToFile(temp_file, state);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to write state to temp file");
        return ret;
    }

    // Create backup
    createBackup();

    // Atomic rename
    if (std::rename(temp_file.c_str(), state_file_.c_str()) != 0) {
        LOG_ERROR("Failed to rename temp file to state file");
        return ErrorCode::FileWriteError;
    }

    LOG_DEBUG("State saved to: {}", state_file_);
    return ErrorCode::Success;
}

ErrorCode StatePersistence::loadState(AppState& state) {
    // Check if state file exists
    struct stat buffer;
    if (stat(state_file_.c_str(), &buffer) != 0) {
        LOG_INFO("State file not found, using defaults");
        state = AppState();  // Use defaults
        return ErrorCode::FileNotFound;
    }

    // Read state from file
    ErrorCode ret = readStateFromFile(state_file_, state);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to read state file");

        // Try to load backup
        if (stat(backup_file_.c_str(), &buffer) == 0) {
            LOG_INFO("Attempting to load backup state");
            ret = readStateFromFile(backup_file_, state);
            if (ret == ErrorCode::Success) {
                LOG_INFO("Successfully loaded backup state");
            }
        }

        return ret;
    }

    // Check version and migrate if needed
    if (state.version != STATE_VERSION) {
        LOG_INFO("State version mismatch, migrating from {} to {}",
                 state.version, STATE_VERSION);
        ret = migrateState(state, state.version);
        if (ret != ErrorCode::Success) {
            LOG_WARNING("State migration failed");
        }
    }

    // Validate loaded state
    ret = validateState(state);
    if (ret != ErrorCode::Success) {
        LOG_ERROR("Loaded state is invalid");
        return ret;
    }

    LOG_INFO("State loaded from: {}", state_file_);
    return ErrorCode::Success;
}

ErrorCode StatePersistence::updatePlaybackState(const PlaybackState& playback) {
    AppState state;
    ErrorCode ret = loadState(state);
    if (ret != ErrorCode::Success && ret != ErrorCode::FileNotFound) {
        return ret;
    }

    state.playback = playback;
    return saveState(state);
}

ErrorCode StatePersistence::updateQueueState(const QueueState& queue) {
    AppState state;
    ErrorCode ret = loadState(state);
    if (ret != ErrorCode::Success && ret != ErrorCode::FileNotFound) {
        return ret;
    }

    state.queue = queue;
    return saveState(state);
}

std::string StatePersistence::getStateFilePath() const {
    return state_file_;
}

ErrorCode StatePersistence::createBackup() {
    struct stat buffer;
    if (stat(state_file_.c_str(), &buffer) == 0) {
        // Copy current state to backup
        std::ifstream src(state_file_, std::ios::binary);
        std::ofstream dst(backup_file_, std::ios::binary);

        if (!src.is_open() || !dst.is_open()) {
            LOG_WARNING("Failed to create backup state");
            return ErrorCode::FileWriteError;
        }

        dst << src.rdbuf();
        src.close();
        dst.close();

        LOG_DEBUG("Backup created: {}", backup_file_);
    }

    return ErrorCode::Success;
}

ErrorCode StatePersistence::validateState(const AppState& state) const {
    // Validate version
    if (state.version.empty()) {
        LOG_ERROR("State version is empty");
        return ErrorCode::InvalidState;
    }

    // Validate playback state
    if (state.playback.position < 0.0) {
        LOG_ERROR("Invalid playback position");
        return ErrorCode::InvalidState;
    }

    if (state.playback.volume < 0.0f || state.playback.volume > 2.0f) {
        LOG_ERROR("Invalid volume: {}", state.playback.volume);
        return ErrorCode::InvalidState;
    }

    // Validate EQ gains (-20dB to +20dB)
    if (state.playback.eq_bass < -20.0f || state.playback.eq_bass > 20.0f) {
        LOG_ERROR("Invalid bass gain: {}", state.playback.eq_bass);
        return ErrorCode::InvalidState;
    }

    if (state.playback.eq_mid < -20.0f || state.playback.eq_mid > 20.0f) {
        LOG_ERROR("Invalid mid gain: {}", state.playback.eq_mid);
        return ErrorCode::InvalidState;
    }

    if (state.playback.eq_treble < -20.0f || state.playback.eq_treble > 20.0f) {
        LOG_ERROR("Invalid treble gain: {}", state.playback.eq_treble);
        return ErrorCode::InvalidState;
    }

    // Validate queue state
    if (state.queue.current_index < 0) {
        LOG_ERROR("Invalid queue index");
        return ErrorCode::InvalidState;
    }

    if (state.queue.current_index >= static_cast<int>(state.queue.track_list.size())) {
        LOG_ERROR("Queue index out of bounds");
        return ErrorCode::InvalidState;
    }

    return ErrorCode::Success;
}

ErrorCode StatePersistence::migrateState(AppState& state, const std::string& from_version) const {
    // Migration logic for future versions
    // For now, just update version
    state.version = STATE_VERSION;

    LOG_INFO("State migrated from {} to {}", from_version, STATE_VERSION);
    return ErrorCode::Success;
}

ErrorCode StatePersistence::writeStateToFile(const std::string& file_path,
                                            const AppState& state) {
    std::ofstream out(file_path);
    if (!out.is_open()) {
        LOG_ERROR("Failed to open state file for writing: {}", file_path);
        return ErrorCode::FileWriteError;
    }

    // Write JSON manually (simplified)
    out << "{\n";
    out << "  \"version\": \"" << state.version << "\",\n";

    // Playback state
    out << "  \"playback\": {\n";
    out << "    \"current_track\": \"" << state.playback.current_track << "\",\n";
    out << "    \"position\": " << state.playback.position << ",\n";
    out << "    \"is_playing\": " << (state.playback.is_playing ? "true" : "false") << ",\n";
    out << "    \"playback_mode\": \"" << state.playback.playback_mode << "\",\n";
    out << "    \"volume\": " << state.playback.volume << ",\n";
    out << "    \"eq_preset\": \"" << state.playback.eq_preset << "\",\n";
    out << "    \"eq_bass\": " << state.playback.eq_bass << ",\n";
    out << "    \"eq_mid\": " << state.playback.eq_mid << ",\n";
    out << "    \"eq_treble\": " << state.playback.eq_treble << "\n";
    out << "  },\n";

    // Queue state
    out << "  \"queue\": {\n";
    out << "    \"current_index\": " << state.queue.current_index << ",\n";
    out << "    \"track_list\": [";
    for (size_t i = 0; i < state.queue.track_list.size(); ++i) {
        out << "\"" << state.queue.track_list[i] << "\"";
        if (i < state.queue.track_list.size() - 1) {
            out << ", ";
        }
    }
    out << "]\n";
    out << "  }\n";

    out << "}\n";

    out.close();
    return ErrorCode::Success;
}

ErrorCode StatePersistence::readStateFromFile(const std::string& file_path,
                                             AppState& state) {
    std::ifstream in(file_path);
    if (!in.is_open()) {
        LOG_ERROR("Failed to open state file for reading: {}", file_path);
        return ErrorCode::FileReadError;
    }

    // Simple JSON parsing (for production, use proper JSON library)
    std::string line;
    while (std::getline(in, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.find("\"version\":") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            std::string value = line.substr(pos);
            // Remove quotes and comma
            value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
            value.erase(std::remove(value.begin(), value.end(), ','), value.end());
            state.version = value;
        }
        // Add more parsing logic for other fields...
    }

    in.close();
    return ErrorCode::Success;
}

} // namespace daemon
} // namespace xpu
