/**
 * @file FormatConverter.cpp
 * @brief Format conversion implementation
 */

#include "FormatConverter.h"
#include "../xpuLoad/AudioFileLoader.h"
#include "../xpuLoad/DSDDecoder.h"
#include "utils/Logger.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdint>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

extern "C" {
#include <samplerate.h>
}

using namespace xpu;

namespace xpu {
namespace in2wav {

/**
 * @brief Convert quality string to libsamplerate converter type
 */
static int getConverterType(const char* quality) {
    if (strcmp(quality, "best") == 0) {
        return SRC_SINC_BEST_QUALITY;
    } else if (strcmp(quality, "medium") == 0) {
        return SRC_SINC_MEDIUM_QUALITY;
    } else if (strcmp(quality, "fast") == 0) {
        return SRC_SINC_FASTEST;
    } else if (strcmp(quality, "linear") == 0) {
        return SRC_LINEAR;
    } else if (strcmp(quality, "zero") == 0) {
        return SRC_ZERO_ORDER_HOLD;
    }
    // Default to medium quality for better performance
    return SRC_SINC_MEDIUM_QUALITY;
}

// ============================================================================
// Streaming Resampler Implementation
// ============================================================================

StreamingResampler::StreamingResampler()
    : input_rate_(0)
    , output_rate_(0)
    , channels_(0)
    , ratio_(1.0)
    , src_state_(nullptr)
    , initialized_(false)
{
}

StreamingResampler::~StreamingResampler() {
    if (src_state_) {
        src_delete(src_state_);
        src_state_ = nullptr;
    }
}

ErrorCode StreamingResampler::init(int input_rate, int output_rate, int channels, const char* quality) {
    input_rate_ = input_rate;
    output_rate_ = output_rate;
    channels_ = channels;
    ratio_ = static_cast<double>(output_rate) / static_cast<double>(input_rate);

    if (input_rate == output_rate) {
        // No resampling needed
        return ErrorCode::Success;
    }

    int converter_type = getConverterType(quality);
    int error = 0;

    src_state_ = src_new(converter_type, channels_, &error);
    if (error) {
        LOG_ERROR("libsamplerate initialization error: {}", src_strerror(error));
        return ErrorCode::AudioDecodeError;
    }

    initialized_ = true;
    LOG_INFO("Streaming resampler initialized: {} Hz -> {} Hz (ratio={}, quality={})",
             input_rate_, output_rate_, ratio_, quality);

    return ErrorCode::Success;
}

ErrorCode StreamingResampler::process(const float* input, int input_frames, std::vector<float>& output) {
    if (!initialized_ || input_rate_ == output_rate_) {
        // No resampling, just copy
        output.assign(input, input + input_frames * channels_);
        return ErrorCode::Success;
    }

    // Calculate output buffer size (with some headroom)
    int output_frames = static_cast<int>(input_frames * ratio_) + 256;
    output.resize(output_frames * channels_);

    SRC_DATA src_data;
    std::memset(&src_data, 0, sizeof(SRC_DATA));

    src_data.data_in = const_cast<float*>(input);
    src_data.input_frames = input_frames;
    src_data.data_out = output.data();
    src_data.output_frames = output_frames;
    src_data.src_ratio = ratio_;

    int error = src_process(src_state_, &src_data);
    if (error) {
        LOG_ERROR("libsamplerate error: {}", src_strerror(error));
        return ErrorCode::AudioDecodeError;
    }

    // Resize to actual output
    output.resize(src_data.output_frames_gen * channels_);

    return ErrorCode::Success;
}

ErrorCode StreamingResampler::flush(std::vector<float>& output) {
    if (!initialized_) {
        output.clear();
        return ErrorCode::Success;
    }

    // Flush the resampler
    std::vector<float> dummy_input(1);
    SRC_DATA src_data;
    std::memset(&src_data, 0, sizeof(SRC_DATA));

    src_data.data_in = dummy_input.data();
    src_data.input_frames = 0;
    src_data.end_of_input = 1;  // Signal end of input

    // Allocate output buffer
    int max_output = 4096;
    output.resize(max_output * channels_);

    src_data.data_out = output.data();
    src_data.output_frames = max_output;
    src_data.src_ratio = ratio_;

    int error = src_process(src_state_, &src_data);
    if (error) {
        LOG_ERROR("libsamplerate flush error: {}", src_strerror(error));
        return ErrorCode::AudioDecodeError;
    }

    // Resize to actual output
    output.resize(src_data.output_frames_gen * channels_);

    return ErrorCode::Success;
}

// WAV header structure
#pragma pack(push, 1)
struct WAVHeader {
    // RIFF chunk
    char riff[4];              // "RIFF"
    uint32_t file_size;        // Total file size - 8
    char wave[4];              // "WAVE"

