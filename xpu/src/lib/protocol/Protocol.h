#ifndef XPU_PROTOCOL_H
#define XPU_PROTOCOL_H

#include "ErrorCode.h"
#include "ErrorResponse.h"
#include <string>
#include <vector>
#include <map>

namespace xpu {
namespace protocol {

/**
 * @brief Audio metadata structure for inter-module communication
 */
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string genre;
    int track_number;
    double duration;  // in seconds
    int sample_rate;
    int bit_depth;
    int channels;
    uint64_t sample_count;
    std::string format;  // FLAC, WAV, ALAC, DSD, etc.
    std::string format_name;  // Display name
    double bitrate;      // in kbps
    std::string file_path;
    bool is_lossless;
    bool is_high_res;
    int original_sample_rate;  // Original sample rate before resampling

    AudioMetadata()
        : track_number(0)
        , duration(0.0)
        , sample_rate(0)
        , bit_depth(0)
        , channels(0)
        , sample_count(0)
        , bitrate(0.0)
        , is_lossless(false)
        , is_high_res(false)
        , original_sample_rate(0) {}
};

/**
 * @brief Convert AudioMetadata to JSON
 */
inline std::string metadataToJSON(const AudioMetadata& meta) {
    std::string json = "{\n";
    json += "  \"title\": \"" + meta.title + "\",\n";
    json += "  \"artist\": \"" + meta.artist + "\",\n";
    json += "  \"album\": \"" + meta.album + "\",\n";
    json += "  \"year\": \"" + meta.year + "\",\n";
    json += "  \"genre\": \"" + meta.genre + "\",\n";
    json += "  \"track_number\": " + std::to_string(meta.track_number) + ",\n";
    json += "  \"duration\": " + std::to_string(meta.duration) + ",\n";
    json += "  \"sample_rate\": " + std::to_string(meta.sample_rate) + ",\n";
    json += "  \"original_sample_rate\": " + std::to_string(meta.original_sample_rate) + ",\n";
    json += "  \"bit_depth\": " + std::to_string(meta.bit_depth) + ",\n";
    json += "  \"channels\": " + std::to_string(meta.channels) + ",\n";
    json += "  \"sample_count\": " + std::to_string(meta.sample_count) + ",\n";
    json += "  \"format\": \"" + meta.format + "\",\n";
    json += "  \"format_name\": \"" + meta.format_name + "\",\n";
    json += "  \"bitrate\": " + std::to_string(meta.bitrate) + ",\n";
    json += "  \"is_lossless\": " + std::string(meta.is_lossless ? "true" : "false") + ",\n";
    json += "  \"is_high_res\": " + std::string(meta.is_high_res ? "true" : "false") + ",\n";
    json += "  \"file_path\": \"" + meta.file_path + "\"\n";
    json += "}\n";
    return json;
}

/**
 * @brief Playback status structure
 */
struct PlaybackStatus {
    enum class State {
        Stopped,
        Playing,
        Paused,
        Error
    };

    State state;
    double current_position;     // in seconds
    double duration;             // in seconds
    float buffer_fill_level;     // percentage (0-100)
    float cpu_usage;             // percentage (0-100)
    int sample_rate;
    int bit_depth;
    int channels;
    std::string current_device;
    uint64_t bytes_played;
    double playback_time;        // in seconds
    float latency_ms;            // latency in milliseconds

    // Convenience aliases for backward compatibility
    double position;             // alias for current_position
    float buffer_fill;           // alias for buffer_fill_level

    PlaybackStatus()
        : state(State::Stopped)
        , current_position(0.0)
        , duration(0.0)
        , buffer_fill_level(0.0f)
        , cpu_usage(0.0f)
        , latency_ms(0.0f)
        , position(0.0)
        , buffer_fill(0.0f)
        , sample_rate(0)
        , bit_depth(0)
        , channels(0)
        , bytes_played(0)
        , playback_time(0.0) {}
};

/**
 * @brief Convert PlaybackStatus to JSON
 */
inline std::string statusToJSON(const PlaybackStatus& status) {
    const char* state_str = "";
    switch (status.state) {
        case PlaybackStatus::State::Stopped: state_str = "stopped"; break;
        case PlaybackStatus::State::Playing: state_str = "playing"; break;
        case PlaybackStatus::State::Paused:  state_str = "paused";  break;
        case PlaybackStatus::State::Error:   state_str = "error";   break;
    }

    std::string json = "{\n";
    json += "  \"status\": {\n";
    json += "    \"state\": \"" + std::string(state_str) + "\",\n";
    json += "    \"current_position\": " + std::to_string(status.current_position) + ",\n";
    json += "    \"duration\": " + std::to_string(status.duration) + ",\n";
    json += "    \"buffer_fill_level\": " + std::to_string(status.buffer_fill_level) + ",\n";
    json += "    \"cpu_usage\": " + std::to_string(status.cpu_usage) + ",\n";
    json += "    \"sample_rate\": " + std::to_string(status.sample_rate) + ",\n";
    json += "    \"bit_depth\": " + std::to_string(status.bit_depth) + ",\n";
    json += "    \"channels\": " + std::to_string(status.channels) + ",\n";
    json += "    \"current_device\": \"" + status.current_device + "\",\n";
    json += "    \"bytes_played\": " + std::to_string(status.bytes_played) + ",\n";
    json += "    \"playback_time\": " + std::to_string(status.playback_time) + "\n";
    json += "  }\n";
    json += "}\n";
    return json;
}

/**
 * @brief Queue entry structure
 */
struct QueueEntry {
    int index;
    AudioMetadata metadata;
    std::string file_path;
    bool is_playing;

