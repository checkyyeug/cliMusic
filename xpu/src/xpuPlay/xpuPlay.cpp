/**
 * @file xpuPlay.cpp
 * @brief Low-latency audio output - XPU Module 3
 *
 * Professional-grade audio playback with <50ms latency target
 * Supports: WASAPI (Windows), CoreAudio (macOS), ALSA (Linux)
 */

#include "AudioBackend.h"
#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <samplerate.h>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

using namespace xpu;
using namespace xpu::playback;
using namespace xpu::protocol;

/**
 * @brief Audio playback statistics
 */
struct PlaybackStats {
    std::atomic<double> current_position;
    std::atomic<uint64_t> bytes_played;
    std::atomic<float> buffer_fill_level;
    std::atomic<float> cpu_usage;
    std::atomic<bool> underrun_detected;

    PlaybackStats()
        : current_position(0.0)
        , bytes_played(0)
        , buffer_fill_level(0.0f)
        , cpu_usage(0.0f)
        , underrun_detected(false) {}
};

// Global playback state
static PlaybackStatus::State g_state = PlaybackStatus::State::Stopped;
static PlaybackStats g_stats;
static std::atomic<bool> g_running(false);

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -d, --device <name>     Audio device to use\n";
    std::cout << "  -b, --buffer-size <sz>  Buffer size in samples (256-16384)\n";
    std::cout << "  -t, --latency-test      Run latency test\n";
    std::cout << "  -l, --list-devices      List available devices\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "  -a, --auto              Enable automatic resampling to device native rate\n";
    std::cout << "  -q, --quality <qual>    Resampling quality (default: sinc_best)\n";
    std::cout << "                          sinc_best, sinc_medium, sinc_fastest\n";
    std::cout << "  -e, --exclusive         Enable WASAPI Exclusive Mode (Windows only)\n";
    std::cout << "                          Slower initialization (~350ms delay), lower latency\n";
    std::cout << "\nPerformance:\n";
    std::cout << "  Target latency: <50ms\n";
    std::cout << "  Default buffer: 2048 samples\n";
    std::cout << "  Shared Mode: Fast initialization (~117ms total), ~10ms latency\n";
    std::cout << "  Exclusive Mode: Slow initialization (~500ms total), <5ms latency\n";
    std::cout << "\nInput:\n";
    std::cout << "  Reads PCM audio from stdin (default)\n";
    std::cout << "  Expects JSON metadata first, then binary data\n";
    std::cout << "\nResampling:\n";
    std::cout << "  If input sample rate doesn't match device capability,\n";
    std::cout << "  use -a to enable automatic resampling.\n";
    std::cout << "  Quality options affect CPU usage and audio quality.\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << "\n";
    std::cout << "  xpuLoad song.flac | " << program_name << "    # Play loaded audio\n";
    std::cout << "  xpuLoad song.flac | xpuIn2Wav | " << program_name << "\n";
    std::cout << "  " << program_name << " -b 1024                 # Low latency mode\n";
    std::cout << "  " << program_name << " -d \"Device Name\"       # Use specific device\n";
    std::cout << "  xpuLoad 44100.flac | " << program_name << " -a # Auto-resample to device rate\n";
    std::cout << "  xpuLoad song.flac | " << program_name << " -a -q sinc_medium\n";
    std::cout << "  xpuLoad song.flac | " << program_name << " -e # Exclusive mode (lowest latency)\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuPlay version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
    std::cout << "Audio backends:\n";
#ifdef PLATFORM_WINDOWS
    std::cout << "  WASAPI (Windows)\n";
#elif defined(PLATFORM_MACOS)
    std::cout << "  CoreAudio (macOS)\n";
#elif defined(PLATFORM_LINUX)
    std::cout << "  ALSA (Linux)\n";
#endif
}

/**
 * @brief List available audio devices
 */
