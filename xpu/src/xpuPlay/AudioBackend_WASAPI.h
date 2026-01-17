/**
 * @file AudioBackend_WASAPI.h
 * @brief Windows WASAPI audio backend
 */

#ifndef XPU_PLAY_AUDIO_BACKEND_WASAPI_H
#define XPU_PLAY_AUDIO_BACKEND_WASAPI_H

#include "AudioBackend.h"
#include <memory>

namespace xpu {
namespace playback {

/**
 * @brief Windows WASAPI audio backend implementation
 */
class AudioBackendWASAPI : public AudioBackend {
public:
    AudioBackendWASAPI();
    virtual ~AudioBackendWASAPI();

    // AudioBackend interface
    virtual ErrorCode initialize() override;
    virtual std::vector<AudioDevice> getDevices() override;
    virtual ErrorCode setDevice(const AudioDevice& device) override;
    virtual ErrorCode configure(int sample_rate, int channels, int buffer_size) override;
    virtual ErrorCode start() override;
    virtual ErrorCode stop() override;
    virtual ErrorCode pause() override;
    virtual ErrorCode resume() override;
    virtual ErrorCode write(const float* data, int frames) override;
    virtual BufferStatus getBufferStatus() const override;
    virtual PlaybackState getState() const override;
    virtual void setStatusCallback(StatusCallback callback) override;
    virtual double getLatencyMs() const override;
    virtual bool isAvailable() const override { return true; }

    // Exclusive mode control
    void setExclusiveMode(bool exclusive);
    bool isExclusiveMode() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    ErrorCode initializeAudioClient();
    ErrorCode initializeAudioClientExclusive();
    ErrorCode initializeAudioClientShared();
    void statusThreadFunc();
};

} // namespace playback
} // namespace xpu

#endif // XPU_PLAY_AUDIO_BACKEND_WASAPI_H
