/**
 * @file AudioBackend.h
 * @brief Cross-platform audio backend abstraction
 *
 * Supports:
 * - Windows: WASAPI (Exclusive mode for <50ms latency)
 * - macOS: CoreAudio (HAL for low-latency)
 * - Linux: ALSA (dmix for low-latency)
 */

#ifndef XPU_PLAY_AUDIO_BACKEND_H
#define XPU_PLAY_AUDIO_BACKEND_H

#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace xpu {
namespace playback {

/**
 * @brief Audio device information
 */
struct AudioDevice {
    std::string id;
    std::string name;
    std::string api;  // "wasapi", "coreaudio", "alsa"
    int sample_rate;
    int channels;
    int buffer_size;  // Preferred buffer size
    bool is_default;

    AudioDevice()
        : sample_rate(48000)
        , channels(2)
        , buffer_size(2048)
        , is_default(false) {}
};

/**
 * @brief Playback state
 */
enum class PlaybackState {
    Stopped,
    Playing,
    Paused,
    Error
};

/**
 * @brief Buffer status
 */
struct BufferStatus {
    int fill_level;      // Percentage (0-100)
    int underruns;       // Number of buffer underruns
    int samples_played;  // Total samples played
    double latency_ms;   // Current latency in milliseconds

    BufferStatus()
        : fill_level(0)
        , underruns(0)
        , samples_played(0)
        , latency_ms(0.0) {}
};

/**
 * @brief Real-time status callback (10Hz)
 * Called periodically with playback status
 */
using StatusCallback = std::function<void(const protocol::PlaybackStatus&)>;

/**
 * @brief Cross-platform audio backend
 *
 * Performance targets:
 * - Latency: <50ms (default buffer: 2048 samples at 48kHz = ~42ms)
 * - Underruns: 0 during normal playback
 * - CPU: <5% for audio playback
 */
class AudioBackend {
public:
    AudioBackend();
    virtual ~AudioBackend();

    /**
     * @brief Initialize audio backend
     */
    virtual ErrorCode initialize() = 0;

    /**
     * @brief Get available devices
     */
    virtual std::vector<AudioDevice> getDevices() = 0;

    /**
     * @brief Set output device
     */
    virtual ErrorCode setDevice(const AudioDevice& device) = 0;

    /**
     * @brief Configure audio format
     */
    virtual ErrorCode configure(int sample_rate, int channels, int buffer_size) = 0;

    /**
     * @brief Start playback
     */
    virtual ErrorCode start() = 0;

    /**
     * @brief Stop playback
     */
    virtual ErrorCode stop() = 0;

    /**
     * @brief Pause playback
     */
    virtual ErrorCode pause() = 0;

    /**
     * @brief Resume playback
     */
    virtual ErrorCode resume() = 0;

    /**
     * @brief Write audio data
     * @param data Interleaved float samples [-1.0, 1.0]
     * @param frames Number of frames (frames = samples / channels)
     */
    virtual ErrorCode write(const float* data, int frames) = 0;

    /**
     * @brief Get current buffer status
     */
    virtual BufferStatus getBufferStatus() const = 0;

    /**
     * @brief Get current playback state
     */
    virtual PlaybackState getState() const = 0;

    /**
     * @brief Set status callback (10Hz push to stdout)
     */
    virtual void setStatusCallback(StatusCallback callback) = 0;

    /**
     * @brief Get latency in milliseconds
     */
    virtual double getLatencyMs() const = 0;

    /**
     * @brief Check if backend is available
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Set exclusive mode (WASAPI only)
     * @param exclusive True for exclusive mode, false for shared mode
     * @note Default is shared mode for faster initialization
     * @note Exclusive mode provides lower latency but slower initialization (~350ms delay)
     */
    virtual void setExclusiveMode(bool exclusive) { (void)exclusive; }

    /**
     * @brief Check if exclusive mode is enabled
     * @return True if exclusive mode is enabled, false otherwise
     */
    virtual bool isExclusiveMode() const { return false; }

    /**
     * @brief Create backend for current platform
     */
    static std::unique_ptr<AudioBackend> create();
};

} // namespace playback
} // namespace xpu

#endif // XPU_PLAY_AUDIO_BACKEND_H