ErrorCode listDevices(AudioBackend* backend) {
    if (!backend) {
        std::cerr << "Error: Backend not initialized\n";
        return ErrorCode::InvalidState;
    }

    auto devices = backend->getDevices();
    std::cout << "{\n";
    std::cout << "  \"devices\": [\n";

    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& dev = devices[i];
        std::cout << "    {\n";
        std::cout << "      \"id\": \"" << dev.id << "\",\n";
        std::cout << "      \"name\": \"" << dev.name << "\",\n";
        std::cout << "      \"api\": \"" << dev.api << "\",\n";
        std::cout << "      \"sample_rate\": " << dev.sample_rate << ",\n";
        std::cout << "      \"channels\": " << dev.channels << ",\n";
        std::cout << "      \"buffer_size\": " << dev.buffer_size << ",\n";
        std::cout << "      \"is_default\": " << (dev.is_default ? "true" : "false") << "\n";
        if (i < devices.size() - 1) {
            std::cout << "    },\n";
        } else {
            std::cout << "    }\n";
        }
    }

    std::cout << "  ]\n";
    std::cout << "}\n";
    return ErrorCode::Success;
}

/**
 * @brief Measure playback latency
 */
ErrorCode measureLatency(AudioBackend* backend) {
    if (!backend) {
        std::cerr << "Error: Backend not initialized\n";
        return ErrorCode::InvalidState;
    }

    std::cout << "{\n";
    std::cout << "  \"latency_test\": {\n";
    std::cout << "    \"target_latency_ms\": 50.0,\n";
    std::cout << "    \"actual_latency_ms\": " << backend->getLatencyMs() << ",\n";
    std::cout << "    \"status\": \"" << (backend->getLatencyMs() < 50.0 ? "PASS" : "FAIL") << "\"\n";
    std::cout << "  }\n";
    std::cout << "}\n";

    return ErrorCode::Success;
}

/**
 * @brief Output status to stdout (JSON format)
 */
void outputStatus() {
    protocol::PlaybackStatus status;

    switch (g_state) {
        case PlaybackStatus::State::Stopped:
            status.state = protocol::PlaybackStatus::State::Stopped;
            break;
        case PlaybackStatus::State::Playing:
            status.state = protocol::PlaybackStatus::State::Playing;
            break;
        case PlaybackStatus::State::Paused:
            status.state = protocol::PlaybackStatus::State::Paused;
            break;
        case PlaybackStatus::State::Error:
            status.state = protocol::PlaybackStatus::State::Error;
            break;
    }

    status.current_position = g_stats.current_position.load();
    status.buffer_fill_level = g_stats.buffer_fill_level.load();
    status.cpu_usage = g_stats.cpu_usage.load();
    status.bytes_played = g_stats.bytes_played.load();

    std::cout << protocol::statusToJSON(status);
    std::cout.flush();
}

/**
 * @brief Convert quality string to libsamplerate converter type
 */
