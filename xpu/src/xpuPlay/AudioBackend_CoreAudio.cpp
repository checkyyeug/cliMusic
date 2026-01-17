/**
 * @file AudioBackend_CoreAudio.cpp
 * @brief macOS CoreAudio audio backend implementation
 *
 * Features:
 * - HAL (Hardware Abstraction Layer) for low-latency
 * - Callback-based for efficient audio delivery
 * - Automatic device management
 */

#ifdef PLATFORM_MACOS

#include "AudioBackend.h"
#include "utils/Logger.h"
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <cstring>
#include <thread>
#include <chrono>

using namespace xpu;

namespace xpu {
namespace playback {

/**
 * @brief CoreAudio implementation
 */
class AudioBackendCoreAudio : public AudioBackend {
public:
    AudioBackendCoreAudio();
    ~AudioBackendCoreAudio() override;

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

    static OSStatus audioCallback(void* ref_con,
                                  AudioUnitRenderActionFlags* action_flags,
                                  const AudioTimeStamp* time_stamp,
                                  UInt32 bus_number,
                                  UInt32 number_frames,
                                  AudioBufferList* io_data);

    void statusThreadFunc();
};

/**
 * @brief Implementation class
 */
class AudioBackendCoreAudio::Impl {
public:
    AudioComponentInstance audio_unit;
    AudioStreamBasicDescription format;

    AudioDeviceID current_device_id;
    AudioDevice current_device;

    int configured_sample_rate;
    int configured_channels;
    int configured_buffer_size;
    PlaybackState state;
    BufferStatus buffer_status;

    std::vector<float> audio_buffer;
    size_t buffer_read_pos;
    size_t buffer_write_pos;
    std::mutex buffer_mutex;

    StatusCallback status_callback;
    std::thread status_thread;
    bool status_thread_running;

    Impl()
        : audio_unit(nullptr)
        , current_device_id(kAudioObjectUnknown)
        , configured_sample_rate(48000)
        , configured_channels(2)
        , configured_buffer_size(2048)
        , state(PlaybackState::Stopped)
        , buffer_read_pos(0)
        , buffer_write_pos(0)
        , status_thread_running(false) {

        // Setup format
        format.mSampleRate = 48000.0;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kLinearPCMFormatFlagIsFloat |
                             kLinearPCMFormatFlagIsPacked;
        format.mBitsPerChannel = 32;
        format.mChannelsPerFrame = 2;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = format.mBitsPerChannel / 8 * format.mChannelsPerFrame;
        format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;

        // Pre-allocate buffer (5 seconds)
        audio_buffer.resize(configured_sample_rate * configured_channels * 5);
    }

    ~Impl() {
        if (status_thread_running) {
            status_thread_running = false;
            if (status_thread.joinable()) {
                status_thread.join();
            }
        }

        if (audio_unit) {
            AudioComponentInstanceDispose(audio_unit);
        }
    }
};

AudioBackendCoreAudio::AudioBackendCoreAudio()
    : impl_(std::make_unique<Impl>()) {}

AudioBackendCoreAudio::~AudioBackendCoreAudio() = default;

ErrorCode AudioBackendCoreAudio::initialize() {
    LOG_INFO("Initializing CoreAudio backend");

    // Get default output device
    AudioObjectPropertyAddress prop_addr;
    prop_addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    prop_addr.mScope = kAudioObjectPropertyScopeGlobal;
    prop_addr.mElement = kAudioObjectPropertyElementMain;

    UInt32 size = sizeof(impl_->current_device_id);
    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &prop_addr, 0, nullptr, &size, &impl_->current_device_id);

    if (status != noErr) {
        LOG_ERROR("Failed to get default output device");
        return ErrorCode::DeviceNotFound;
    }

    // Get device name
    prop_addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
    prop_addr.mScope = kAudioDevicePropertyScopeOutput;
    size = sizeof(CFStringRef);
    CFStringRef device_name = nullptr;
    status = AudioObjectGetPropertyData(
        impl_->current_device_id, &prop_addr, 0, nullptr, &size, &device_name);

    if (status == noErr && device_name) {
        char name_buffer[256];
        CFStringGetCString(device_name, name_buffer, sizeof(name_buffer), kCFStringEncodingUTF8);
        impl_->current_device.name = name_buffer;
        CFRelease(device_name);
    }

    impl_->current_device.id = "default";
    impl_->current_device.api = "coreaudio";
    impl_->current_device.is_default = true;