    // fmt chunk
    char fmt[4];               // "fmt "
    uint32_t fmt_size;         // 16 for PCM
    uint16_t audio_format;     // 1 = PCM, 3 = IEEE float
    uint16_t num_channels;     // Number of channels
    uint32_t sample_rate;      // Sample rate
    uint32_t byte_rate;        // Byte rate = sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;      // Block align = num_channels * bits_per_sample/8
    uint16_t bits_per_sample;  // Bits per sample

    // data chunk
    char data[4];              // "data"
    uint32_t data_size;        // Data size
};
#pragma pack(pop)

/**
 * @brief Create WAV header
 */
WAVHeader createWAVHeader(uint32_t data_size, int sample_rate,
                          int channels, int bits_per_sample, bool use_float = true) {
    WAVHeader header;
    std::memset(&header, 0, sizeof(WAVHeader));

    std::memcpy(header.riff, "RIFF", 4);
    std::memcpy(header.wave, "WAVE", 4);
    std::memcpy(header.fmt, "fmt ", 4);
    std::memcpy(header.data, "data", 4);

    header.fmt_size = 16;
    header.audio_format = use_float ? static_cast<uint16_t>(3) : static_cast<uint16_t>(1);  // 3 = IEEE float, 1 = PCM
    header.num_channels = static_cast<uint16_t>(channels);
    header.sample_rate = static_cast<uint32_t>(sample_rate);
    header.bits_per_sample = static_cast<uint16_t>(bits_per_sample);
    header.block_align = static_cast<uint16_t>(channels * (bits_per_sample / 8));
    header.byte_rate = static_cast<uint32_t>(sample_rate * header.block_align);
    header.data_size = static_cast<uint32_t>(data_size);
    header.file_size = 36 + static_cast<uint32_t>(data_size);

    return header;
}

/**
 * @brief Read line from stdin
 */
static std::string readStdinLine() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

/**
 * @brief Parse JSON metadata from xpuLoad output
 */
static bool parseMetadataFromStdin(protocol::AudioMetadata& metadata) {
    // Read JSON metadata character by character until we hit the binary size header
    std::string json_str;
    char c;

    // Read until we find the end of JSON (closing brace)
    bool in_json = false;
    int brace_count = 0;

    while (std::cin.get(c)) {
        json_str += c;

        if (c == '{') {
            in_json = true;
            brace_count++;
        } else if (c == '}') {
            brace_count--;
            if (in_json && brace_count == 0) {
                // Found end of JSON
                break;
            }
        }
    }

    // For now, we'll skip the JSON parsing and use the binary size header
    // In a full implementation, we would parse the JSON here
    LOG_INFO("Received metadata from xpuLoad ({} bytes)", json_str.size());

    return true;
}

ErrorCode FormatConverter::convertStdinToWAV(const std::string& output_file,
                                            int sample_rate,
                                            int bit_depth,
                                            int channels,
                                            const char* quality) {
    LOG_INFO("Converting stdin to WAV");
    LOG_INFO("  Target sample rate: {}", sample_rate);
    LOG_INFO("  Quality: {}", quality);
    LOG_INFO("  Target bit depth: {}", bit_depth);
    LOG_INFO("  Target channels: {}", channels);

    // Set stdin to binary mode
    #ifdef PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    #endif

    // Read and skip JSON metadata
    // xpuLoad outputs: [JSON metadata][8-byte size header][PCM data]
    // JSON metadata ends with "}\n", then immediately followed by 8-byte size header

    std::string json_str;
    char c;
    bool json_complete = false;

    // Read until we find the end of JSON (closing brace + newline)
    int brace_count = 0;
    bool in_json = false;
    int max_json_size = 100000;  // Safety limit
    int json_bytes = 0;

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
        LOG_ERROR("Failed to read complete JSON metadata from stdin (read {} bytes)", json_bytes);
        LOG_ERROR("JSON content so far: {}", json_str);
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("JSON metadata received: {} bytes", json_str.size());

    // Now read the 8-byte size header
    uint64_t data_size = 0;
    char size_buffer[8];
    if (!std::cin.read(size_buffer, 8)) {
        LOG_ERROR("Failed to read size header from stdin");
        return ErrorCode::InvalidOperation;
    }

    // Copy bytes to uint64_t (little-endian)
    std::memcpy(&data_size, size_buffer, 8);

    if (data_size == 0) {
        LOG_ERROR("Invalid data size: 0");
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("PCM data size from stdin: {} bytes ({} samples)",
             data_size, data_size / sizeof(float));

    // Read PCM data
    std::vector<uint8_t> pcm_data(data_size);
    std::cin.read(reinterpret_cast<char*>(pcm_data.data()), data_size);

    if (!std::cin) {
        LOG_ERROR("Failed to read PCM data from stdin");
        return ErrorCode::InvalidOperation;
    }

    // Convert to float
    size_t sample_count = pcm_data.size() / sizeof(float);
    const float* input_samples = reinterpret_cast<const float*>(pcm_data.data());
    std::vector<float> audio_buffer(input_samples, input_samples + sample_count);

    // Get actual sample rate from metadata (assuming 48000 if not specified)
    int actual_sample_rate = 48000;  // Default fallback
    // TODO: Parse from JSON metadata
    int current_sample_rate = actual_sample_rate;

    // Resample if needed
    if (sample_rate > 0 && current_sample_rate != sample_rate) {
        LOG_INFO("Resampling from {} Hz to {} Hz", current_sample_rate, sample_rate);
        std::vector<float> resampled;
        ErrorCode ret = resample(audio_buffer, current_sample_rate, sample_rate, resampled, quality);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Resampling failed: {}", static_cast<int>(ret));
            return ret;
        }
        audio_buffer = std::move(resampled);
        current_sample_rate = sample_rate;
    }

    // Channel configuration
    int input_channels = 2;  // Assume stereo
    if (channels > 0 && channels != input_channels) {
        LOG_INFO("Converting channels: {} -> {}", input_channels, channels);
        std::vector<float> remixed;

        if (channels < input_channels) {
            // Downmix: take first N channels
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * channels);

            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    remixed[i * channels + ch] = audio_buffer[i * input_channels + ch];
                }
            }
        } else {
            // Upmix: duplicate channels
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * channels);

            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    int src_ch = (ch < input_channels) ? ch : 0;
                    remixed[i * channels + ch] = audio_buffer[i * input_channels + src_ch];
                }
            }
        }

        audio_buffer = std::move(remixed);
    }

    // Convert bit depth if needed
    std::vector<uint8_t> output_data;
    ErrorCode ret;

    if (bit_depth != 32) {
        ret = convertBitDepth(audio_buffer, 32, bit_depth, output_data);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Bit depth conversion failed: {}", static_cast<int>(ret));
            return ret;
        }
    } else {
        // Keep as 32-bit float
        size_t byte_count = audio_buffer.size() * sizeof(float);
        output_data.resize(byte_count);
        std::memcpy(output_data.data(), audio_buffer.data(), byte_count);
    }

    // Create WAV file
    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        LOG_ERROR("Failed to create output file: {}", output_file);
        return ErrorCode::FileWriteError;
    }

    // Write WAV header
    bool use_float = (bit_depth == 32);
    WAVHeader header = createWAVHeader(output_data.size(), current_sample_rate,
                                       channels > 0 ? channels : 2, bit_depth, use_float);
    out.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));

    // Write audio data
    out.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());

    out.close();

    LOG_INFO("WAV file created: {}", output_file);
    LOG_INFO("  Size: {} bytes", output_data.size() + sizeof(WAVHeader));

    return ErrorCode::Success;
}

