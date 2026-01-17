/**
 * @file AudioBackend_WASAPI.cpp
 * @brief Windows WASAPI audio backend implementation
 *
 * Features:
 * - Exclusive mode for <50ms latency
 * - Event-driven for low CPU usage
 * - Automatic format conversion
 */

#ifdef PLATFORM_WINDOWS

#ifdef NOMINMAX
#undef NOMINMAX
#endif
#define NOMINMAX

#include "AudioBackend.h"
#include "AudioBackend_WASAPI.h"
#include "utils/Logger.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace xpu;

namespace xpu {
namespace playback {

/**
 * @brief Implementation class
 */
class AudioBackendWASAPI::Impl {
public:
    IMMDeviceEnumerator* device_enumerator;
    IMMDevice* device;
    IAudioClient* audio_client;
    IAudioRenderClient* render_client;

    // Use byte array to store either WAVEFORMATEX or WAVEFORMATEXTENSIBLE
    BYTE wave_format_buffer[sizeof(WAVEFORMATEXTENSIBLE)];
    WAVEFORMATEX& wave_format = *reinterpret_cast<WAVEFORMATEX*>(wave_format_buffer);

    UINT32 buffer_frame_count;
    HANDLE event_handle;

    AudioDevice current_device;
    int configured_sample_rate;
    int configured_channels;
    int configured_buffer_size;
    PlaybackState state;
    BufferStatus buffer_status;

    StatusCallback status_callback;
    std::thread status_thread;
    bool status_thread_running;
    bool exclusive_mode;  // True for exclusive mode, false for shared mode

    // WASAPI constants
    static const int REFTIMES_PER_SEC = 10000000;
    static const int REFTIMES_PER_MILLISEC = 10000;

    Impl()
        : device_enumerator(nullptr)
        , device(nullptr)
        , audio_client(nullptr)
        , render_client(nullptr)
        , buffer_frame_count(0)
        , event_handle(nullptr)
        , configured_sample_rate(48000)
        , configured_channels(2)
        , configured_buffer_size(2048)
        , state(PlaybackState::Stopped)
        , status_thread_running(false)
        , exclusive_mode(false) {  // Default to shared mode for faster initialization (~350ms faster)

        // Initialize format to zero
        std::memset(wave_format_buffer, 0, sizeof(wave_format_buffer));
    }

    ~Impl() {
        if (status_thread_running) {
            status_thread_running = false;
            if (status_thread.joinable()) {
                status_thread.join();
            }
        }

        if (render_client) {
            render_client->Release();
        }
        if (audio_client) {
            audio_client->Release();
        }
        if (device) {
            device->Release();
        }
        if (device_enumerator) {
            device_enumerator->Release();
        }
        if (event_handle) {
            CloseHandle(event_handle);
        }

        CoUninitialize();
    }
};

AudioBackendWASAPI::AudioBackendWASAPI()
    : impl_(std::make_unique<Impl>()) {}

AudioBackendWASAPI::~AudioBackendWASAPI() = default;

void AudioBackendWASAPI::setExclusiveMode(bool exclusive) {
    impl_->exclusive_mode = exclusive;
}

bool AudioBackendWASAPI::isExclusiveMode() const {
    return impl_->exclusive_mode;
}

ErrorCode AudioBackendWASAPI::initialize() {
    LOG_INFO("Initializing WASAPI backend");

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize COM");
        return ErrorCode::AudioBackendError;
    }

    // Get device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&impl_->device_enumerator));

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create device enumerator");
        return ErrorCode::AudioBackendError;
    }

