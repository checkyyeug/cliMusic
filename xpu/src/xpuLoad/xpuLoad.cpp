/**
 * @file xpuLoad.cpp
 * @brief Audio file loader - XPU Module 1
 *
 * Loads audio files and outputs metadata and PCM data to stdout
 * Supports: FLAC, WAV, ALAC, DSD (DSF/DSD), MP3, AAC, OGG, OPUS
 */

#include "AudioFileLoader.h"
#include "DSDDecoder.h"
#include "protocol/ErrorCode.h"
#include "protocol/ErrorResponse.h"
#include "protocol/Protocol.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include "../lib/audio/AudioFormat.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>  // for isatty()
#endif

using namespace xpu;

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_file>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "  -m, --metadata          Output only metadata (JSON format)\n";
    std::cout << "  -d, --data              Output only PCM data (binary)\n";
    std::cout << "  -r <rate>, --sample-rate <rate>  Target sample rate (default: keep original)\n";
    std::cout << "\nSupported formats:\n";
    std::cout << "  Lossless: FLAC, WAV, ALAC, DSD (DSF/DSDIFF)\n";
    std::cout << "  Lossy: MP3, AAC, OGG, OPUS\n";
    std::cout << "\nHigh-resolution support:\n";
    std::cout << "  Up to 768kHz sample rate, 32-bit depth\n";
    std::cout << "\nOutput format:\n";
    std::cout << "  By default: Keeps original sample rate\n";
    std::cout << "  With -r/--sample-rate: Outputs at specified rate (32-bit float)\n";
    std::cout << "  Output: [JSON metadata][8-byte size header][PCM data]\n";
    std::cout << "  PCM data: 32-bit float, interleaved, stereo\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " song.flac\n";
    std::cout << "  " << program_name << " -r 48000 song.flac\n";
    std::cout << "  " << program_name << " --metadata song.flac\n";
    std::cout << "  " << program_name << " song.flac | xpuIn2Wav -\n";
    std::cout << "  " << program_name << " song.flac | xpuIn2Wav - -r 48000 -b 16\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuLoad version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
    std::cout << "Copyright (c) 2025 XPU Project\n";
}

/**
 * @brief Convert metadata to JSON string
 */