ErrorCode FormatConverter::convertToWAV(const std::string& input_file,
                                          const std::string& output_file,
                                          int sample_rate,
                                          int bit_depth,
                                          int channels,
                                          const char* quality) {
    LOG_INFO("Converting {} to WAV", input_file);
    LOG_INFO("  Target sample rate: {}", sample_rate);
    LOG_INFO("  Target bit depth: {}", bit_depth);
    LOG_INFO("  Target channels: {}", channels);
    LOG_INFO("  Quality: {}", quality);

    // Load audio file
    bool is_dsd = false;
    if (input_file.size() > 4) {
        std::string ext = input_file.substr(input_file.size() - 4);
        if (ext == ".dsf" || ext == ".dff") {
            is_dsd = true;
        }
    }

    ErrorCode ret;
    protocol::AudioMetadata metadata;
    std::vector<uint8_t> pcm_data_copy;  // Store a copy of PCM data

    if (is_dsd) {
        load::DSDDecoder decoder;
        // Keep original sample rate for xpuIn2Wav
        decoder.setTargetSampleRate(sample_rate > 0 ? sample_rate : 0);  // 0 = keep original
        ret = decoder.load(input_file);
        if (ret == ErrorCode::Success) {
            metadata = decoder.getMetadata();
            // Copy PCM data before decoder is destroyed
            pcm_data_copy = decoder.getPCMData();
        }
    } else {
        load::AudioFileLoader loader;
        // Keep original sample rate for xpuIn2Wav (don't convert to 48000)
        // Only convert if user explicitly requested a different sample rate
        int target_rate = (sample_rate > 0) ? sample_rate : 0;
        loader.setTargetSampleRate(target_rate);
        ret = loader.load(input_file);
        if (ret == ErrorCode::Success) {
            metadata = loader.getMetadata();
            // Copy PCM data before loader is destroyed
            pcm_data_copy = loader.getPCMData();
        }
    }

    if (ret != ErrorCode::Success) {
        LOG_ERROR("Failed to load input file: {}", static_cast<int>(ret));
        return ret;
    }

    // Debug: log PCM data size
    LOG_INFO("PCM data pointer: {}, size: {} bytes ({} samples)",
             static_cast<const void*>(pcm_data_copy.data()),
             pcm_data_copy.size(), pcm_data_copy.size() / sizeof(float));

    // Convert PCM data to float
    size_t sample_count = pcm_data_copy.size() / sizeof(float);
    const float* input_samples = reinterpret_cast<const float*>(pcm_data_copy.data());

    std::vector<float> audio_buffer(input_samples, input_samples + sample_count);

    // Determine if we need to resample
    // Use original_sample_rate if available (xpuLoad output), otherwise use current sample_rate
    int current_sample_rate = metadata.original_sample_rate > 0 ? metadata.original_sample_rate : metadata.sample_rate;

    // Resample if needed
    if (sample_rate > 0 && current_sample_rate != sample_rate) {
        LOG_INFO("Resampling from {} Hz to {} Hz", current_sample_rate, sample_rate);
        std::vector<float> resampled;
        ret = resample(audio_buffer, current_sample_rate, sample_rate, resampled, quality);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Resampling failed: {}", static_cast<int>(ret));
            return ret;
        }
        audio_buffer = std::move(resampled);
        metadata.sample_rate = sample_rate;
    }

    // Channel configuration (simplified - just take first N channels)
    int input_channels = metadata.channels;
    if (channels > 0 && channels != input_channels) {
        LOG_INFO("Converting channels: {} -> {}", input_channels, channels);
        std::vector<float> remixed;

        if (channels < input_channels) {
            // Downmix: take first N channels
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * channels);

            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    remixed[i * channels + ch] = audio_buffer[i * input_channels + ch];
                }
            }
        } else {
            // Upmix: duplicate channels
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * channels);

            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    int src_ch = (ch < input_channels) ? ch : 0;
                    remixed[i * channels + ch] = audio_buffer[i * input_channels + src_ch];
                }
            }
        }

        audio_buffer = std::move(remixed);
        metadata.channels = channels;
    }

    // Convert bit depth if needed
    std::vector<uint8_t> output_data;
    if (bit_depth != 32) {
        ret = convertBitDepth(audio_buffer, 32, bit_depth, output_data);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Bit depth conversion failed: {}", static_cast<int>(ret));
            return ret;
        }
    } else {
        // Keep as 32-bit float
        size_t byte_count = audio_buffer.size() * sizeof(float);
        output_data.resize(byte_count);
        std::memcpy(output_data.data(), audio_buffer.data(), byte_count);
    }

    // Create WAV file
    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        LOG_ERROR("Failed to create output file: {}", output_file);
        return ErrorCode::FileWriteError;
    }

    // Write WAV header
    bool use_float = (bit_depth == 32);
    WAVHeader header = createWAVHeader(output_data.size(), metadata.sample_rate,
                                       metadata.channels, bit_depth, use_float);
    out.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));

    // Write audio data
    out.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());

    out.close();

    LOG_INFO("WAV file created: {}", output_file);
    LOG_INFO("  Size: {} bytes", output_data.size() + sizeof(WAVHeader));

    return ErrorCode::Success;
}

