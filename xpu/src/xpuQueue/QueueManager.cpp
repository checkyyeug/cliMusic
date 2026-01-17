/**
 * @file QueueManager.cpp
 * @brief Queue manager implementation
 */

#ifdef PLATFORM_WINDOWS
#ifdef NOMINMAX
#undef NOMINMAX
#endif
#define NOMINMAX
#endif

#include "QueueManager.h"
#include "utils/Logger.h"
#include <fstream>
#include <algorithm>
#include <random>
#include <cstring>

extern "C" {
#include <sys/stat.h>
}

using namespace xpu;

namespace xpu {
namespace queue {

/**
 * @brief Queue file format version
 */
static const std::string QUEUE_VERSION = "1.0";

QueueManager::QueueManager() {
    queue_file_ = utils::PlatformUtils::getConfigDirectory() + "/queue.json";
}

QueueManager::~QueueManager() {
    // Auto-save on exit
    saveQueue();
}

ErrorCode QueueManager::initialize() {
    LOG_INFO("Initializing queue manager");

    // Load queue from disk
    ErrorCode ret = loadQueue();
    if (ret != ErrorCode::Success && ret != ErrorCode::FileNotFound) {
        LOG_WARNING("Failed to load queue: {}", static_cast<int>(ret));
    }

    LOG_INFO("Queue manager initialized with {} tracks", state_.entries.size());
    return ErrorCode::Success;
}

ErrorCode QueueManager::addTrack(const std::string& file_path,
                                  const protocol::AudioMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);

    QueueEntry entry;
    entry.file_path = file_path;
    entry.metadata = metadata;
    entry.position = state_.entries.size();
    entry.is_playing = false;

    state_.entries.push_back(entry);

    LOG_INFO("Added track to queue: {} (position {})", file_path, entry.position);

    // Auto-save
    return saveQueue();
}

ErrorCode QueueManager::addTrack(const std::string& file_path) {
    // Create empty metadata - will be filled when track is loaded
    protocol::AudioMetadata metadata;
    return addTrack(file_path, metadata);
}

ErrorCode QueueManager::removeTrack(int index) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (index < 0 || index >= static_cast<int>(state_.entries.size())) {
        return ErrorCode::InvalidOperation;
    }

    std::string removed_path = state_.entries[index].file_path;
    state_.entries.erase(state_.entries.begin() + index);

    // Adjust current index
    if (state_.current_index >= index) {
        state_.current_index = std::max(0, state_.current_index - 1);
    }

    updatePositions();

    LOG_INFO("Removed track from queue: {}", removed_path);

    // Auto-save
    return saveQueue();
}

ErrorCode QueueManager::clearQueue() {
    std::lock_guard<std::mutex> lock(mutex_);

    state_.entries.clear();
    state_.current_index = 0;

    LOG_INFO("Queue cleared");

    // Auto-save
    return saveQueue();
}

QueueState QueueManager::getQueueState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

int QueueManager::getQueueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(state_.entries.size());
}

ErrorCode QueueManager::getCurrentTrack(QueueEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_.entries.empty()) {
        return ErrorCode::InvalidOperation;
    }

    if (state_.current_index < 0 ||
        state_.current_index >= static_cast<int>(state_.entries.size())) {
        return ErrorCode::InvalidOperation;
    }

    entry = state_.entries[state_.current_index];
    return ErrorCode::Success;
}

ErrorCode QueueManager::getNextTrack(QueueEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_.entries.empty()) {
        return ErrorCode::InvalidOperation;
    }

    switch (state_.mode) {
        case PlaybackMode::Sequential:
        case PlaybackMode::LoopAll:
            state_.current_index++;
            if (state_.current_index >= static_cast<int>(state_.entries.size())) {
                if (state_.mode == PlaybackMode::LoopAll) {
                    state_.current_index = 0;
                } else {
                    state_.current_index = state_.entries.size() - 1;
                    return ErrorCode::EndOfQueue;
                }
            }
            break;

        case PlaybackMode::Random:
            state_.current_index = getRandomIndex();
            break;

        case PlaybackMode::LoopSingle:
            // Stay on current track
            break;
    }

    entry = state_.entries[state_.current_index];
    return ErrorCode::Success;
}

ErrorCode QueueManager::getPreviousTrack(QueueEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_.entries.empty()) {
        return ErrorCode::InvalidOperation;
    }

    state_.current_index--;
    if (state_.current_index < 0) {
        state_.current_index = 0;
        return ErrorCode::EndOfQueue;
    }

    entry = state_.entries[state_.current_index];
    return ErrorCode::Success;
}

std::string QueueManager::getNextTrack() {
    QueueEntry entry;
    if (getNextTrack(entry) == ErrorCode::Success) {
        return entry.file_path;
    }
    return "";
}