std::string metadataToJSON(const protocol::AudioMetadata& metadata) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"success\": true,\n";
    json << "  \"metadata\": {\n";
    json << "    \"file_path\": \"" << metadata.file_path << "\",\n";
    json << "    \"format\": \"" << metadata.format_name << "\",\n";
    json << "    \"title\": \"" << metadata.title << "\",\n";
    json << "    \"artist\": \"" << metadata.artist << "\",\n";
    json << "    \"album\": \"" << metadata.album << "\",\n";
    json << "    \"year\": \"" << metadata.year << "\",\n";
    json << "    \"genre\": \"" << metadata.genre << "\",\n";
    json << "    \"track_number\": " << metadata.track_number << ",\n";
    json << "    \"duration\": " << metadata.duration << ",\n";
    json << "    \"sample_rate\": " << metadata.sample_rate << ",\n";
    json << "    \"original_sample_rate\": " << metadata.original_sample_rate << ",\n";
    json << "    \"bit_depth\": " << metadata.bit_depth << ",\n";
    json << "    \"original_bit_depth\": " << metadata.original_bit_depth << ",\n";
    json << "    \"channels\": " << metadata.channels << ",\n";
    json << "    \"sample_count\": " << metadata.sample_count << ",\n";
    json << "    \"bitrate\": " << metadata.bitrate << ",\n";
    json << "    \"is_lossless\": " << (metadata.is_lossless ? "true" : "false") << ",\n";
    json << "    \"is_high_res\": " << (metadata.is_high_res ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Set console to UTF-8 mode on Windows
    #ifdef PLATFORM_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // Set stdout to binary mode for proper data piping
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
    utils::Logger::initialize(utils::PlatformUtils::getLogFilePath(), true, verbose, "xpuLoad");

    LOG_INFO("xpuLoad starting");

    // Parse command-line arguments (second pass for all options)
    const char* input_file = nullptr;
    bool metadata_only = false;
    bool data_only = false;
    int target_sample_rate = 0;  // 0 = keep original, no conversion

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--metadata") == 0) {
            metadata_only = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0) {
            data_only = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            // -r is shorthand for --sample-rate
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                target_sample_rate = atoi(argv[++i]);
            } else {
                std::cerr << "Error: -r requires a sample rate argument\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--sample-rate") == 0) {
            if (i + 1 < argc) {
                target_sample_rate = atoi(argv[++i]);
            } else {
                std::cerr << "Error: --sample-rate requires a rate argument\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Validate arguments
    if (!input_file) {
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        return 1;
    }

    if (metadata_only && data_only) {
        std::cerr << "Error: Cannot specify both --metadata and --data\n";
        return 1;
    }

    // Validate sample rate (only if explicitly specified, not 0)
    if (target_sample_rate != 0 &&
        target_sample_rate != 44100 && target_sample_rate != 48000 &&
        target_sample_rate != 96000 && target_sample_rate != 192000 &&
        target_sample_rate != 384000 && target_sample_rate != 768000) {
        std::cerr << "Warning: Unusual sample rate: " << target_sample_rate << "\n";
    }

    LOG_INFO("Loading file: {}", input_file);
    LOG_INFO("Target sample rate: {}", target_sample_rate);

    // Detect format from extension
    std::string file_path(input_file);
    bool is_dsd = false;

    if (file_path.size() > 4) {
        std::string ext = file_path.substr(file_path.size() - 4);
        if (ext == ".dsf" || ext == ".dff") {
            is_dsd = true;
        }
    }

    ErrorCode ret;
    protocol::AudioMetadata metadata;

    if (is_dsd) {
        // DSD files: Use batch mode (DSD decoder doesn't support streaming yet)
        LOG_INFO("Using DSD decoder (batch mode)");
        load::DSDDecoder dsd_decoder;
        dsd_decoder.setTargetSampleRate(target_sample_rate);
        ret = dsd_decoder.load(input_file);

        if (ret != ErrorCode::Success) {
            std::string error_msg = "Error code: " + std::to_string(static_cast<int>(ret));
            std::cerr << error_msg << "\n";
            LOG_ERROR("Failed to load DSD file: {}", static_cast<int>(ret));
            return static_cast<int>(getHTTPStatusCode(ret));
        }

        metadata = dsd_decoder.getMetadata();

        // Output metadata as JSON
        if (!data_only) {
            std::cout << ::metadataToJSON(metadata);
            std::cout.flush();
            LOG_INFO("Metadata output to stdout");
        }

        // Output PCM data in batch mode for DSD
        #ifdef PLATFORM_WINDOWS
        DWORD mode;
        bool is_piped = !GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
        #else
        bool is_piped = !isatty(STDOUT_FILENO);
        #endif

        if (!metadata_only && (data_only || is_piped)) {
            std::vector<uint8_t> pcm_data_copy = dsd_decoder.getPCMData();
            if (!pcm_data_copy.empty()) {
                constexpr size_t CHUNK_SIZE = 64 * 1024;
                size_t total_size = pcm_data_copy.size();
                size_t offset = 0;
                int chunk_count = 0;

                LOG_INFO("Streaming PCM data in chunks: {} bytes total", total_size);

                while (offset < total_size) {
                    size_t chunk_size = (CHUNK_SIZE < total_size - offset) ? CHUNK_SIZE : total_size - offset;

                    uint64_t chunk_bytes = chunk_size;
                    std::cout.write(reinterpret_cast<const char*>(&chunk_bytes), sizeof(chunk_bytes));
                    std::cout.write(reinterpret_cast<const char*>(pcm_data_copy.data() + offset), chunk_size);
                    std::cout.flush();

                    #ifdef PLATFORM_WINDOWS
                    _flushall();
                    #else
                    fflush(nullptr);
                    #endif

                    chunk_count++;
                    offset += chunk_size;

                    if (chunk_count <= 5) {
                        LOG_INFO("Output chunk {}: {} bytes", chunk_count, chunk_size);
                    }
                }

                LOG_INFO("PCM data streaming complete: {} chunks, {} bytes", chunk_count, total_size);
            }
        } else if (!metadata_only) {
            LOG_INFO("PCM data skipped (not in pipe mode, use -d to force output)");
        }

    } else {
        // Non-DSD files: Use streaming mode (Phase 3 optimization)
        LOG_INFO("Using FFmpeg decoder (streaming mode - Phase 3)");
        load::AudioFileLoader loader;
        loader.setTargetSampleRate(target_sample_rate);

        // Step 1: Prepare streaming (opens file and extracts metadata)
        ret = loader.prepareStreaming(input_file);
        if (ret != ErrorCode::Success) {
            std::string error_msg = "Error code: " + std::to_string(static_cast<int>(ret));
            std::cerr << error_msg << "\n";
            LOG_ERROR("Failed to prepare streaming: {}", static_cast<int>(ret));
            return static_cast<int>(getHTTPStatusCode(ret));
        }

        // Step 2: Get metadata
        metadata = loader.getMetadata();
        LOG_INFO("Metadata extracted successfully");

        // Mark high-resolution audio
        if (metadata.sample_rate >= 96000) {
            metadata.is_high_res = true;
            LOG_INFO("High-resolution audio detected: {} Hz", metadata.sample_rate);
        }

        // Step 3: Output metadata as JSON
        if (!data_only) {
            std::cout << ::metadataToJSON(metadata);
            std::cout.flush();
            LOG_INFO("Metadata output to stdout");
        }

        // Step 4: Stream PCM data ONLY if:
        // 1. --data option is specified, OR
        // 2. stdout is NOT a terminal (piped to another program)
        #ifdef PLATFORM_WINDOWS
        DWORD mode;
        bool is_piped = !GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
        #else
        bool is_piped = !isatty(STDOUT_FILENO);
        #endif

        if (!metadata_only && (data_only || is_piped)) {
            // Stream PCM data using callback
            int chunk_count = 0;

            auto streaming_callback = [&](const float* chunk_data, size_t chunk_samples) -> bool {
                chunk_count++;
                size_t chunk_bytes = chunk_samples * sizeof(float);

                // Output chunk: [8-byte size header][PCM data]
                uint64_t size_header = chunk_bytes;
                std::cout.write(reinterpret_cast<const char*>(&size_header), sizeof(size_header));
                std::cout.write(reinterpret_cast<const char*>(chunk_data), chunk_bytes);
                std::cout.flush();

                #ifdef PLATFORM_WINDOWS
                _flushall();
                #else
                fflush(nullptr);
                #endif

                // Log first few chunks
                if (chunk_count <= 5) {
                    LOG_INFO("Output chunk {}: {} samples ({} bytes)", chunk_count, chunk_samples, chunk_bytes);
                }

                return true;  // Continue streaming
            };

            LOG_INFO("Starting PCM data streaming...");
            ret = loader.streamPCM(streaming_callback, 64 * 1024);  // 64KB chunks

            if (ret != ErrorCode::Success) {
                LOG_ERROR("Streaming failed: {}", static_cast<int>(ret));
                return static_cast<int>(getHTTPStatusCode(ret));
            }

            LOG_INFO("PCM data streaming complete: {} chunks", chunk_count);
        } else if (!metadata_only) {
            LOG_INFO("PCM data skipped (not in pipe mode, use -d to force output)");
        }
    }

    LOG_INFO("xpuLoad completed successfully");
    return 0;
}
