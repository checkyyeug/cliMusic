/**
 * @file AudioBackend_ALSA.cpp
 * @brief Linux ALSA audio backend implementation
 *
 * Features:
 * - dmix plugin for low-latency shared access
 * - Non-blocking mode for efficient playback
 * - Automatic device detection
 */

#ifdef PLATFORM_LINUX

#include "AudioBackend.h"
#include "utils/Logger.h"
#include <alsa/asoundlib.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <poll.h>

using namespace xpu;

namespace xpu {
namespace playback {

/**
 * @brief ALSA implementation
 */
class AudioBackendALSA : public AudioBackend {
public:
    AudioBackendALSA();
    ~AudioBackendALSA() override;

    // AudioBackend interface
    ErrorCode initialize() override;
    std::vector<AudioDevice> getDevices() override;
    ErrorCode setDevice(const AudioDevice& device) override;
    ErrorCode configure(int sample_rate, int channels, int buffer_size) override;
    ErrorCode start() override;
    ErrorCode stop() override;
    ErrorCode pause() override;
    ErrorCode resume() override;
    ErrorCode write(const float* data, int frames) override;
    BufferStatus getBufferStatus() const override;
    PlaybackState getState() const override;
    void setStatusCallback(StatusCallback callback) override;
    double getLatencyMs() const override;
    bool isAvailable() const override { return true; }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    void statusThreadFunc();
    ErrorCode openDevice(const std::string& device_name);
};

/**
 * @brief Implementation class
 */
class AudioBackendALSA::Impl {
public:
    snd_pcm_t* pcm_handle;
    std::string device_name;

    AudioDevice current_device;

    int configured_sample_rate;
    int configured_channels;
    int configured_buffer_size;
    snd_pcm_format_t pcm_format;
    PlaybackState state;
    BufferStatus buffer_status;

    StatusCallback status_callback;
    std::thread status_thread;
    bool status_thread_running;
    struct pollfd poll_fds[1];
    int num_poll_fds;

    Impl()
        : pcm_handle(nullptr)
        , device_name("default")
        , configured_sample_rate(48000)
        , configured_channels(2)
        , configured_buffer_size(2048)
        , pcm_format(SND_PCM_FORMAT_FLOAT_LE)
        , state(PlaybackState::Stopped)
        , status_thread_running(false)
        , num_poll_fds(0) {

        current_device.id = "default";
        current_device.name = "Default ALSA Device";
        current_device.api = "alsa";
        current_device.is_default = true;
    }

    ~Impl() {
        if (status_thread_running) {
            status_thread_running = false;
            if (status_thread.joinable()) {
                status_thread.join();
            }
        }

        if (pcm_handle) {
            snd_pcm_drain(pcm_handle);
            snd_pcm_close(pcm_handle);
        }
    }
};

AudioBackendALSA::AudioBackendALSA()
    : impl_(std::make_unique<Impl>()) {}

AudioBackendALSA::~AudioBackendALSA() = default;

ErrorCode AudioBackendALSA::initialize() {
    LOG_INFO("Initializing ALSA backend");

    // List devices to find default
    void** hints;
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err == 0) {
        // Successfully got device hints
        snd_device_name_free_hint(hints);
    }

    LOG_INFO("ALSA backend initialized: {}", impl_->current_device.name);
    return ErrorCode::Success;
}

std::vector<AudioDevice> AudioBackendALSA::getDevices() {
    std::vector<AudioDevice> devices;

    void** hints;
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err != 0) {
        LOG_ERROR("Failed to get device hints");
        return devices;
    }

    for (void** hint = hints; *hint != nullptr; ++hint) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* desc = snd_device_name_get_hint(*hint, "DESC");

        if (name && strcmp(name, "null") != 0) {
            AudioDevice device;
            device.id = name;
            device.name = desc ? desc : name;
            device.api = "alsa";

            // Check if default
            if (strcmp(name, "default") == 0) {
                device.is_default = true;
            }

            devices.push_back(device);
        }

        if (name) free(name);
        if (desc) free(desc);
    }

    snd_device_name_free_hint(hints);
    return devices;
}