    LOG_INFO("CoreAudio backend initialized: {}", impl_->current_device.name);
    return ErrorCode::Success;
}

std::vector<AudioDevice> AudioBackendCoreAudio::getDevices() {
    std::vector<AudioDevice> devices;

    AudioObjectPropertyAddress prop_addr;
    prop_addr.mSelector = kAudioHardwarePropertyDevices;
    prop_addr.mScope = kAudioObjectPropertyScopeGlobal;
    prop_addr.mElement = kAudioObjectPropertyElementMain;

    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &prop_addr, 0, nullptr, &size);

    if (status != noErr) {
        LOG_ERROR("Failed to get device list size");
        return devices;
    }

    int device_count = size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> device_ids(device_count);

    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &prop_addr, 0, nullptr, &size, device_ids.data());

    if (status != noErr) {
        LOG_ERROR("Failed to get device list");
        return devices;
    }

    for (AudioDeviceID device_id : device_ids) {
        AudioDevice device;
        device.id = "device_" + std::to_string(device_id);
        device.api = "coreaudio";

        // Get device name
        prop_addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
        prop_addr.mScope = kAudioDevicePropertyScopeOutput;
        size = sizeof(CFStringRef);
        CFStringRef device_name = nullptr;
        status = AudioObjectGetPropertyData(
            device_id, &prop_addr, 0, nullptr, &size, &device_name);

        if (status == noErr && device_name) {
            char name_buffer[256];
            CFStringGetCString(device_name, name_buffer, sizeof(name_buffer), kCFStringEncodingUTF8);
            device.name = name_buffer;
            CFRelease(device_name);
        }

        devices.push_back(device);
    }

    return devices;
}