ErrorCode FormatConverter::resample(const std::vector<float>& input,
                                     int input_rate,
                                     int output_rate,
                                     std::vector<float>& output,
                                     const char* quality) {
    if (input_rate == output_rate) {
        output = input;
        return ErrorCode::Success;
    }

    // Determine channels (assume stereo for now)
    int channels = 2;
    size_t input_frames = input.size() / channels;

    // Calculate output frames
    double ratio = static_cast<double>(output_rate) / static_cast<double>(input_rate);
    size_t output_frames = static_cast<size_t>(input_frames * ratio) + 1;

    // Setup libsamplerate
    SRC_DATA src_data;
    std::memset(&src_data, 0, sizeof(SRC_DATA));

    src_data.data_in = const_cast<float*>(input.data());
    src_data.input_frames = input_frames;
    src_data.output_frames = output_frames;
    src_data.src_ratio = ratio;

    output.resize(output_frames * channels);
    src_data.data_out = output.data();

    // Get converter type from quality string
    int converter_type = getConverterType(quality);
    const char* quality_name = (converter_type == SRC_SINC_BEST_QUALITY) ? "best" :
                               (converter_type == SRC_SINC_MEDIUM_QUALITY) ? "medium" :
                               (converter_type == SRC_SINC_FASTEST) ? "fast" : "unknown";

    LOG_INFO("Resampling quality: {}", quality_name);

    int error = 0;
    SRC_STATE* src_state = src_new(converter_type, channels, &error);
    if (error) {
        LOG_ERROR("libsamplerate initialization error: {}", src_strerror(error));
        return ErrorCode::AudioDecodeError;
    }

    error = src_process(src_state, &src_data);
    src_delete(src_state);

    if (error) {
        LOG_ERROR("libsamplerate error: {}", src_strerror(error));
        return ErrorCode::AudioDecodeError;
    }

    output.resize(src_data.output_frames_gen * channels);

    LOG_INFO("Resampled: {} frames -> {} frames", input_frames, src_data.output_frames_gen);

    return ErrorCode::Success;
}

