/**
 * @file xpuProcess.cpp
 * @brief DSP processing - XPU Module 5
 *
 * Provides volume control, fade effects, and basic EQ
 */

#include "VolumeControl.h"
#include "FadeEffects.h"
#include "Equalizer.h"
#include "protocol/ErrorCode.h"
#include "utils/Logger.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

using namespace xpu;
using namespace xpu::process;

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "  --volume <0-200>        Volume percentage (default: 100)\n";
    std::cout << "  --fade-in <ms>          Fade-in duration in ms\n";
    std::cout << "  --fade-out <ms>         Fade-out duration in ms\n";
    std::cout << "  --eq <preset>           EQ preset (rock, pop, classical, jazz, flat)\n";
    std::cout << "  --eq-low <dB>           Custom low gain (-20 to +20)\n";
    std::cout << "  --eq-mid <dB>           Custom mid gain (-20 to +20)\n";
    std::cout << "  --eq-high <dB>          Custom high gain (-20 to +20)\n";
    std::cout << "\nInput:\n";
    std::cout << "  Reads JSON metadata + PCM audio from stdin\n";
    std::cout << "  Input format: [JSON metadata][8-byte size header][PCM data]\n";
    std::cout << "\nOutput:\n";
    std::cout << "  Writes JSON metadata + processed audio to stdout\n";
    std::cout << "  Output format: [JSON metadata][8-byte size header][PCM data]\n";
    std::cout << "\nExamples:\n";
    std::cout << "  xpuLoad song.flac | xpuProcess --volume 80 | xpuPlay\n";
    std::cout << "  xpuLoad song.flac | xpuProcess --eq rock | xpuPlay\n";
    std::cout << "  xpuLoad song.flac | xpuProcess --fade-in 2000 --fade-out 3000 | xpuPlay\n";
    std::cout << "\nVerbose mode:\n";
    std::cout << "  Use -V to enable debug logging (default: warnings and errors only)\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuProcess version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
    std::cout << "DSP Effects: Volume, Fade, 3-band EQ\n";
}

/**
 * @brief Apply EQ preset
 */
