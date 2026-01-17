/**
 * @file QueueManager.h
 * @brief Queue management with persistence
 */

#ifndef XPU_QUEUE_QUEUE_MANAGER_H
#define XPU_QUEUE_QUEUE_MANAGER_H

#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include "utils/PlatformUtils.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace xpu {
namespace queue {

/**
 * @brief Queue entry
 */
struct QueueEntry {
    std::string file_path;
    protocol::AudioMetadata metadata;
    int position;        // Position in queue
    bool is_playing;     // Currently playing

    QueueEntry()
        : position(0)
        , is_playing(false) {}
};

/**
 * @brief Playback mode
 */
enum class PlaybackMode {
    Sequential,    // Play in order
    Random,        // Shuffle
    LoopSingle,    // Loop single track
    LoopAll        // Loop entire queue
};

/**
 * @brief Queue state
 */
struct QueueState {
    int current_index;
    PlaybackMode mode;
    std::vector<QueueEntry> entries;

    QueueState()
        : current_index(0)
        , mode(PlaybackMode::Sequential) {}
};

/**
 * @brief Queue manager with persistence
 */
class QueueManager {
public:
    QueueManager();
    ~QueueManager();

    /**
     * @brief Initialize queue manager
     */
    ErrorCode initialize();

    /**
     * @brief Add track to queue (with metadata)
     */
    ErrorCode addTrack(const std::string& file_path, const protocol::AudioMetadata& metadata);

    /**
     * @brief Add track to queue (file path only, metadata will be loaded)
     */
    ErrorCode addTrack(const std::string& file_path);

    /**
     * @brief Remove track from queue
     */
    ErrorCode removeTrack(int index);

    /**
     * @brief Clear queue
     */
    ErrorCode clearQueue();

    /**
     * @brief Get queue state
     */
    QueueState getQueueState() const;

    /**
     * @brief Get queue size
     */
    int getQueueSize() const;

    /**
     * @brief Get current track
     */
    ErrorCode getCurrentTrack(QueueEntry& entry) const;

    /**
     * @brief Get next track
     */
    ErrorCode getNextTrack(QueueEntry& entry);

    /**
     * @brief Get next track file path
     */
    std::string getNextTrack();

    /**
     * @brief Get previous track
     */
    ErrorCode getPreviousTrack(QueueEntry& entry);

    /**
     * @brief Get previous track file path
     */
    std::string getPreviousTrack();

    /**
     * @brief Jump to index
     */
    ErrorCode jumpToIndex(int index);

    /**
     * @brief Jump to track (alias for jumpToIndex)
     */
    ErrorCode jumpToTrack(int index) { return jumpToIndex(index); }

    /**
     * @brief Set playback mode
     */
    ErrorCode setPlaybackMode(PlaybackMode mode);

    /**
     * @brief Get playback mode
     */
    PlaybackMode getPlaybackMode() const;

    /**
     * @brief Shuffle queue
     */
    ErrorCode shuffleQueue();

    /**
     * @brief Save queue to disk
     */
    ErrorCode saveQueue();

    /**
     * @brief Load queue from disk
     */
    ErrorCode loadQueue();

    /**
     * @brief Get queue file path
     */
    std::string getQueueFilePath() const;

private:
    mutable std::mutex mutex_;
    QueueState state_;
    std::string queue_file_;

    /**
     * @brief Update positions
     */
    void updatePositions();

    /**
     * @brief Get random index (excluding current)
     */
    int getRandomIndex();
};

} // namespace queue
} // namespace xpu

#endif // XPU_QUEUE_QUEUE_MANAGER_H