    // Get default output device
    hr = impl_->device_enumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &impl_->device);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get default audio endpoint");
        return ErrorCode::DeviceNotFound;
    }

    // Get device name
    IPropertyStore* props = nullptr;
    hr = impl_->device->OpenPropertyStore(STGM_READ, &props);
    if (SUCCEEDED(hr)) {
        PROPVARIANT var_name;
        PropVariantInit(&var_name);
        hr = props->GetValue(PKEY_Device_FriendlyName, &var_name);
        if (SUCCEEDED(hr)) {
            // Convert LPWSTR to std::string
            int size = WideCharToMultiByte(CP_UTF8, 0, var_name.pwszVal, -1, nullptr, 0, nullptr, nullptr);
            std::string converted_str(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, var_name.pwszVal, -1, &converted_str[0], size, nullptr, nullptr);
            impl_->current_device.name = converted_str;
            PropVariantClear(&var_name);
        }
        props->Release();
    }

    impl_->current_device.id = "default";
    impl_->current_device.api = "wasapi";
    impl_->current_device.is_default = true;

    LOG_INFO("WASAPI backend initialized: {}", impl_->current_device.name);
    return ErrorCode::Success;
}

std::vector<AudioDevice> AudioBackendWASAPI::getDevices() {
    std::vector<AudioDevice> devices;

    // Get default device ID first
    IMMDevice* default_device = nullptr;
    std::string default_device_id;
    HRESULT hr = impl_->device_enumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &default_device);

    if (SUCCEEDED(hr)) {
        LPWSTR default_device_id_w = nullptr;
        if (SUCCEEDED(default_device->GetId(&default_device_id_w))) {
            int size = WideCharToMultiByte(CP_UTF8, 0, default_device_id_w, -1, nullptr, 0, nullptr, nullptr);
            default_device_id.resize(size - 1);
            WideCharToMultiByte(CP_UTF8, 0, default_device_id_w, -1, &default_device_id[0], size, nullptr, nullptr);
            CoTaskMemFree(default_device_id_w);
        }
        default_device->Release();
    }

    IMMDeviceCollection* collection = nullptr;
    hr = impl_->device_enumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &collection);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to enumerate audio endpoints");
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        collection->Item(i, &dev);

        AudioDevice device;
        device.api = "wasapi";
        device.id = "device_" + std::to_string(i);

        // Get device ID to check if it's the default
        LPWSTR device_id_w = nullptr;
        if (SUCCEEDED(dev->GetId(&device_id_w))) {
            int size = WideCharToMultiByte(CP_UTF8, 0, device_id_w, -1, nullptr, 0, nullptr, nullptr);
            std::string device_id_str(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, device_id_w, -1, &device_id_str[0], size, nullptr, nullptr);

            // Check if this is the default device
            if (device_id_str == default_device_id) {
                device.is_default = true;
            }

            CoTaskMemFree(device_id_w);
        }

        IPropertyStore* props = nullptr;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var_name;
            PropVariantInit(&var_name);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var_name))) {
                // Convert LPWSTR to std::string
                int size = WideCharToMultiByte(CP_UTF8, 0, var_name.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                std::string converted_str(size - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, var_name.pwszVal, -1, &converted_str[0], size, nullptr, nullptr);
                device.name = converted_str;
                PropVariantClear(&var_name);
            }
            props->Release();
        }

        devices.push_back(device);
        dev->Release();
    }

    collection->Release();
    return devices;
}