ErrorCode AudioBackendCoreAudio::setDevice(const AudioDevice& device) {
    // TODO: Implement device switching
    LOG_WARNING("Device switching not fully implemented in CoreAudio");
    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::configure(int sample_rate, int channels, int buffer_size) {
    impl_->configured_sample_rate = sample_rate;
    impl_->configured_channels = channels;
    impl_->configured_buffer_size = buffer_size;

    // Update format
    impl_->format.mSampleRate = sample_rate;
    impl_->format.mChannelsPerFrame = channels;
    impl_->format.mBytesPerFrame = (impl_->format.mBitsPerChannel / 8) * channels;
    impl_->format.mBytesPerPacket = impl_->format.mBytesPerFrame;

    // Create audio unit
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        LOG_ERROR("Failed to find audio component");
        return ErrorCode::AudioBackendError;
    }

    OSStatus status = AudioComponentInstanceNew(component, &impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to create audio unit");
        return ErrorCode::AudioBackendError;
    }

    // Set format
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &impl_->format,
        sizeof(impl_->format));

    if (status != noErr) {
        LOG_ERROR("Failed to set audio unit format");
        return ErrorCode::AudioBackendError;
    }

    // Set buffer size
    UInt32 buffer_frames = buffer_size;
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Global,
        0,
        &buffer_frames,
        sizeof(buffer_frames));

    if (status != noErr) {
        LOG_WARNING("Failed to set buffer size");
    }

    // Set callback
    AURenderCallbackStruct callback;
    callback.inputProc = audioCallback;
    callback.inputProcRefCon = this;

    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Global,
        0,
        &callback,
        sizeof(callback));

    if (status != noErr) {
        LOG_ERROR("Failed to set audio callback");
        return ErrorCode::AudioBackendError;
    }

    // Initialize audio unit
    status = AudioUnitInitialize(impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to initialize audio unit");
        return ErrorCode::AudioBackendError;
    }

    // Calculate latency
    double latency_ms = (buffer_frames * 1000.0) / sample_rate;
    LOG_INFO("CoreAudio configured: {} Hz, {} channels, {} frames buffer ({:.2f} ms latency)",
             sample_rate, channels, buffer_frames, latency_ms);

    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::start() {
    if (!impl_->audio_unit) {
        return ErrorCode::InvalidState;
    }

    OSStatus status = AudioOutputUnitStart(impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to start audio unit");
        return ErrorCode::AudioBackendError;
    }

    impl_->state = PlaybackState::Playing;

    // Start status thread
    if (!impl_->status_thread_running) {
        impl_->status_thread_running = true;
        impl_->status_thread = std::thread([this]() { statusThreadFunc(); });
    }

    LOG_INFO("CoreAudio playback started");
    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::stop() {
    if (!impl_->audio_unit) {
        return ErrorCode::InvalidState;
    }

    AudioOutputUnitStop(impl_->audio_unit);
    impl_->state = PlaybackState::Stopped;

    // Clear buffer
    std::lock_guard<std::mutex> lock(impl_->buffer_mutex);
    impl_->buffer_read_pos = 0;
    impl_->buffer_write_pos = 0;

    // Stop status thread
    impl_->status_thread_running = false;
    if (impl_->status_thread.joinable()) {
        impl_->status_thread.join();
    }

    LOG_INFO("CoreAudio playback stopped");
    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::pause() {
    if (!impl_->audio_unit) {
        return ErrorCode::InvalidState;
    }

    AudioOutputUnitStop(impl_->audio_unit);
    impl_->state = PlaybackState::Paused;

    LOG_INFO("CoreAudio playback paused");
    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::resume() {
    if (!impl_->audio_unit) {
        return ErrorCode::InvalidState;
    }

    OSStatus status = AudioOutputUnitStart(impl_->audio_unit);
    if (status != noErr) {
        LOG_ERROR("Failed to resume audio unit");
        return ErrorCode::AudioBackendError;
    }

    impl_->state = PlaybackState::Playing;

    LOG_INFO("CoreAudio playback resumed");
    return ErrorCode::Success;
}

ErrorCode AudioBackendCoreAudio::write(const float* data, int frames) {
    std::lock_guard<std::mutex> lock(impl_->buffer_mutex);

    size_t samples = frames * impl_->configured_channels;
    size_t available_space = impl_->audio_buffer.size() -
        (impl_->buffer_write_pos - impl_->buffer_read_pos);

    if (samples > available_space) {
        LOG_WARNING("Audio buffer overflow");
        impl_->buffer_status.underruns++;
        return ErrorCode::BufferOverflow;
    }

    // Write to circular buffer
    for (size_t i = 0; i < samples; ++i) {
        impl_->audio_buffer[impl_->buffer_write_pos % impl_->audio_buffer.size()] = data[i];
        impl_->buffer_write_pos++;
    }

    return ErrorCode::Success;
}

BufferStatus AudioBackendCoreAudio::getBufferStatus() const {
    return impl_->buffer_status;
}

PlaybackState AudioBackendCoreAudio::getState() const {
    return impl_->state;
}

void AudioBackendCoreAudio::setStatusCallback(StatusCallback callback) {
    impl_->status_callback = callback;
}

double AudioBackendCoreAudio::getLatencyMs() const {
    return (impl_->configured_buffer_size * 1000.0) / impl_->configured_sample_rate;
}

OSStatus AudioBackendCoreAudio::audioCallback(
    void* ref_con,
    AudioUnitRenderActionFlags* action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_frames,
    AudioBufferList* io_data) {

    AudioBackendCoreAudio* backend = static_cast<AudioBackendCoreAudio*>(ref_con);
    auto* impl = backend->impl_.get();

    // Get output buffer
    float* out_buffer = static_cast<float*>(io_data->mBuffers[0].mData);
    size_t samples_requested = number_frames * impl->configured_channels;

    std::lock_guard<std::mutex> lock(impl->buffer_mutex);

    size_t samples_available = impl->buffer_write_pos - impl->buffer_read_pos;

    if (samples_available >= samples_requested) {
        // Copy from buffer
        for (size_t i = 0; i < samples_requested; ++i) {
            out_buffer[i] = impl->audio_buffer[impl->buffer_read_pos % impl->audio_buffer.size()];
            impl->buffer_read_pos++;
        }
        impl->buffer_status.samples_played += number_frames;
    } else {
        // Buffer underrun - fill with silence
        std::memset(out_buffer, 0, samples_requested * sizeof(float));
        impl->buffer_status.underruns++;
    }

    return noErr;
}

void AudioBackendCoreAudio::statusThreadFunc() {
    while (impl_->status_thread_running) {
        // Calculate buffer fill level
        std::lock_guard<std::mutex> lock(impl_->buffer_mutex);
        size_t buffer_used = impl_->buffer_write_pos - impl_->buffer_read_pos;
        impl_->buffer_status.fill_level = (buffer_used * 100) / impl_->audio_buffer.size();
        impl_->buffer_status.latency_ms =
            (buffer_used * 1000.0) / (impl_->configured_sample_rate * impl_->configured_channels);

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

#endif // PLATFORM_MACOS