    QueueEntry()
        : index(0)
        , is_playing(false) {}
};

/**
 * @brief Queue status structure
 */
struct QueueStatus {
    std::vector<QueueEntry> entries;
    int current_index;
    int total_count;
    std::string playback_mode;  // "sequential", "random", "loop_single", "loop_all"
    double total_duration;      // in seconds

    QueueStatus()
        : current_index(-1)
        , total_count(0)
        , playback_mode("sequential")
        , total_duration(0.0) {}
};

/**
 * @brief Convert QueueStatus to JSON
 */
inline std::string queueToJSON(const QueueStatus& queue) {
    std::string json = "{\n";
    json += "  \"queue\": {\n";
    json += "    \"current_index\": " + std::to_string(queue.current_index) + ",\n";
    json += "    \"total_count\": " + std::to_string(queue.total_count) + ",\n";
    json += "    \"playback_mode\": \"" + queue.playback_mode + "\",\n";
    json += "    \"total_duration\": " + std::to_string(queue.total_duration) + ",\n";
    json += "    \"entries\": [\n";

    for (size_t i = 0; i < queue.entries.size(); ++i) {
        const auto& entry = queue.entries[i];
        json += "      {\n";
        json += "        \"index\": " + std::to_string(entry.index) + ",\n";
        json += "        \"file_path\": \"" + entry.file_path + "\",\n";
        json += "        \"is_playing\": " + std::string(entry.is_playing ? "true" : "false") + ",\n";
        json += "        \"title\": \"" + entry.metadata.title + "\",\n";
        json += "        \"artist\": \"" + entry.metadata.artist + "\",\n";
        json += "        \"duration\": " + std::to_string(entry.metadata.duration) + "\n";
        json += "      }";
        if (i < queue.entries.size() - 1) {
            json += ",";
        }
        json += "\n";
    }

    json += "    ]\n";
    json += "  }\n";
    json += "}\n";
    return json;
}

/**
 * @brief Device information structure
 */
struct DeviceInfo {
    std::string name;
    std::string id;
    int index;
    std::vector<int> sample_rates;      // Supported sample rates
    std::vector<int> bit_depths;        // Supported bit depths
    std::vector<int> channel_counts;    // Supported channel configurations
    bool is_default;
    bool is_exclusive;                  // Supports exclusive mode

    DeviceInfo()
        : index(0)
        , is_default(false)
        , is_exclusive(false) {}
};

/**
 * @brief Convert DeviceInfo to JSON
 */
inline std::string deviceToJSON(const DeviceInfo& device) {
    std::string json = "{\n";
    json += "  \"device\": {\n";
    json += "    \"name\": \"" + device.name + "\",\n";
    json += "    \"id\": \"" + device.id + "\",\n";
    json += "    \"index\": " + std::to_string(device.index) + ",\n";
    json += "    \"is_default\": " + std::string(device.is_default ? "true" : "false") + ",\n";
    json += "    \"is_exclusive\": " + std::string(device.is_exclusive ? "true" : "false") + ",\n";

    json += "    \"sample_rates\": [";
    for (size_t i = 0; i < device.sample_rates.size(); ++i) {
        json += std::to_string(device.sample_rates[i]);
        if (i < device.sample_rates.size() - 1) json += ", ";
    }
    json += "],\n";

    json += "    \"bit_depths\": [";
    for (size_t i = 0; i < device.bit_depths.size(); ++i) {
        json += std::to_string(device.bit_depths[i]);
        if (i < device.bit_depths.size() - 1) json += ", ";
    }
    json += "],\n";

    json += "    \"channel_counts\": [";
    for (size_t i = 0; i < device.channel_counts.size(); ++i) {
        json += std::to_string(device.channel_counts[i]);
        if (i < device.channel_counts.size() - 1) json += ", ";
    }
    json += "]\n";

    json += "  }\n";
    json += "}\n";
    return json;
}

} // namespace protocol
} // namespace xpu

#endif // XPU_PROTOCOL_H