ErrorCode FormatConverter::convertBitDepth(const std::vector<float>& input,
                                            int input_bits,
                                            int output_bits,
                                            std::vector<uint8_t>& output) {
    if (input_bits != 32) {
        LOG_ERROR("Only 32-bit float input is supported");
        return ErrorCode::InvalidOperation;
    }

    switch (output_bits) {
        case 16: {
            // Convert to 16-bit PCM
            size_t sample_count = input.size();
            output.resize(sample_count * sizeof(int16_t));

            int16_t* out_samples = reinterpret_cast<int16_t*>(output.data());

            for (size_t i = 0; i < sample_count; ++i) {
                // Clamp to [-1.0, 1.0]
                float sample = std::max(-1.0f, std::min(1.0f, input[i]));

                // Convert to 16-bit integer
                if (sample < 0.0f) {
                    out_samples[i] = static_cast<int16_t>(sample * 32768.0f);
                } else {
                    out_samples[i] = static_cast<int16_t>(sample * 32767.0f);
                }
            }
            break;
        }

        case 24: {
            // Convert to 24-bit PCM (packed in 3 bytes)
            size_t sample_count = input.size();
            output.resize(sample_count * 3);

            uint8_t* out_bytes = output.data();

            for (size_t i = 0; i < sample_count; ++i) {
                // Clamp to [-1.0, 1.0]
                float sample = std::max(-1.0f, std::min(1.0f, input[i]));

                // Convert to 24-bit integer
                int32_t sample_24bit;
                if (sample < 0.0f) {
                    sample_24bit = static_cast<int32_t>(sample * 8388608.0f);  // 2^23
                } else {
                    sample_24bit = static_cast<int32_t>(sample * 8388607.0f);
                }

                // Pack as little-endian 3 bytes
                out_bytes[i * 3 + 0] = static_cast<uint8_t>(sample_24bit & 0xFF);
                out_bytes[i * 3 + 1] = static_cast<uint8_t>((sample_24bit >> 8) & 0xFF);
                out_bytes[i * 3 + 2] = static_cast<uint8_t>((sample_24bit >> 16) & 0xFF);
            }
            break;
        }

        case 32: {
            // Keep as 32-bit float
            size_t byte_count = input.size() * sizeof(float);
            output.resize(byte_count);
            std::memcpy(output.data(), input.data(), byte_count);
            break;
        }

        default:
            LOG_ERROR("Unsupported output bit depth: {}", output_bits);
            return ErrorCode::InvalidOperation;
    }

    LOG_INFO("Bit depth converted: {} -> {}", input_bits, output_bits);

    return ErrorCode::Success;
}