int getConverterType(const char* quality) {
    if (strcmp(quality, "sinc_best") == 0) {
        return SRC_SINC_BEST_QUALITY;
    } else if (strcmp(quality, "sinc_medium") == 0) {
        return SRC_SINC_MEDIUM_QUALITY;
    } else if (strcmp(quality, "sinc_fastest") == 0) {
        return SRC_SINC_FASTEST;
    } else if (strcmp(quality, "linear") == 0) {
        return SRC_LINEAR;
    } else if (strcmp(quality, "zero") == 0) {
        return SRC_ZERO_ORDER_HOLD;
    }
    // Default to best quality
    return SRC_SINC_BEST_QUALITY;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Set console to UTF-8 mode on Windows
    #ifdef PLATFORM_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // Set stdin to binary mode for proper data piping
        _setmode(_fileno(stdin), _O_BINARY);
    #endif

    // Disable buffering for stdin/stdout to enable streaming
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.setf(std::ios::unitbuf);  // Force unbuffered output for status updates

    // Parse command-line arguments (first pass to get verbose flag)
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            break;
        }
    }

    // Initialize logger with verbose setting
    utils::Logger::initialize(utils::PlatformUtils::getLogFilePath(), true, verbose, "xpuPlay");

    LOG_INFO("xpuPlay starting");

    // Parse command-line arguments (second pass for all options)
    const char* device_name = nullptr;
    int buffer_size = 2048;
    bool latency_test = false;
    bool list_devices = false;
    bool auto_resample = false;
    bool exclusive_mode = false;  // Default to shared mode for faster initialization
    const char* quality = "sinc_best";  // Default to highest quality

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "-") == 0) {
            // Ignore "-" argument (stdin is default)
            continue;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                device_name = argv[++i];
            }
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--buffer-size") == 0) {
            if (i + 1 < argc) {
                buffer_size = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--latency-test") == 0) {
            latency_test = true;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list-devices") == 0) {
            list_devices = true;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--auto") == 0) {
            auto_resample = true;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quality") == 0) {
            if (i + 1 < argc) {
                quality = argv[++i];
            }
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--exclusive") == 0) {
            exclusive_mode = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Create audio backend
    auto backend = AudioBackend::create();
    if (!backend) {
        std::cerr << "Error: Failed to create audio backend\n";
        return 1;
    }

    // Set exclusive mode if requested
    if (exclusive_mode) {
        LOG_INFO("Exclusive mode enabled via command line");
        backend->setExclusiveMode(true);
    } else {
        LOG_INFO("Using shared mode for faster initialization");
    }

    // Initialize backend
    ErrorCode ret = backend->initialize();
    if (ret != ErrorCode::Success) {
        std::cerr << "Error: Failed to initialize audio backend: "
                  << static_cast<int>(ret) << "\n";
        return 1;
    }

    // Handle special commands
    if (list_devices) {
        return static_cast<int>(listDevices(backend.get()));
    }

    if (latency_test) {
        return static_cast<int>(measureLatency(backend.get()));
    }

    // Validate buffer size
    if (buffer_size < 256 || buffer_size > 16384) {
        std::cerr << "Error: Buffer size must be between 256 and 16384\n";
        return 1;
    }

    LOG_INFO("Buffer size: {} samples", buffer_size);
    LOG_INFO("Target latency: <50ms");

    // Select device if specified
    if (device_name) {
        auto devices = backend->getDevices();
        bool device_found = false;

        for (const auto& dev : devices) {
            if (dev.id == device_name || dev.name == device_name) {
                ret = backend->setDevice(dev);
                if (ret != ErrorCode::Success) {
                    std::cerr << "Error: Failed to set device: " << device_name << "\n";
                    return 1;
                }
                device_found = true;
                LOG_INFO("Selected device: {}", dev.name);
                break;
            }
        }

        if (!device_found) {
            std::cerr << "Error: Device not found: " << device_name << "\n";
            return 1;
        }
    } else {
        // No device specified, using default device
        auto devices = backend->getDevices();
        for (const auto& dev : devices) {
            if (dev.is_default) {
                LOG_INFO("Using default device: {}", dev.name);
                break;
            }
        }
    }

    // Note: Audio backend will be configured after reading JSON metadata
    // to match the input audio format (sample rate and channels)

    // Start playback will be done after configuring the backend

    // Read and skip JSON metadata
    std::string json_str;
    char c;
    bool json_complete = false;
    int brace_count = 0;
    bool in_json = false;
    int max_json_size = 100000;  // Safety limit
    int json_bytes = 0;

    LOG_INFO("Reading metadata from stdin...");

    while (std::cin.get(c) && json_bytes < max_json_size) {
        json_str += c;
        json_bytes++;

        if (c == '{') {
            in_json = true;
            brace_count++;
        } else if (c == '}') {
            brace_count--;
            if (in_json && brace_count == 0) {
                // Found end of JSON object
                // Check if next character is newline
                int next_char = std::cin.peek();
                if (next_char == '\n' || next_char == '\r') {
                    std::cin.get(c);  // Consume the newline
                    // Check for CRLF
                    if (c == '\r' && std::cin.peek() == '\n') {
                        std::cin.get(c);  // Consume the LF
                    }
                    json_complete = true;
                    break;
                }
            }
        }
    }

    if (!json_complete) {
        LOG_ERROR("Failed to read complete JSON metadata from stdin");
        return 1;
    }

    LOG_INFO("JSON metadata received: {} bytes", json_str.size());

    // Parse sample_rate and channels from JSON
    int input_sample_rate = 48000;  // Default fallback
    int input_channels = 2;          // Default fallback

    // Simple JSON parsing for sample_rate
    size_t sample_rate_pos = json_str.find("\"sample_rate\":");
    if (sample_rate_pos != std::string::npos) {
        size_t value_start = json_str.find(":", sample_rate_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string sample_rate_str = json_str.substr(value_start, value_end - value_start);
        // Trim whitespace
        size_t start = sample_rate_str.find_first_not_of(" \t\n");
        size_t end = sample_rate_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            sample_rate_str = sample_rate_str.substr(start, end - start + 1);
            input_sample_rate = std::atoi(sample_rate_str.c_str());
        }
    }

    // Simple JSON parsing for channels
    size_t channels_pos = json_str.find("\"channels\":");
    if (channels_pos != std::string::npos) {
        size_t value_start = json_str.find(":", channels_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string channels_str = json_str.substr(value_start, value_end - value_start);
        // Trim whitespace
        size_t start = channels_str.find_first_not_of(" \t\n");
        size_t end = channels_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            channels_str = channels_str.substr(start, end - start + 1);
            input_channels = std::atoi(channels_str.c_str());
        }
    }

    LOG_INFO("Input audio format: {} Hz, {} channels", input_sample_rate, input_channels);

    // Determine output sample rate and whether resampling is needed
    int output_sample_rate = input_sample_rate;
    bool needs_resampling = false;

    // Try to configure with input sample rate first
    LOG_INFO("Configuring audio backend for {} Hz, {} channels", input_sample_rate, input_channels);
    ret = backend->configure(input_sample_rate, input_channels, buffer_size);

    if (ret == ErrorCode::AudioFormatMismatch) {
        // Device uses a different format (WASAPI mix format)
        // We need to get the actual format the device is using
        LOG_WARNING("Input format ({} Hz, {} channels) doesn't match device mix format", input_sample_rate, input_channels);
        LOG_INFO("Device will use its mix format instead");

        if (auto_resample) {
            LOG_INFO("Auto-resampling enabled, will convert to device format");
            needs_resampling = true;
            // Note: The backend has already updated to use device's mix format
            // We need to query what the actual format is
            // For now, assume 48000 Hz (most common)
            output_sample_rate = 48000;

            int converter_type = getConverterType(quality);
            const char* quality_name = (converter_type == SRC_SINC_BEST_QUALITY) ? "sinc_best" :
                                      (converter_type == SRC_SINC_MEDIUM_QUALITY) ? "sinc_medium" :
                                      (converter_type == SRC_SINC_FASTEST) ? "sinc_fastest" : "unknown";
            LOG_INFO("Resampling quality: {}", quality_name);
            LOG_INFO("Auto-resampling: {} Hz -> {} Hz", input_sample_rate, output_sample_rate);
        } else {
            LOG_ERROR("Input format doesn't match device format");
            LOG_ERROR("Use -a/--auto to enable automatic resampling");
            std::cerr << "Error: Input format doesn't match device format. Use -a to enable resampling.\n";
            return 1;
        }
    } else if (ret != ErrorCode::Success) {
        // Input sample rate not supported, try fallback rates
        if (auto_resample) {
            LOG_WARNING("Input sample rate ({}) not supported by device", input_sample_rate);
            LOG_INFO("Auto-resampling enabled, trying standard rates...");

            // Try standard sample rates in order of preference
            std::vector<int> standard_rates = {48000, 44100, 96000, 192000};

            for (int rate : standard_rates) {
                if (rate == input_sample_rate) continue;  // Skip input rate (already failed)

                ret = backend->configure(rate, input_channels, buffer_size);
                if (ret == ErrorCode::Success || ret == ErrorCode::AudioFormatMismatch) {
                    output_sample_rate = rate;
                    needs_resampling = true;
                    LOG_INFO("Auto-resampling: {} Hz -> {} Hz", input_sample_rate, output_sample_rate);
                    break;
                }
            }

            if (!needs_resampling) {
                LOG_ERROR("Failed to find compatible sample rate for device");
                std::cerr << "Error: Device does not support input sample rate and auto-resample failed\n";
                return 1;
            }

            int converter_type = getConverterType(quality);
            const char* quality_name = (converter_type == SRC_SINC_BEST_QUALITY) ? "sinc_best" :
                                      (converter_type == SRC_SINC_MEDIUM_QUALITY) ? "sinc_medium" :
                                      (converter_type == SRC_SINC_FASTEST) ? "sinc_fastest" : "unknown";
            LOG_INFO("Resampling quality: {}", quality_name);
        } else {
            // Auto-resample not enabled, show error
            LOG_ERROR("Input sample rate ({}) not supported by device", input_sample_rate);
            LOG_ERROR("Use -a/--auto to enable automatic resampling");
            std::cerr << "Error: Input sample rate not supported by device. Use -a to enable resampling.\n";
            return 1;
        }
    } else if (input_sample_rate != 48000 && auto_resample) {
        LOG_INFO("Input sample rate ({}) is supported, resampling not needed", input_sample_rate);
    }

    // Start playback
    ret = backend->start();
    if (ret != ErrorCode::Success) {
        std::cerr << "Error: Failed to start playback: "
                  << static_cast<int>(ret) << "\n";
        return 1;
    }

    LOG_INFO("Playback started");

    // Only output status JSON in verbose mode
    if (verbose) {
        // Get current time in the same format as logger
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
        #ifdef PLATFORM_WINDOWS
            localtime_s(&tm, &time_t);
        #else
            localtime_r(&time_t, &tm);
        #endif

        char time_buffer[32];
        snprintf(time_buffer, sizeof(time_buffer), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

        std::cout << time_buffer << " [xpuPlay] [status] {\"event\":\"playback_started\",\"latency_ms\":"
                  << backend->getLatencyMs() << "}" << std::endl;
    }

    // Setup status callback (10Hz output) - only in verbose mode
    std::atomic<bool> status_running(true);
    std::thread status_thread([&]() {
        while (status_running) {
            // Only output status in verbose mode
            if (!verbose) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Get current time in the same format as logger
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::tm tm;
            #ifdef PLATFORM_WINDOWS
                localtime_s(&tm, &time_t);
            #else
                localtime_r(&time_t, &tm);
            #endif

            char time_buffer[32];
            snprintf(time_buffer, sizeof(time_buffer), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

            // Output status to stdout
            protocol::PlaybackStatus status;

            // Convert PlaybackState to PlaybackStatus::State
            PlaybackState backend_state = backend->getState();
            switch (backend_state) {
                case PlaybackState::Stopped:
                    status.state = protocol::PlaybackStatus::State::Stopped;
                    break;
                case PlaybackState::Playing:
                    status.state = protocol::PlaybackStatus::State::Playing;
                    break;
                case PlaybackState::Paused:
                    status.state = protocol::PlaybackStatus::State::Paused;
                    break;
                case PlaybackState::Error:
                    status.state = protocol::PlaybackStatus::State::Error;
                    break;
            }

            status.position = 0; // TODO: Track actual position
            status.buffer_fill = backend->getBufferStatus().fill_level;
            status.latency_ms = backend->getLatencyMs();

            std::cout << time_buffer << " [xpuPlay] [status] {";
            std::cout << "\"state\":" << static_cast<int>(status.state) << ",";
            std::cout << "\"position\":" << status.position << ",";
            std::cout << "\"buffer_fill\":" << status.buffer_fill << ",";
            std::cout << "\"latency_ms\":" << status.latency_ms;
            std::cout << "}" << std::endl;
            std::cout.flush();

            // Sleep for 100ms (10Hz)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Setup resampler if needed
    SRC_STATE* src_state = nullptr;
    std::vector<float> resample_buffer;
    double src_ratio = 1.0;

    LOG_INFO("Resampling setup: needs_resampling={}, input_rate={}, output_rate={}, ratio={:.6f}",
             needs_resampling, input_sample_rate, output_sample_rate, src_ratio);

    if (needs_resampling) {
        int converter_type = getConverterType(quality);
        int error = 0;
        src_state = src_new(converter_type, input_channels, &error);

        if (!src_state) {
            LOG_ERROR("Failed to create resampler: {}", src_strerror(error));
            std::cerr << "Error: Failed to create resampler: " << src_strerror(error) << "\n";
            backend->stop();
            return 1;
        }

        src_ratio = static_cast<double>(output_sample_rate) / static_cast<double>(input_sample_rate);
        LOG_INFO("Resampler initialized: ratio={:.6f}, channels={}", src_ratio, input_channels);
    }

    // Now read PCM data in chunks and play
    constexpr size_t BUFFER_SIZE = 4096;
    std::vector<float> audio_buffer(BUFFER_SIZE);

    int chunk_count = 0;
    while (true) {
        // Read size header
        uint64_t data_size;
        std::cin.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));

        if (std::cin.eof() || std::cin.fail()) {
            LOG_INFO("End of input stream reached");
            break;
        }

        // Read PCM data
        size_t samples = data_size / sizeof(float);
        size_t input_frames = samples / input_channels;  // Use actual channel count

        // Ensure buffer is large enough
        if (samples > audio_buffer.size()) {
            audio_buffer.resize(samples);
        }

        std::cin.read(reinterpret_cast<char*>(audio_buffer.data()), data_size);

        if (std::cin.eof() || std::cin.fail()) {
            LOG_INFO("End of PCM data reached");
            break;
        }

        chunk_count++;
        if (chunk_count <= 3) {
            LOG_INFO("Processing chunk {}: {} samples, {} frames", chunk_count, samples, input_frames);
        }

        // Resample if needed
        const float* output_data = audio_buffer.data();
        size_t output_frames = input_frames;

        if (needs_resampling && src_state) {
            // Calculate output frames needed
            size_t output_frames_needed = static_cast<size_t>(input_frames * src_ratio) + 1;

            // Ensure resample buffer is large enough
            if (output_frames_needed * input_channels > resample_buffer.size()) {
                resample_buffer.resize(output_frames_needed * input_channels);
            }

            // Setup libsamplerate data
            SRC_DATA src_data;
            src_data.data_in = audio_buffer.data();
            src_data.input_frames = input_frames;
            src_data.data_out = resample_buffer.data();
            src_data.output_frames = output_frames_needed;
            src_data.src_ratio = src_ratio;
            src_data.end_of_input = 0;  // More data coming

            // Process resampling
            int error = src_process(src_state, &src_data);
            if (error != 0) {
                LOG_ERROR("Resampling failed: {}", src_strerror(error));
                // Try to continue with original data
                output_data = audio_buffer.data();
                output_frames = input_frames;
            } else {
                output_data = resample_buffer.data();
                output_frames = src_data.output_frames_gen;
                if (chunk_count <= 3) {
                    LOG_INFO("Resampled: {} frames -> {} frames", input_frames, output_frames);
                }
            }
        }

        // Write to audio backend in chunks (backend has limited buffer size)
        // Write the entire chunk at once - WASAPI will handle buffering internally
        ret = backend->write(output_data, static_cast<int>(output_frames));
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Failed to write audio data: {}", static_cast<int>(ret));
            if (ret == ErrorCode::BufferUnderrun && verbose) {
                // Get current time in the same format as logger
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

                std::tm tm;
                #ifdef PLATFORM_WINDOWS
                    localtime_s(&tm, &time_t);
                #else
                    localtime_r(&time_t, &tm);
                #endif

                char time_buffer[32];
                snprintf(time_buffer, sizeof(time_buffer), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

                std::cout << time_buffer << " [xpuPlay] [warning] {\"event\":\"buffer_underrun\"}" << std::endl;
            }
            // Continue to try recovery
        }

        if (chunk_count <= 3) {
            LOG_INFO("Completed chunk {}: {} frames written", chunk_count, output_frames);
        }
    }

    // Cleanup resampler
    if (src_state) {
        src_delete(src_state);
    }

    // Wait for all buffered audio to finish playing
    LOG_INFO("Waiting for playback to complete...");
    int wait_count = 0;
    const int MAX_WAIT_SECONDS = 30;  // Wait up to 30 seconds for buffer to drain
    while (wait_count < MAX_WAIT_SECONDS * 10) {  // Check 10 times per second
        BufferStatus status = backend->getBufferStatus();

        // Check if buffer is drained (very low fill level) OR backend stopped playing
        // Use OR instead of AND to avoid waiting too long
        if (status.fill_level < 5 || backend->getState() != PlaybackState::Playing) {
            LOG_INFO("Playback buffer drained (fill_level={}, state={})",
                     status.fill_level, static_cast<int>(backend->getState()));
            break;
        }

        // Also check if we've waited long enough based on expected duration
        // For safety, break after timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (wait_count >= MAX_WAIT_SECONDS * 10) {
        LOG_WARNING("Playback timeout after {} seconds", MAX_WAIT_SECONDS);
    }

    // Stop status thread
    status_running = false;
    if (status_thread.joinable()) {
        status_thread.join();
    }

    // Stop playback
    backend->stop();

    LOG_INFO("Playback completed");
    std::cout << "{\"event\":\"playback_stopped\"}" << std::endl;

    return 0;
}