ErrorCode AudioBackendWASAPI::setDevice(const AudioDevice& device) {
    (void)device;  // Suppress unused parameter warning
    
    // For now, only support default device
    // TODO: Implement device switching
    LOG_WARNING("Device switching not fully implemented in WASAPI");
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::configure(int sample_rate, int channels, int buffer_size) {
    impl_->configured_sample_rate = sample_rate;
    impl_->configured_channels = channels;
    impl_->configured_buffer_size = buffer_size;

    // Note: The actual format will be determined by the device's mix format
    // in initializeAudioClient(). We'll check if it matches our requirements.

    return initializeAudioClient();
}

ErrorCode AudioBackendWASAPI::initializeAudioClient() {
    // Try exclusive mode first (if enabled), then fallback to shared mode
    if (impl_->exclusive_mode) {
        LOG_INFO("Attempting WASAPI Exclusive Mode ({} Hz, {} channels)",
                 impl_->configured_sample_rate, impl_->configured_channels);

        ErrorCode ret = initializeAudioClientExclusive();
        if (ret == ErrorCode::Success) {
            LOG_INFO("WASAPI Exclusive Mode initialized successfully");
            return ErrorCode::Success;
        }

        LOG_WARNING("Exclusive mode failed, falling back to Shared Mode");
        impl_->exclusive_mode = false;
    }

    // Fallback to shared mode
    LOG_INFO("Using WASAPI Shared Mode");
    return initializeAudioClientShared();
}

ErrorCode AudioBackendWASAPI::initializeAudioClientExclusive() {
    // Release old audio client
    if (impl_->audio_client) {
        impl_->audio_client->Release();
        impl_->audio_client = nullptr;
    }

    // Activate audio client
    HRESULT hr = impl_->device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&impl_->audio_client));

    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio client: HRESULT={:#x}", static_cast<unsigned int>(hr));
        return ErrorCode::AudioBackendError;
    }

    // Setup the desired format for exclusive mode
    std::memset(impl_->wave_format_buffer, 0, sizeof(impl_->wave_format_buffer));
    impl_->wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    impl_->wave_format.nChannels = impl_->configured_channels;
    impl_->wave_format.nSamplesPerSec = impl_->configured_sample_rate;
    impl_->wave_format.wBitsPerSample = 32;
    impl_->wave_format.nBlockAlign = impl_->wave_format.nChannels * impl_->wave_format.wBitsPerSample / 8;
    impl_->wave_format.nAvgBytesPerSec = impl_->wave_format.nSamplesPerSec * impl_->wave_format.nBlockAlign;
    impl_->wave_format.cbSize = 0;

    // Check if format is supported
    WAVEFORMATEX* closest_match = nullptr;
    hr = impl_->audio_client->IsFormatSupported(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        &impl_->wave_format,
        &closest_match);

    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
        LOG_WARNING("Format {} Hz, {} channels not supported in exclusive mode",
                    impl_->configured_sample_rate, impl_->configured_channels);
        if (closest_match) {
            LOG_INFO("Closest match: {} Hz, {} channels",
                     closest_match->nSamplesPerSec, closest_match->nChannels);
            CoTaskMemFree(closest_match);
        }
        return ErrorCode::SampleRateNotSupported;
    } else if (FAILED(hr) && hr != AUDCLNT_E_UNSUPPORTED_FORMAT) {
        LOG_ERROR("IsFormatSupported failed: HRESULT={:#x}", static_cast<unsigned int>(hr));
        return ErrorCode::AudioBackendError;
    }

    // Calculate buffer duration for exclusive mode (aim for very low latency)
    // Exclusive mode allows much smaller buffers
    REFERENCE_TIME buffer_duration =
        (impl_->configured_buffer_size * Impl::REFTIMES_PER_SEC) /
        impl_->configured_sample_rate;

    // Initialize audio client in exclusive mode
    hr = impl_->audio_client->Initialize(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        buffer_duration,  // In exclusive mode, periodicity must equal duration
        &impl_->wave_format,
        nullptr);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize exclusive mode audio client: HRESULT={:#x}", static_cast<unsigned int>(hr));
        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
            return ErrorCode::SampleRateNotSupported;
        } else if (hr == AUDCLNT_E_DEVICE_IN_USE) {
            LOG_ERROR("Device is already in use by another application");
            return ErrorCode::DeviceUnavailable;
        }
        return ErrorCode::AudioBackendError;
    }

    // Get buffer size
    hr = impl_->audio_client->GetBufferSize(&impl_->buffer_frame_count);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get buffer size");
        return ErrorCode::AudioBackendError;
    }

    // Create event handle
    if (!impl_->event_handle) {
        impl_->event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    // Set event handle
    hr = impl_->audio_client->SetEventHandle(impl_->event_handle);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set event handle");
        return ErrorCode::AudioBackendError;
    }

    // Get render client
    hr = impl_->audio_client->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&impl_->render_client));

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get render client");
        return ErrorCode::AudioBackendError;
    }

    // Calculate actual latency
    double latency_ms = (impl_->buffer_frame_count * 1000.0) / impl_->configured_sample_rate;
    LOG_INFO("WASAPI Exclusive Mode configured: {} Hz, {} channels, {} frames buffer ({:.2f} ms latency)",
             impl_->configured_sample_rate, impl_->configured_channels,
             impl_->buffer_frame_count, latency_ms);

    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::initializeAudioClientShared() {
    // Release old audio client
    if (impl_->audio_client) {
        impl_->audio_client->Release();
        impl_->audio_client = nullptr;
    }

    // Store requested format for comparison
    int requested_sample_rate = impl_->configured_sample_rate;
    int requested_channels = impl_->configured_channels;

    // Activate audio client
    HRESULT hr = impl_->device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&impl_->audio_client));

    if (FAILED(hr)) {
        LOG_ERROR("Failed to activate audio client: HRESULT={:#x}", static_cast<unsigned int>(hr));
        return ErrorCode::AudioBackendError;
    }

    // Get the mix format - in shared mode we need to tell WASAPI what format to use
    WAVEFORMATEX* mix_format = nullptr;
    hr = impl_->audio_client->GetMixFormat(&mix_format);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get mix format: HRESULT={:#x}", static_cast<unsigned int>(hr));
        return ErrorCode::AudioBackendError;
    }

    LOG_INFO("Device mix format: {} Hz, {} channels, {} bits, tag={}",
             mix_format->nSamplesPerSec, mix_format->nChannels,
             mix_format->wBitsPerSample, mix_format->wFormatTag);

    // Calculate buffer duration using device's sample rate
    REFERENCE_TIME buffer_duration =
        static_cast<REFERENCE_TIME>(
            (static_cast<double>(impl_->configured_buffer_size) * Impl::REFTIMES_PER_SEC) /
            mix_format->nSamplesPerSec
        );

    LOG_INFO("Buffer duration: {} ref_time ({} samples at device rate)",
             buffer_duration, impl_->configured_buffer_size);

    // Initialize audio client in shared mode with mix format
    // The Windows audio engine will automatically convert our input to match this format
    hr = impl_->audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0,              // Must be 0 in shared mode
        mix_format,     // Use the device's mix format - system handles conversion
        nullptr);

    // Free mix format immediately after Initialize (it makes a copy)
    CoTaskMemFree(mix_format);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize shared mode audio client: HRESULT={:#x}", static_cast<unsigned int>(hr));
        if (hr == 0x80070057) {
            LOG_ERROR("E_INVALIDARG - This usually means incorrect format or buffer duration");
        }
        return ErrorCode::AudioBackendError;
    }

    // Get the actual format being used (query it again to be sure)
    WAVEFORMATEX* actual_format = nullptr;
    hr = impl_->audio_client->GetMixFormat(&actual_format);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get actual format: HRESULT={:#x}", static_cast<unsigned int>(hr));
        return ErrorCode::AudioBackendError;
    }

    // Copy the actual format
    std::memcpy(&impl_->wave_format, actual_format, sizeof(WAVEFORMATEX));
    if (actual_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* wave_ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(actual_format);
        std::memcpy(impl_->wave_format_buffer, wave_ext, sizeof(WAVEFORMATEXTENSIBLE));
    }

    impl_->configured_sample_rate = impl_->wave_format.nSamplesPerSec;
    impl_->configured_channels = impl_->wave_format.nChannels;

    LOG_INFO("Shared mode initialized successfully: {} Hz, {} channels",
             impl_->configured_sample_rate, impl_->configured_channels);
    LOG_INFO("Input will be auto-converted to match device format");

    CoTaskMemFree(actual_format);

    // Get buffer size
    hr = impl_->audio_client->GetBufferSize(&impl_->buffer_frame_count);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get buffer size");
        return ErrorCode::AudioBackendError;
    }

    // Create event handle
    if (!impl_->event_handle) {
        impl_->event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    // Set event handle
    hr = impl_->audio_client->SetEventHandle(impl_->event_handle);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set event handle");
        return ErrorCode::AudioBackendError;
    }

    // Get render client
    hr = impl_->audio_client->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&impl_->render_client));

    if (FAILED(hr)) {
        LOG_ERROR("Failed to get render client");
        return ErrorCode::AudioBackendError;
    }

    // Calculate actual latency
    double latency_ms = (impl_->buffer_frame_count * 1000.0) / impl_->configured_sample_rate;
    LOG_INFO("WASAPI Shared Mode configured: {} Hz, {} channels, {} frames buffer ({:.2f} ms latency)",
             impl_->configured_sample_rate, impl_->configured_channels,
             impl_->buffer_frame_count, latency_ms);

    // NOW check if device format matches requested format
    // If not, return AudioFormatMismatch to signal the need for resampling
    // IMPORTANT: We do this AFTER all initialization is complete
    if (impl_->configured_sample_rate != requested_sample_rate ||
        impl_->configured_channels != requested_channels) {
        LOG_INFO("Device format differs from requested format - returning AudioFormatMismatch");
        LOG_INFO("  Requested: {} Hz, {} channels", requested_sample_rate, requested_channels);
        LOG_INFO("  Actual: {} Hz, {} channels (resampling required)", impl_->configured_sample_rate, impl_->configured_channels);
        // The audio client is fully initialized and ready to use
        // We return AudioFormatMismatch to tell the main code to enable resampling
        return ErrorCode::AudioFormatMismatch;
    }

    // Format matches perfectly, return success
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::start() {
    if (!impl_->audio_client) {
        return ErrorCode::InvalidState;
    }

    HRESULT hr = impl_->audio_client->Start();
    if (FAILED(hr)) {
        LOG_ERROR("Failed to start audio client");
        return ErrorCode::AudioBackendError;
    }

    impl_->state = PlaybackState::Playing;

    // Start status thread
    if (!impl_->status_thread_running) {
        impl_->status_thread_running = true;
        impl_->status_thread = std::thread([this]() { statusThreadFunc(); });
    }

    LOG_INFO("WASAPI playback started");
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::stop() {
    if (!impl_->audio_client) {
        return ErrorCode::InvalidState;
    }

    impl_->audio_client->Stop();
    impl_->audio_client->Reset();
    impl_->state = PlaybackState::Stopped;

    // Stop status thread
    impl_->status_thread_running = false;
    if (impl_->status_thread.joinable()) {
        impl_->status_thread.join();
    }

    LOG_INFO("WASAPI playback stopped");
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::pause() {
    if (!impl_->audio_client) {
        return ErrorCode::InvalidState;
    }

    impl_->audio_client->Stop();
    impl_->state = PlaybackState::Paused;

    LOG_INFO("WASAPI playback paused");
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::resume() {
    if (!impl_->audio_client) {
        return ErrorCode::InvalidState;
    }

    impl_->audio_client->Start();
    impl_->state = PlaybackState::Playing;

    LOG_INFO("WASAPI playback resumed");
    return ErrorCode::Success;
}

ErrorCode AudioBackendWASAPI::write(const float* data, int frames) {
    if (!impl_->render_client || impl_->state != PlaybackState::Playing) {
        return ErrorCode::InvalidState;
    }

    // Log first write for debugging
    static bool first_write = true;
    if (first_write) {
        LOG_INFO("First write: {} frames", frames);
        LOG_INFO("  Configured: {} Hz, {} channels", impl_->configured_sample_rate, impl_->configured_channels);
        LOG_INFO("  Device format: {} Hz, {} ch, {} bits, tag={}, blockAlign={}",
                 impl_->wave_format.nSamplesPerSec, impl_->wave_format.nChannels,
                 impl_->wave_format.wBitsPerSample, impl_->wave_format.wFormatTag,
                 impl_->wave_format.nBlockAlign);
        first_write = false;
    }

    // Try multiple times if buffer is full - loop until all data is written
    // Use a large retry limit since we may be writing millions of frames
    const int MAX_RETRIES = 10000000;  // Effectively unlimited for normal use
    int frames_written = 0;

    for (int retry = 0; retry < MAX_RETRIES && frames_written < frames; ++retry) {
        // Get available buffer space
        UINT32 padding = 0;
        HRESULT hr = impl_->audio_client->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            LOG_ERROR("GetCurrentPadding failed: frames_written={}/{}", frames_written, frames);
            return ErrorCode::AudioBackendError;
        }

        UINT32 available_frames = impl_->buffer_frame_count - padding;
        if (available_frames == 0) {
            // Buffer full, wait for event with timeout
            DWORD wait_result = WaitForSingleObject(impl_->event_handle, 100);
            if (wait_result != WAIT_OBJECT_0) {
                // Timeout - check if we've made progress
                if (retry > 0 && retry % 1000 == 0) {
                    LOG_WARNING("Buffer still full after {} retries, {} frames written", retry, frames_written);
                }
                // Don't break, keep trying
            }
            continue;
        }

        UINT32 frames_to_write = static_cast<UINT32>(std::min(
            static_cast<int>(frames - frames_written),
            static_cast<int>(available_frames)
        ));

        // Get buffer
        BYTE* buffer = nullptr;
        hr = impl_->render_client->GetBuffer(frames_to_write, &buffer);
        if (FAILED(hr)) {
            LOG_ERROR("GetBuffer failed: frames={}, hr={:#x}", frames_to_write,
                     static_cast<unsigned int>(hr));
            return ErrorCode::AudioBackendError;
        }

        // Copy and convert data based on format
        // Check if format is IEEE_FLOAT (WAVE_FORMAT_IEEE_FLOAT = 3)
        bool is_float = (impl_->wave_format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                       (impl_->wave_format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);

        // Debug: Log first write data statistics
        static bool first_data_write = true;
        if (first_data_write && frames_to_write > 0) {
            const float* write_data = data + (frames_written * impl_->configured_channels);
            float min_val = write_data[0];
            float max_val = write_data[0];
            double sum = 0.0;
            UINT32 total_samples = frames_to_write * impl_->wave_format.nChannels;
            UINT32 check_samples = (total_samples < 100) ? total_samples : 100;
            for (UINT32 i = 0; i < check_samples; ++i) {
                if (write_data[i] < min_val) min_val = write_data[i];
                if (write_data[i] > max_val) max_val = write_data[i];
                sum += (write_data[i] >= 0) ? write_data[i] : -write_data[i];
            }
            double avg = sum / check_samples;
            LOG_INFO("First write data stats (first {} samples): min={}, max={}, avg_abs={}",
                     check_samples, min_val, max_val, avg);
            first_data_write = false;
        }

        if (is_float && impl_->wave_format.wBitsPerSample == 32) {
            // 32-bit float - direct copy
            size_t bytes_to_copy = frames_to_write * impl_->wave_format.nBlockAlign;
            std::memcpy(buffer, data + (frames_written * impl_->configured_channels), bytes_to_copy);
        } else if (impl_->wave_format.wBitsPerSample == 32) {
            // 32-bit integer - convert from float
            int32_t* int_buffer = reinterpret_cast<int32_t*>(buffer);
            for (UINT32 i = 0; i < frames_to_write * impl_->wave_format.nChannels; i++) {
                float sample = data[(frames_written * impl_->configured_channels) + i];
                // Clamp to [-1.0, 1.0] and convert to 32-bit integer
                sample = std::max(-1.0f, std::min(1.0f, sample));
                int_buffer[i] = static_cast<int32_t>(sample * 2147483647.0f);
            }
        } else if (impl_->wave_format.wBitsPerSample == 24) {
            // 24-bit integer - convert from float
            uint8_t* byte_buffer = buffer;
            for (UINT32 i = 0; i < frames_to_write * impl_->wave_format.nChannels; i++) {
                float sample = data[(frames_written * impl_->configured_channels) + i];
                // Clamp and convert to 24-bit
                sample = std::max(-1.0f, std::min(1.0f, sample));
                int32_t sample_24 = static_cast<int32_t>(sample * 8388607.0f);
                // Write as little-endian 24-bit
                byte_buffer[i * 3] = sample_24 & 0xFF;
                byte_buffer[i * 3 + 1] = (sample_24 >> 8) & 0xFF;
                byte_buffer[i * 3 + 2] = (sample_24 >> 16) & 0xFF;
            }
        } else if (impl_->wave_format.wBitsPerSample == 16) {
            // 16-bit integer - convert from float
            int16_t* short_buffer = reinterpret_cast<int16_t*>(buffer);
            for (UINT32 i = 0; i < frames_to_write * impl_->wave_format.nChannels; i++) {
                float sample = data[(frames_written * impl_->configured_channels) + i];
                // Clamp and convert to 16-bit
                sample = std::max(-1.0f, std::min(1.0f, sample));
                short_buffer[i] = static_cast<int16_t>(std::round(sample * 32767.0f));
            }
        } else {
            LOG_ERROR("Unsupported bit depth: {}", impl_->wave_format.wBitsPerSample);
            impl_->render_client->ReleaseBuffer(0, 0);
            return ErrorCode::AudioBackendError;
        }

        // Release buffer
        hr = impl_->render_client->ReleaseBuffer(frames_to_write, 0);
        if (FAILED(hr)) {
            LOG_ERROR("ReleaseBuffer failed: hr={:#x}", static_cast<unsigned int>(hr));
            return ErrorCode::AudioBackendError;
        }

        frames_written += frames_to_write;
        impl_->buffer_status.samples_played += frames_to_write;

        // Log progress for large writes
        if (frames > 100000 && frames_written % 100000 == 0) {
            LOG_INFO("  Write progress: {} / {} frames ({:.1f}%)",
                     frames_written, frames, (frames_written * 100.0) / frames);
        }
    }

    if (frames_written < frames) {
        LOG_WARNING("Only wrote {} / {} frames before returning", frames_written, frames);
    }

    // Return success if we wrote at least some data
    return ErrorCode::Success;
}

BufferStatus AudioBackendWASAPI::getBufferStatus() const {
    return impl_->buffer_status;
}

PlaybackState AudioBackendWASAPI::getState() const {
    return impl_->state;
}

void AudioBackendWASAPI::setStatusCallback(StatusCallback callback) {
    impl_->status_callback = callback;
}

double AudioBackendWASAPI::getLatencyMs() const {
    return (impl_->buffer_frame_count * 1000.0) / impl_->configured_sample_rate;
}

void AudioBackendWASAPI::statusThreadFunc() {
    while (impl_->status_thread_running) {
        // Update buffer status
        UINT32 padding = 0;
        if (impl_->audio_client) {
            impl_->audio_client->GetCurrentPadding(&padding);
        }

        impl_->buffer_status.fill_level =
            static_cast<int>(100 - (padding * 100.0 / impl_->buffer_frame_count));

        impl_->buffer_status.latency_ms =
            (padding * 1000.0) / impl_->configured_sample_rate;

        // Call status callback
        if (impl_->status_callback) {
            protocol::PlaybackStatus status;
            // Convert PlaybackState to PlaybackStatus::State
            switch (impl_->state) {
                case PlaybackState::Stopped:
                    status.state = protocol::PlaybackStatus::State::Stopped;
                    break;
                case PlaybackState::Playing:
                    status.state = protocol::PlaybackStatus::State::Playing;
                    break;
                case PlaybackState::Paused:
                    status.state = protocol::PlaybackStatus::State::Paused;
                    break;
                default:
                    status.state = protocol::PlaybackStatus::State::Error;
                    break;
            }
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

#endif // PLATFORM_WINDOWS