void applyEQPreset(const std::string& preset, float& low, float& mid, float& high) {
    if (preset == "rock") {
        low = 5.0f;
        mid = -2.0f;
        high = 5.0f;
    } else if (preset == "pop") {
        low = 3.0f;
        mid = 1.0f;
        high = 3.0f;
    } else if (preset == "classical") {
        low = 4.0f;
        mid = 2.0f;
        high = 0.0f;
    } else if (preset == "jazz") {
        low = 3.0f;
        mid = 1.0f;
        high = -2.0f;
    } else if (preset == "flat") {
        low = 0.0f;
        mid = 0.0f;
        high = 0.0f;
    } else if (preset == "electronic") {
        low = 6.0f;
        mid = -3.0f;
        high = 3.0f;
    }
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Set console to UTF-8 mode on Windows
    #ifdef PLATFORM_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // Set stdin/stdout to binary mode for proper data piping
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    #endif

    // Disable buffering for stdin/stdout to enable streaming
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.setf(std::ios::unitbuf);  // Force unbuffered output

    // Parse command-line arguments (first pass to get verbose flag)
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            break;
        }
    }

    // Initialize logger with verbose setting
    utils::Logger::initialize("", false, verbose, "xpuProcess");  // Console only for xpuProcess

    float volume = 1.0f;
    int fade_in_ms = 0;
    int fade_out_ms = 0;
    std::string eq_preset = "flat";
    float eq_low = 0.0f;
    float eq_mid = 0.0f;
    float eq_high = 0.0f;
    bool use_custom_eq = false;

    // Parse command-line arguments (second pass for all options)
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            // Already handled in first pass, ignore here
            continue;
        } else if (strcmp(argv[i], "--volume") == 0) {
            if (i + 1 < argc) {
                float vol_percent = static_cast<float>(std::atof(argv[++i]));
                volume = vol_percent / 100.0f;
                if (volume < 0.0f || volume > 2.0f) {
                    std::cerr << "Error: Volume must be between 0 and 200%\n";
                    return 1;
                }
            }
        } else if (strcmp(argv[i], "--fade-in") == 0) {
            if (i + 1 < argc) {
                fade_in_ms = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--fade-out") == 0) {
            if (i + 1 < argc) {
                fade_out_ms = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--eq") == 0) {
            if (i + 1 < argc) {
                eq_preset = argv[++i];
                applyEQPreset(eq_preset, eq_low, eq_mid, eq_high);
            }
        } else if (strcmp(argv[i], "--eq-low") == 0) {
            if (i + 1 < argc) {
                eq_low = static_cast<float>(std::atof(argv[++i]));
                use_custom_eq = true;
            }
        } else if (strcmp(argv[i], "--eq-mid") == 0) {
            if (i + 1 < argc) {
                eq_mid = static_cast<float>(std::atof(argv[++i]));
                use_custom_eq = true;
            }
        } else if (strcmp(argv[i], "--eq-high") == 0) {
            if (i + 1 < argc) {
                eq_high = static_cast<float>(std::atof(argv[++i]));
                use_custom_eq = true;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    LOG_INFO("xpuProcess starting");
    LOG_INFO("Volume: {}%", volume * 100);
    LOG_INFO("Fade-in: {} ms", fade_in_ms);
    LOG_INFO("Fade-out: {} ms", fade_out_ms);
    LOG_INFO("EQ: low={}dB, mid={}dB, high={}dB", eq_low, eq_mid, eq_high);

    // Initialize DSP effects
    VolumeControl volume_ctrl;
    volume_ctrl.setVolume(volume);

    FadeEffects fade_effects;
    FadeEffects fade_out_effects;

    Equalizer eq;
    eq.setBandGain(0, eq_low);    // Bass
    eq.setBandGain(1, eq_mid);    // Mid
    eq.setBandGain(2, eq_high);   // Treble

    // Read JSON metadata from stdin
    std::string json_str;
    char c;
    bool json_complete = false;
    int brace_count = 0;
    bool in_json = false;
    int max_json_size = 100000;
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
                int next_char = std::cin.peek();
                if (next_char == '\n' || next_char == '\r') {
                    std::cin.get(c);
                    if (c == '\r' && std::cin.peek() == '\n') {
                        std::cin.get(c);
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
    int input_sample_rate = 48000;
    int input_channels = 2;

    size_t sample_rate_pos = json_str.find("\"sample_rate\":");
    if (sample_rate_pos != std::string::npos) {
        size_t value_start = json_str.find(":", sample_rate_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string sample_rate_str = json_str.substr(value_start, value_end - value_start);
        size_t start = sample_rate_str.find_first_not_of(" \t\n");
        size_t end = sample_rate_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            sample_rate_str = sample_rate_str.substr(start, end - start + 1);
            input_sample_rate = std::atoi(sample_rate_str.c_str());
        }
    }

    size_t channels_pos = json_str.find("\"channels\":");
    if (channels_pos != std::string::npos) {
        size_t value_start = json_str.find(":", channels_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string channels_str = json_str.substr(value_start, value_end - value_start);
        size_t start = channels_str.find_first_not_of(" \t\n");
        size_t end = channels_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            channels_str = channels_str.substr(start, end - start + 1);
            input_channels = std::atoi(channels_str.c_str());
        }
    }

    LOG_INFO("Input audio format: {} Hz, {} channels", input_sample_rate, input_channels);

    // Output JSON metadata to stdout
    std::cout << json_str << std::endl;
    std::cout.flush();
    LOG_INFO("JSON metadata output to stdout: {} bytes", json_str.size());

    // Configure fade effects
    bool fade_in_active = (fade_in_ms > 0);
    bool fade_out_active = (fade_out_ms > 0);

    if (fade_in_active) {
        fade_effects.configure(FadeType::In, fade_in_ms, input_sample_rate);
        LOG_INFO("Fade-in configured: {} ms", fade_in_ms);
    }

    if (fade_out_active) {
        // Note: Fade-out requires knowing total duration in advance
        // This will be implemented in Phase 2 when we have better audio analysis
        LOG_INFO("Fade-out will be implemented in Phase 2 (requires total duration)");
    }

    // Read PCM data from stdin with pre-allocated buffers for better performance
    // Pre-allocate to maximum expected chunk size to avoid reallocations
    constexpr size_t MAX_CHUNK_SIZE = 256 * 1024;  // 256KB max chunk size (same as xpuIn2Wav)
    constexpr size_t MAX_SAMPLES = MAX_CHUNK_SIZE / sizeof(float);
    constexpr size_t MAX_CHANNELS = 8;  // Support up to 8 channels

    std::vector<float> audio_buffer;
    std::vector<float> processed_buffer;

    // Pre-allocate buffers to avoid frequent reallocations
    audio_buffer.reserve(MAX_SAMPLES);
    processed_buffer.reserve(MAX_SAMPLES);

    uint64_t total_samples = 0;
    uint64_t total_frames_processed = 0;

    while (true) {
        // Read size header
        uint64_t data_size;
        std::cin.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));

        if (std::cin.eof() || std::cin.fail()) {
            break;
        }

        // Read PCM data
        size_t samples = data_size / sizeof(float);

        // Resize buffers (using reserve ensures no reallocation if size <= capacity)
        audio_buffer.resize(samples);
        processed_buffer.resize(samples);

        std::cin.read(reinterpret_cast<char*>(audio_buffer.data()), data_size);

        if (std::cin.eof() || std::cin.fail()) {
            break;
        }

        // Copy to processed buffer
        std::memcpy(processed_buffer.data(), audio_buffer.data(), data_size);

        // Calculate frames
        size_t frames = samples / input_channels;

        // Apply fade-in
        if (fade_in_active && !fade_effects.isComplete()) {
            fade_effects.process(processed_buffer.data(), static_cast<int>(frames), input_channels);
        }

        // Apply volume control
        volume_ctrl.process(processed_buffer.data(), static_cast<int>(frames), input_channels);

        // Apply EQ
        eq.process(processed_buffer.data(), static_cast<int>(frames), input_channels, input_sample_rate);

        // Write processed audio to stdout
        uint64_t output_size = samples * sizeof(float);
        std::cout.write(reinterpret_cast<const char*>(&output_size), sizeof(output_size));
        std::cout.write(reinterpret_cast<const char*>(processed_buffer.data()), output_size);
        std::cout.flush();
        #ifdef PLATFORM_WINDOWS
        _flushall();  // Force flush all streams on Windows
        #else
        fflush(nullptr);  // Force flush all streams on Unix
        #endif


        total_samples += samples;
        total_frames_processed += frames;
    }

    LOG_INFO("xpuProcess completed");
    LOG_INFO("Total samples processed: {}", total_samples);
    LOG_INFO("Total frames processed: {}", total_frames_processed);

    return 0;
}