ErrorCode FormatConverter::convertStdinToStdout(int sample_rate,
                                                 int bit_depth,
                                                 int channels,
                                                 const char* quality) {
    LOG_INFO("Converting stdin to stdout (pipeline mode)");
    LOG_INFO("  Target sample rate: {}", sample_rate);
    LOG_INFO("  Target bit depth: {}", bit_depth);
    LOG_INFO("  Target channels: {}", channels);
    LOG_INFO("  Quality: {}", quality);

    // Set stdin/stdout to binary mode
    #ifdef PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    #endif

    // Read and parse JSON metadata from xpuLoad
    std::string json_str;
    char c;
    bool json_complete = false;
    int brace_count = 0;
    bool in_json = false;
    int max_json_size = 100000;
    int json_bytes = 0;

    while (std::cin.get(c) && json_bytes < max_json_size) {
        json_str += c;
        json_bytes++;

        if (c == '{') {
            in_json = true;
            brace_count++;
        } else if (c == '}') {
            brace_count--;
            if (in_json && brace_count == 0) {
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
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("JSON metadata received: {} bytes", json_str.size());

    // Parse sample rate and channels from JSON
    int input_sample_rate = 48000;
    int input_channels = 2;

    size_t sr_pos = json_str.find("\"sample_rate\":");
    if (sr_pos != std::string::npos) {
        size_t value_start = json_str.find(":", sr_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string sr_str = json_str.substr(value_start, value_end - value_start);
        size_t start = sr_str.find_first_not_of(" \t\n");
        size_t end = sr_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            sr_str = sr_str.substr(start, end - start + 1);
            input_sample_rate = std::atoi(sr_str.c_str());
        }
    }

    size_t ch_pos = json_str.find("\"channels\":");
    if (ch_pos != std::string::npos) {
        size_t value_start = json_str.find(":", ch_pos) + 1;
        size_t value_end = json_str.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_str.find("}", value_start);
        }
        std::string ch_str = json_str.substr(value_start, value_end - value_start);
        size_t start = ch_str.find_first_not_of(" \t\n");
        size_t end = ch_str.find_last_not_of(" \t\n");
        if (start != std::string::npos && end != std::string::npos) {
            ch_str = ch_str.substr(start, end - start + 1);
            input_channels = std::atoi(ch_str.c_str());
        }
    }

    LOG_INFO("Input format: {} Hz, {} channels", input_sample_rate, input_channels);

    // Read 8-byte size header
    uint64_t data_size = 0;
    char size_buffer[8];
    if (!std::cin.read(size_buffer, 8)) {
        LOG_ERROR("Failed to read size header from stdin");
        return ErrorCode::InvalidOperation;
    }
    std::memcpy(&data_size, size_buffer, 8);

    if (data_size == 0) {
        LOG_ERROR("Invalid data size: 0");
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("PCM data size: {} bytes ({} samples)", data_size, data_size / sizeof(float));

    // Read PCM data
    std::vector<uint8_t> pcm_data(data_size);
    std::cin.read(reinterpret_cast<char*>(pcm_data.data()), data_size);

    if (!std::cin) {
        LOG_ERROR("Failed to read PCM data from stdin");
        return ErrorCode::InvalidOperation;
    }

    // Convert to float vector
    size_t sample_count = pcm_data.size() / sizeof(float);
    const float* input_samples = reinterpret_cast<const float*>(pcm_data.data());
    std::vector<float> audio_buffer(input_samples, input_samples + sample_count);

    // Determine output parameters
    int output_sample_rate = (sample_rate > 0) ? sample_rate : input_sample_rate;
    int output_channels = (channels > 0) ? channels : input_channels;
    int output_bit_depth = bit_depth;

    // Resample if needed
    if (output_sample_rate != input_sample_rate) {
        LOG_INFO("Resampling: {} Hz -> {} Hz", input_sample_rate, output_sample_rate);
        std::vector<float> resampled;
        ErrorCode ret = resample(audio_buffer, input_sample_rate, output_sample_rate, resampled, quality);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Resampling failed: {}", static_cast<int>(ret));
            return ret;
        }
        audio_buffer = std::move(resampled);
    }

    // Convert channels if needed
    if (output_channels != input_channels) {
        LOG_INFO("Converting channels: {} -> {}", input_channels, output_channels);
        std::vector<float> remixed;

        if (output_channels < input_channels) {
            // Downmix
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * output_channels);
            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < output_channels; ++ch) {
                    remixed[i * output_channels + ch] = audio_buffer[i * input_channels + ch];
                }
            }
        } else {
            // Upmix
            size_t frames = audio_buffer.size() / input_channels;
            remixed.resize(frames * output_channels);
            for (size_t i = 0; i < frames; ++i) {
                for (int ch = 0; ch < output_channels; ++ch) {
                    int src_ch = (ch < input_channels) ? ch : 0;
                    remixed[i * output_channels + ch] = audio_buffer[i * input_channels + src_ch];
                }
            }
        }
        audio_buffer = std::move(remixed);
    }

    // Convert bit depth if needed (output 32-bit float for xpuPlay)
    std::vector<uint8_t> output_data;
    if (output_bit_depth != 32) {
        ErrorCode ret = convertBitDepth(audio_buffer, 32, output_bit_depth, output_data);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Bit depth conversion failed: {}", static_cast<int>(ret));
            return ret;
        }
    } else {
        // Keep as 32-bit float
        size_t byte_count = audio_buffer.size() * sizeof(float);
        output_data.resize(byte_count);
        std::memcpy(output_data.data(), audio_buffer.data(), byte_count);
    }

    // Create new metadata for output
    protocol::AudioMetadata output_metadata;
    output_metadata.sample_rate = output_sample_rate;
    output_metadata.original_sample_rate = input_sample_rate;
    output_metadata.channels = output_channels;
    output_metadata.bit_depth = output_bit_depth;
    output_metadata.original_bit_depth = 32;
    output_metadata.sample_count = output_data.size() / (output_bit_depth / 8);
    output_metadata.is_lossless = true;

    // Calculate duration
    size_t total_samples = output_metadata.sample_count / output_metadata.channels;
    output_metadata.duration = static_cast<double>(total_samples) / output_sample_rate;

    // Generate JSON metadata
    std::ostringstream json;
    json << "{\n";
    json << "  \"success\": true,\n";
    json << "  \"metadata\": {\n";
    json << "    \"file_path\": \"stdin\",\n";
    json << "    \"format\": \"PCM\",\n";
    json << "    \"sample_rate\": " << output_metadata.sample_rate << ",\n";
    json << "    \"original_sample_rate\": " << output_metadata.original_sample_rate << ",\n";
    json << "    \"channels\": " << output_metadata.channels << ",\n";
    json << "    \"bit_depth\": " << output_metadata.bit_depth << ",\n";
    json << "    \"original_bit_depth\": " << output_metadata.original_bit_depth << ",\n";
    json << "    \"sample_count\": " << output_metadata.sample_count << ",\n";
    json << "    \"duration\": " << output_metadata.duration << ",\n";
    json << "    \"is_lossless\": true\n";
    json << "  }\n";
    json << "}\n";

    // Output to stdout: [JSON metadata][8-byte size header][PCM data]
    std::cout << json.str();
    std::cout.flush();

    uint64_t output_size = output_data.size();
    std::cout.write(reinterpret_cast<const char*>(&output_size), sizeof(output_size));
    std::cout.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
    std::cout.flush();

    LOG_INFO("Conversion complete: {} samples, {} bytes output to stdout",
             sample_count, output_data.size());

    return ErrorCode::Success;
}

} // namespace in2wav
} // namespace xpu