std::string QueueManager::getPreviousTrack() {
    QueueEntry entry;
    if (getPreviousTrack(entry) == ErrorCode::Success) {
        return entry.file_path;
    }
    return "";
}

ErrorCode QueueManager::jumpToIndex(int index) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (index < 0 || index >= static_cast<int>(state_.entries.size())) {
        return ErrorCode::InvalidOperation;
    }

    state_.current_index = index;
    LOG_INFO("Jumped to queue index: {}", index);

    return ErrorCode::Success;
}

ErrorCode QueueManager::setPlaybackMode(PlaybackMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    state_.mode = mode;

    const char* mode_names[] = {"Sequential", "Random", "LoopSingle", "LoopAll"};
    LOG_INFO("Playback mode set to: {}", mode_names[static_cast<int>(mode)]);

    return ErrorCode::Success;
}

PlaybackMode QueueManager::getPlaybackMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.mode;
}

ErrorCode QueueManager::shuffleQueue() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_.entries.size() < 2) {
        return ErrorCode::Success;
    }

    // Shuffle all entries except current
    int current_pos = state_.current_index;
    QueueEntry current_entry = state_.entries[current_pos];

    // Remove current entry
    state_.entries.erase(state_.entries.begin() + current_pos);

    // Shuffle the rest
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(state_.entries.begin(), state_.entries.end(), g);

    // Insert current entry at the beginning
    state_.entries.insert(state_.entries.begin(), current_entry);

    // Reset to beginning
    state_.current_index = 0;
    updatePositions();

    LOG_INFO("Queue shuffled");

    return saveQueue();
}

ErrorCode QueueManager::saveQueue() {
    // Create JSON manually (no nlohmann/json dependency in header)
    std::string temp_file = queue_file_ + ".tmp";
    std::ofstream out(temp_file);

    if (!out.is_open()) {
        LOG_ERROR("Failed to open queue file for writing: {}", queue_file_);
        return ErrorCode::FileWriteError;
    }

    out << "{\n";
    out << "  \"version\": \"" << QUEUE_VERSION << "\",\n";
    out << "  \"current_index\": " << state_.current_index << ",\n";
    out << "  \"playback_mode\": " << static_cast<int>(state_.mode) << ",\n";
    out << "  \"entries\": [\n";

    for (size_t i = 0; i < state_.entries.size(); ++i) {
        const QueueEntry& entry = state_.entries[i];

        out << "    {\n";
        out << "      \"file_path\": \"" << entry.file_path << "\",\n";
        out << "      \"position\": " << entry.position << ",\n";
        out << "      \"metadata\": {\n";
        out << "        \"title\": \"" << entry.metadata.title << "\",\n";
        out << "        \"artist\": \"" << entry.metadata.artist << "\",\n";
        out << "        \"album\": \"" << entry.metadata.album << "\",\n";
        out << "        \"duration\": " << entry.metadata.duration << ",\n";
        out << "        \"sample_rate\": " << entry.metadata.sample_rate << "\n";
        out << "      }\n";

        if (i < state_.entries.size() - 1) {
            out << "    },\n";
        } else {
            out << "    }\n";
        }
    }

    out << "  ]\n";
    out << "}\n";

    out.close();

    // Atomic rename
    std::rename(temp_file.c_str(), queue_file_.c_str());

    LOG_DEBUG("Queue saved to: {}", queue_file_);
    return ErrorCode::Success;
}

ErrorCode QueueManager::loadQueue() {
    std::ifstream in(queue_file_);

    if (!in.is_open()) {
        return ErrorCode::FileNotFound;
    }

    // Simple JSON parsing (for production, use proper JSON library)
    std::string line;
    bool in_entries = false;
    QueueEntry current_entry;
    int entry_count = 0;

    while (std::getline(in, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.find("\"current_index\":") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            std::string value = line.substr(pos);
            state_.current_index = std::stoi(value);
        } else if (line.find("\"playback_mode\":") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            std::string value = line.substr(pos);
            state_.mode = static_cast<PlaybackMode>(std::stoi(value));
        } else if (line.find("\"entries\"") != std::string::npos) {
            in_entries = true;
        }
    }

    in.close();

    LOG_INFO("Queue loaded from: {} ({} entries)", queue_file_, state_.entries.size());
    return ErrorCode::Success;
}

std::string QueueManager::getQueueFilePath() const {
    return queue_file_;
}

void QueueManager::updatePositions() {
    for (size_t i = 0; i < state_.entries.size(); ++i) {
        state_.entries[i].position = i;
    }
}

int QueueManager::getRandomIndex() {
    if (state_.entries.size() <= 1) {
        return 0;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, state_.entries.size() - 1);

    int new_index;
    do {
        new_index = dis(gen);
    } while (new_index == state_.current_index);

    return new_index;
}

} // namespace queue
} // namespace xpu
