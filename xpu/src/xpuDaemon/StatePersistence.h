/**
 * @file StatePersistence.h
 * @brief State persistence for playback and configuration
 */

#ifndef XPU_DAEMON_STATE_PERSISTENCE_H
#define XPU_DAEMON_STATE_PERSISTENCE_H

#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include <string>
#include <vector>
#include <memory>

namespace xpu {
namespace daemon {

/**
 * @brief Playback state
 */
struct PlaybackState {
    std::string current_track;
    double position;        // Position in seconds
    bool is_playing;
    std::string playback_mode;  // "sequential", "random", "loop_single", "loop_all"
    float volume;
    std::string eq_preset;
    float eq_bass;
    float eq_mid;
    float eq_treble;

    PlaybackState()
        : position(0.0)
        , is_playing(false)
        , playback_mode("sequential")
        , volume(1.0f)
        , eq_preset("flat")
        , eq_bass(0.0f)
        , eq_mid(0.0f)
        , eq_treble(0.0f) {}
};

/**
 * @brief Queue state
 */
struct QueueState {
    int current_index;
    std::vector<std::string> track_list;

    QueueState()
        : current_index(0) {}
};

/**
 * @brief Complete application state
 */
struct AppState {
    PlaybackState playback;
    QueueState queue;
    std::string version;  // State format version

    AppState()
        : version("1.0") {}
};

/**
 * @brief State persistence manager
 */
class StatePersistence {
public:
    StatePersistence();
    ~StatePersistence() = default;

    /**
     * @brief Initialize state persistence
     */
    ErrorCode initialize(const std::string& state_file);

    /**
     * @brief Save state to disk
     */
    ErrorCode saveState(const AppState& state);

    /**
     * @brief Load state from disk
     */
    ErrorCode loadState(AppState& state);

    /**
     * @brief Update playback state
     */
    ErrorCode updatePlaybackState(const PlaybackState& playback);

    /**
     * @brief Update queue state
     */
    ErrorCode updateQueueState(const QueueState& queue);

    /**
     * @brief Get state file path
     */
    std::string getStateFilePath() const;

    /**
     * @brief Create backup
     */
    ErrorCode createBackup();

private:
    std::string state_file_;
    std::string backup_file_;

    /**
     * @brief Validate state
     */
    ErrorCode validateState(const AppState& state) const;

    /**
     * @brief Migrate state from older version
     */
    ErrorCode migrateState(AppState& state, const std::string& from_version) const;

    /**
     * @brief Write state to file with atomic operation
     */
    ErrorCode writeStateToFile(const std::string& file_path, const AppState& state);

    /**
     * @brief Read state from file
     */
    ErrorCode readStateFromFile(const std::string& file_path, AppState& state);
};

} // namespace daemon
} // namespace xpu

#endif // XPU_DAEMON_STATE_PERSISTENCE_H
