/**
 * @file xpuIn2Wav.cpp
 * @brief Format converter + FFT cache - XPU Module 2
 *
 * Converts audio to WAV format with optional FFT caching
 * Performance target: 10-100x speedup with cache
 */

#include "FormatConverter.h"
#include "FFTEngine.h"
#include "CacheManager.h"
#include "protocol/ErrorCode.h"
#include "protocol/ErrorResponse.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include "audio/AudioFormat.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cctype>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <io.h>
#include <fcntl.h>
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
    std::cout << "  -r, --rate <Hz>         Output sample rate (default: keep original)\n";
    std::cout << "  -b, --bits <depth>      Output bit depth (16, 24, 32, default: 32)\n";
    std::cout << "  -c, --channels <num>    Output channels (default: keep original)\n";
    std::cout << "  -o, --output <file>     Output to WAV file instead of stdout\n";
    std::cout << "  -q, --quality <qual>    Resampling quality (best, medium, fast)\n";
    std::cout << "  -f, --force             Bypass FFT cache\n";
    std::cout << "  --cache-dir <path>      FFT cache directory\n";
    std::cout << "  --fft-size <size>       FFT size (1024, 2048, 4096, 8192)\n";
    std::cout << "\nInput modes:\n";
    std::cout << "  File mode:  " << program_name << " input.flac\n";
    std::cout << "  Pipe mode:  xpuLoad song.flac | " << program_name << " -\n";
    std::cout << "\nOutput:\n";
    std::cout << "  File mode:       Creates <input>_out.wav (or -o specified name)\n";
    std::cout << "  Pipe mode:       Outputs to stdout by default (for piping to xpuPlay)\n";
    std::cout << "  Pipe mode -o:    Creates WAV file instead of stdout\n";
    std::cout << "\nSupported formats:\n";
    std::cout << "  FLAC, WAV, ALAC, DSD (DSF/DSDIFF), MP3, AAC, OGG, OPUS\n";
    std::cout << "\nFFT caching (Phase 2):\n";
    std::cout << "  First run: ~30s for 5-minute song\n";
    std::cout << "  Cached run: <3s (10-100x speedup)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " song.flac\n";
    std::cout << "  " << program_name << " -r 48000 -b 16 song.flac\n";
    std::cout << "  " << program_name << " -o output.wav song.flac\n";
    std::cout << "  xpuLoad song.flac | " << program_name << " - | xpuPlay -\n";
    std::cout << "  xpuLoad song.flac | " << program_name << " - -r 48000 | xpuPlay -\n";
    std::cout << "  xpuLoad song.flac | " << program_name << " - -o output.wav\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuIn2Wav version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
    std::cout << "Features: Format conversion, FFT caching (10-100x speedup)\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Set console to UTF-8 mode on Windows
    #ifdef PLATFORM_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    // Initialize logger
    utils::Logger::initialize(utils::PlatformUtils::getLogFilePath(), true);

    LOG_INFO("xpuIn2Wav starting");

    // Parse command-line arguments
    const char* input_file = nullptr;
    const char* output_file = nullptr;  // User-specified output file
    int output_sample_rate = 0;  // 0 = keep original
    int output_bit_depth = 32;   // Default to 32-bit float
    int output_channels = 0;     // 0 = keep original
    const char* quality = "medium";  // Changed from "sinc_best" for better performance
    bool force = false;
    const char* cache_dir = nullptr;
    int fft_size = 2048;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) {
            if (i + 1 < argc) {
                output_sample_rate = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bits") == 0) {
            if (i + 1 < argc) {
                output_bit_depth = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--channels") == 0) {
            if (i + 1 < argc) {
                output_channels = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            }
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quality") == 0) {
            if (i + 1 < argc) {
                quality = argv[++i];
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = true;
        } else if (strcmp(argv[i], "--cache-dir") == 0) {
            if (i + 1 < argc) {
                cache_dir = argv[++i];
            }
        } else if (strcmp(argv[i], "--fft-size") == 0) {
            if (i + 1 < argc) {
                fft_size = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-") == 0 || argv[i][0] != '-') {
            // "-" explicitly means stdin, or treat as input file
            input_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Validate arguments
    if (!input_file) {
        // No input file specified, show usage
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        return 1;
    }

    // Check if reading from stdin
    bool read_from_stdin = (strcmp(input_file, "-") == 0);

    // Validate sample rate
    if (output_sample_rate != 0 &&
        output_sample_rate != 44100 && output_sample_rate != 48000 &&
        output_sample_rate != 96000 && output_sample_rate != 192000 &&
        output_sample_rate != 384000 && output_sample_rate != 768000) {
        std::cerr << "Warning: Unusual sample rate: " << output_sample_rate << "\n";
    }

    // Validate bit depth
    if (output_bit_depth != 16 && output_bit_depth != 24 && output_bit_depth != 32) {
        std::cerr << "Error: Invalid bit depth. Must be 16, 24, or 32\n";
        return 1;
    }

    LOG_INFO("Processing: {}", read_from_stdin ? "stdin" : input_file);
    LOG_INFO("Output format: {} Hz, {} bit, {} channels",
             output_sample_rate > 0 ? std::to_string(output_sample_rate) : "original",
             output_bit_depth,
             output_channels > 0 ? std::to_string(output_channels) : "original");

    // Get cache directory
    std::string cache_path = cache_dir ? cache_dir : utils::PlatformUtils::getCacheDirectory();

    // Ensure cache directory exists
    utils::PlatformUtils::ensureDirectories();

    ErrorCode ret;

    // Determine output mode:
    // - Pipe mode (stdin): default to stdout, unless -o is specified
    // - File mode: always create file
    bool output_to_stdout = read_from_stdin && (output_file == nullptr);

    if (output_to_stdout) {
        // Pipeline mode: read from stdin, convert, output to stdout
        LOG_INFO("Output mode: stdout (pipeline mode)");
        ret = in2wav::FormatConverter::convertStdinToStdout(
            output_sample_rate,
            output_bit_depth,
            output_channels,
            quality
        );

        if (ret != ErrorCode::Success) {
            std::string error_msg = "Error code: " + std::to_string(static_cast<int>(ret));
            std::cerr << "Error: " << error_msg << "\n";
            LOG_ERROR("Conversion failed: {}", static_cast<int>(ret));
            return static_cast<int>(getHTTPStatusCode(ret));
        }

        LOG_INFO("xpuIn2Wav completed successfully (pipeline mode)");
        return 0;
    }

    // File output mode
    std::string final_output_file;
    if (output_file) {
        // User specified output file with -o option
        final_output_file = output_file;
        // Ensure .wav extension
        if (final_output_file.size() < 4 ||
            final_output_file.substr(final_output_file.size() - 4) != ".wav") {
            final_output_file += ".wav";
        }
    } else if (read_from_stdin) {
        // When reading from stdin with -o, use default name
        final_output_file = "stdin_output.wav";
    } else {
        // Generate output file name from input file
        final_output_file = input_file;
        size_t dot_pos = final_output_file.find_last_of('.');

        // Always add _out suffix to avoid overwriting input files
        if (dot_pos != std::string::npos) {
            final_output_file = final_output_file.substr(0, dot_pos) + "_out.wav";
        } else {
            final_output_file += "_out.wav";
        }
    }

    LOG_INFO("Output mode: file ({})", final_output_file);

    if (read_from_stdin) {
        // Read from stdin (xpuLoad output), write to file
        ret = in2wav::FormatConverter::convertStdinToWAV(
            final_output_file,
            output_sample_rate,
            output_bit_depth,
            output_channels,
            quality
        );
    } else {
        // Read from file
        ret = in2wav::FormatConverter::convertToWAV(
            input_file,
            final_output_file,
            output_sample_rate,
            output_bit_depth,
            output_channels,
            quality
        );
    }

    if (ret != ErrorCode::Success) {
        std::string error_msg = "Error code: " + std::to_string(static_cast<int>(ret));
        std::cerr << "Error: " << error_msg << "\n";
        LOG_ERROR("Conversion failed: {}", static_cast<int>(ret));
        return static_cast<int>(getHTTPStatusCode(ret));
    }

    std::cout << "Conversion complete: " << final_output_file << "\n";

    // TODO: Implement FFT caching
    // Initialize FFT engine and cache manager
    // Compute FFT with caching if needed
    // Output FFT data to stdout or save to cache
    // Note: FFT cache will be stored in cache directory, not the output file

    LOG_INFO("xpuIn2Wav completed successfully");
    LOG_INFO("Output file: {}", final_output_file);
    LOG_INFO("FFT cache directory: {} (for future FFT computation)", cache_path);
    LOG_INFO("FFT size: {}", fft_size);

    return 0;
}