ErrorCode AudioBackendALSA::setDevice(const AudioDevice& device) {
    impl_->device_name = device.id;
    impl_->current_device = device;

    LOG_INFO("ALSA device set to: {}", device.name);
    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::configure(int sample_rate, int channels, int buffer_size) {
    impl_->configured_sample_rate = sample_rate;
    impl_->configured_channels = channels;
    impl_->configured_buffer_size = buffer_size;

    // Close existing handle
    if (impl_->pcm_handle) {
        snd_pcm_close(impl_->pcm_handle);
        impl_->pcm_handle = nullptr;
    }

    return openDevice(impl_->device_name);
}

ErrorCode AudioBackendALSA::openDevice(const std::string& device_name) {
    // Open PCM device
    int err = snd_pcm_open(&impl_->pcm_handle, device_name.c_str(),
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

    if (err < 0) {
        LOG_ERROR("Failed to open PCM device: {}", snd_strerror(err));
        return ErrorCode::DeviceNotFound;
    }

    // Allocate hardware parameters object
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    // Fill with default values
    snd_pcm_hw_params_any(impl_->pcm_handle, hw_params);

    // Set access type (interleaved)
    snd_pcm_hw_params_set_access(impl_->pcm_handle, hw_params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);

    // Set sample format
    impl_->pcm_format = SND_PCM_FORMAT_FLOAT_LE;
    snd_pcm_hw_params_set_format(impl_->pcm_handle, hw_params, impl_->pcm_format);

    // Set channel count
    snd_pcm_hw_params_set_channels(impl_->pcm_handle, hw_params,
                                    impl_->configured_channels);

    // Set sample rate
    unsigned int rate = impl_->configured_sample_rate;
    snd_pcm_hw_params_set_rate_near(impl_->pcm_handle, hw_params, &rate, 0);
    if (rate != static_cast<unsigned int>(impl_->configured_sample_rate)) {
        LOG_WARNING("Sample rate adjusted: {} -> {}", impl_->configured_sample_rate, rate);
        impl_->configured_sample_rate = rate;
    }

    // Set buffer size (aim for <50ms latency)
    snd_pcm_hw_params_set_buffer_size_near(impl_->pcm_handle, hw_params,
                                           reinterpret_cast<snd_pcm_uframes_t*>(
                                               &impl_->configured_buffer_size));

    // Apply hardware parameters
    err = snd_pcm_hw_params(impl_->pcm_handle, hw_params);
    if (err < 0) {
        LOG_ERROR("Failed to set HW params: {}", snd_strerror(err));
        return ErrorCode::AudioBackendError;
    }

    // Get poll descriptors
    impl_->num_poll_fds = snd_pcm_poll_descriptors_count(impl_->pcm_handle);
    if (impl_->num_poll_fds > 0) {
        snd_pcm_poll_descriptors(impl_->pcm_handle, impl_->poll_fds,
                                 impl_->num_poll_fds);
    }

    // Calculate latency
    snd_pcm_hw_params_current(impl_->pcm_handle, hw_params);
    snd_pcm_uframes_t buffer_size;
    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);
    double latency_ms = (buffer_size * 1000.0) / impl_->configured_sample_rate;

    LOG_INFO("ALSA configured: {} Hz, {} channels, {} frames buffer ({:.2f} ms latency)",
             impl_->configured_sample_rate, impl_->configured_channels,
             buffer_size, latency_ms);

    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::start() {
    if (!impl_->pcm_handle) {
        return ErrorCode::InvalidState;
    }

    // Prepare PCM
    int err = snd_pcm_prepare(impl_->pcm_handle);
    if (err < 0) {
        LOG_ERROR("Failed to prepare PCM: {}", snd_strerror(err));
        return ErrorCode::AudioBackendError;
    }

    impl_->state = PlaybackState::Playing;

    // Start status thread
    if (!impl_->status_thread_running) {
        impl_->status_thread_running = true;
        impl_->status_thread = std::thread([this]() { statusThreadFunc(); });
    }

    LOG_INFO("ALSA playback started");
    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::stop() {
    if (!impl_->pcm_handle) {
        return ErrorCode::InvalidState;
    }

    snd_pcm_drain(impl_->pcm_handle);
    impl_->state = PlaybackState::Stopped;

    // Stop status thread
    impl_->status_thread_running = false;
    if (impl_->status_thread.joinable()) {
        impl_->status_thread.join();
    }

    LOG_INFO("ALSA playback stopped");
    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::pause() {
    if (!impl_->pcm_handle) {
        return ErrorCode::InvalidState;
    }

    // Drop audio in progress
    snd_pcm_drop(impl_->pcm_handle);
    impl_->state = PlaybackState::Paused;

    LOG_INFO("ALSA playback paused");
    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::resume() {
    if (!impl_->pcm_handle) {
        return ErrorCode::InvalidState;
    }

    // Prepare PCM again
    int err = snd_pcm_prepare(impl_->pcm_handle);
    if (err < 0) {
        LOG_ERROR("Failed to prepare PCM: {}", snd_strerror(err));
        return ErrorCode::AudioBackendError;
    }

    impl_->state = PlaybackState::Playing;

    LOG_INFO("ALSA playback resumed");
    return ErrorCode::Success;
}

ErrorCode AudioBackendALSA::write(const float* data, int frames) {
    if (!impl_->pcm_handle || impl_->state != PlaybackState::Playing) {
        return ErrorCode::InvalidState;
    }

    // Write frames
    snd_pcm_sframes_t frames_written = snd_pcm_writei(
        impl_->pcm_handle, data, frames);

    if (frames_written < 0) {
        if (frames_written == -EPIPE) {
            // Buffer underrun
            LOG_WARNING("ALSA buffer underrun");
            impl_->buffer_status.underruns++;

            // Recover
            snd_pcm_recover(impl_->pcm_handle, frames_written, 0);

            return ErrorCode::BufferUnderrun;
        } else {
            LOG_ERROR("ALSA write error: {}", snd_strerror(frames_written));
            return ErrorCode::AudioBackendError;
        }
    }

    impl_->buffer_status.samples_played += frames_written;

    return ErrorCode::Success;
}

BufferStatus AudioBackendALSA::getBufferStatus() const {
    return impl_->buffer_status;
}

PlaybackState AudioBackendALSA::getState() const {
    return impl_->state;
}

void AudioBackendALSA::setStatusCallback(StatusCallback callback) {
    impl_->status_callback = callback;
}

double AudioBackendALSA::getLatencyMs() const {
    if (!impl_->pcm_handle) {
        return 0.0;
    }

    snd_pcm_sframes_t delay;
    int err = snd_pcm_delay(impl_->pcm_handle, &delay);
    if (err < 0) {
        return 0.0;
    }

    return (delay * 1000.0) / impl_->configured_sample_rate;
}

void AudioBackendALSA::statusThreadFunc() {
    while (impl_->status_thread_running) {
        // Get delay
        snd_pcm_sframes_t delay = 0;
        if (impl_->pcm_handle) {
            snd_pcm_delay(impl_->pcm_handle, &delay);
        }

        // Get buffer state
        snd_pcm_hw_params_t* hw_params;
        snd_pcm_hw_params_alloca(&hw_params);
        snd_pcm_hw_params_current(impl_->pcm_handle, hw_params);

        snd_pcm_uframes_t buffer_size;
        snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);

        impl_->buffer_status.fill_level =
            100 - ((delay * 100) / buffer_size);
        impl_->buffer_status.latency_ms =
            (delay * 1000.0) / impl_->configured_sample_rate;

        // Call status callback
        if (impl_->status_callback) {
            protocol::PlaybackStatus status;
            status.state = static_cast<int>(impl_->state);
            status.position = impl_->buffer_status.samples_played;
            status.buffer_fill = impl_->buffer_status.fill_level;
            status.latency_ms = impl_->buffer_status.latency_ms;

            impl_->status_callback(status);
        }

        // Sleep for 100ms (10Hz)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace playback
} // namespace xpu

#endif // PLATFORM_LINUX
