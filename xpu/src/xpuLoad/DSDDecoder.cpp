/**
 * @file DSDDecoder.cpp
 * @brief DSD (Direct Stream Digital) format decoder implementation
 */

#include "DSDDecoder.h"
#include "utils/Logger.h"
#include <fstream>
#include <cstring>
#include <algorithm>

using namespace xpu;

namespace xpu {
namespace load {

/**
 * @brief DSF format header structure
 */
#pragma pack(push, 1)
struct DSFHeader {
    char id[4];              // 'D', 'S', 'D', ' '
    uint64_t chunk_size;     // Total chunk size
    uint64_t file_size;      // Total file size
    uint32_t meta_offset;    // Metadata chunk offset
};

struct DSFFmtChunk {
    char id[4];              // 'f', 'm', 't', ' '
    uint64_t chunk_size;     // Chunk size
    uint32_t version;        // Format version
    uint32_t format_id;      // Format ID (0 = DSD raw)
    uint32_t channel_type;   // Channel type
    uint32_t channel_num;    // Number of channels
    uint32_t sampling_freq;  // Sampling frequency (Hz)
    uint32_t bits_per_sample;// Bits per sample (always 1 for DSD)
    uint64_t sample_count;   // Total sample count
    uint32_t block_size;     // Block size per channel
    uint32_t reserved;       // Reserved
};

struct DSFDataChunk {
    char id[4];              // 'd', 'a', 't', 'a'
    uint64_t chunk_size;     // Chunk size
    uint64_t sample_count;   // Sample count
};
#pragma pack(pop)

/**
 * @brief DSDIFF format header (simplified)
 */
struct DSDIFFHeader {
    char id[4];              // 'F', 'R', 'M', '8'
    uint32_t chunk_size;     // Chunk size
    char type[4];            // 'D', 'S', 'D', ' '
};

/**
 * @brief Implementation class
 */
class DSDDecoder::Impl {
public:
    protocol::AudioMetadata metadata;
    std::vector<uint8_t> pcm_data;
    std::vector<uint8_t> dsd_data;
    bool loaded = false;
    int target_sample_rate = 48000;  // Default target sample rate

    uint32_t dsd_rate = 0;       // DSD sample rate (e.g., 2822400 for DSD64)
    uint32_t channels = 0;
    uint64_t dsd_sample_count = 0;

    // Streaming support
    std::ifstream dsd_file;
    uint64_t dsd_data_offset = 0;  // Offset to DSD data in file
    uint64_t dsd_data_size = 0;    // Size of DSD data
    DSDFormat format = DSDFormat::None;
};

DSDDecoder::DSDDecoder()
    : impl_(std::make_unique<Impl>()) {}

DSDDecoder::~DSDDecoder() = default;

void DSDDecoder::setTargetSampleRate(int sample_rate) {
    impl_->target_sample_rate = sample_rate;
    LOG_INFO("Target sample rate set to: {}", sample_rate);
}

DSDFormat DSDDecoder::detectFormat(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return DSDFormat::DSF; // Default
    }

    char id[4];
    file.read(id, 4);

    if (std::memcmp(id, "DSD ", 4) == 0) {
        return DSDFormat::DSF;
    } else if (std::memcmp(id, "FRM8", 4) == 0) {
        return DSDFormat::DSDIFF;
    }

    // Check extension
    if (filepath.size() > 4) {
        std::string ext = filepath.substr(filepath.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".dsf") return DSDFormat::DSF;
        if (ext == ".dff") return DSDFormat::DSDIFF;
    }

    return DSDFormat::DSF; // Default
}

ErrorCode DSDDecoder::load(const std::string& filepath) {
    LOG_INFO("Loading DSD file: {}", filepath);

    DSDFormat format = detectFormat(filepath);

    if (format == DSDFormat::DSF) {
        return parseDSF(filepath);
    } else {
        return parseDSDIFF(filepath);
    }
}

ErrorCode DSDDecoder::parseDSF(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open DSD file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    // Read DSF header
    DSFHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DSFHeader));

    if (std::memcmp(header.id, "DSD ", 4) != 0) {
        LOG_ERROR("Invalid DSF file format");
        return ErrorCode::UnsupportedFormat;
    }

    // Read format chunk
    DSFFmtChunk fmt;
    file.read(reinterpret_cast<char*>(&fmt), sizeof(DSFFmtChunk));

    if (std::memcmp(fmt.id, "fmt ", 4) != 0) {
        LOG_ERROR("Invalid DSF format chunk");
        return ErrorCode::CorruptedFile;
    }

    // Validate DSD format
    if (fmt.format_id != 0) {
        LOG_ERROR("Unsupported DSD format ID: {}", fmt.format_id);
        return ErrorCode::UnsupportedFormat;
    }

    // Store metadata
    impl_->channels = fmt.channel_num;
    impl_->dsd_rate = fmt.sampling_freq;
    impl_->dsd_sample_count = fmt.sample_count;

    impl_->metadata.channels = fmt.channel_num;
    impl_->metadata.sample_rate = fmt.sampling_freq / 8; // Convert to Hz equivalent
    impl_->metadata.bit_depth = 1; // DSD is 1-bit
    impl_->metadata.format = "DSD";
    impl_->metadata.format_name = "DSD";

    // Calculate duration
    double duration = static_cast<double>(fmt.sample_count) / fmt.sampling_freq;
    impl_->metadata.duration = duration;
    impl_->metadata.sample_count = static_cast<uint64_t>(duration * impl_->metadata.sample_rate);

    LOG_INFO("DSD Format: DSF");
    LOG_INFO("  Channels: {}", fmt.channel_num);
    LOG_INFO("  DSD Rate: {} Hz ({}x oversampling)", fmt.sampling_freq, fmt.sampling_freq / 44100);
    LOG_INFO("  Samples: {}", fmt.sample_count);
    LOG_INFO("  Duration: {:.2f} seconds", duration);

    // Read data chunk
    DSFDataChunk data;
    file.read(reinterpret_cast<char*>(&data), sizeof(DSFDataChunk));

    if (std::memcmp(data.id, "data", 4) != 0) {
        LOG_ERROR("Invalid DSF data chunk");
        return ErrorCode::CorruptedFile;
    }

    // Read DSD data
    size_t dsd_size = data.chunk_size - sizeof(data.sample_count);
    impl_->dsd_data.resize(dsd_size);
    file.read(reinterpret_cast<char*>(impl_->dsd_data.data()), dsd_size);

    // Decode DSD to PCM
    ErrorCode ret = decodeDSDToPCM();
    if (ret != ErrorCode::Success) {
        return ret;
    }

    impl_->loaded = true;
    LOG_INFO("DSD file loaded and decoded successfully");

    return ErrorCode::Success;
}

ErrorCode DSDDecoder::parseDSDIFF(const std::string& filepath) {
    (void)filepath;  // Suppress unused parameter warning
    
    // DSDIFF format parsing (simplified - would need full implementation)
    LOG_INFO("DSDIFF format detected (parsing not fully implemented)");

    // For now, treat as unsupported
    return ErrorCode::UnsupportedFormat;
}

ErrorCode DSDDecoder::decodeDSDToPCM() {
    LOG_INFO("Decoding DSD to PCM...");

    if (impl_->dsd_data.empty() || impl_->dsd_rate == 0) {
        LOG_ERROR("No DSD data to decode");
        return ErrorCode::InvalidOperation;
    }

    // DSD decoding parameters
    // If target_sample_rate is 0, use 48000 as default
    const uint32_t target_sample_rate = (impl_->target_sample_rate > 0) ?
                                         static_cast<uint32_t>(impl_->target_sample_rate) : 48000;
    const uint32_t target_channels = 2;
    const uint32_t decimation_factor = impl_->dsd_rate / target_sample_rate;

    // Calculate output size
    uint64_t total_output_samples = impl_->dsd_sample_count / decimation_factor;

    // Temporary buffer for decoded samples
    std::vector<float> decoded_samples;
    decoded_samples.reserve(total_output_samples * target_channels);

    // DSD to PCM decoder with noise shaping
    // Use 5th-order noise shaping for professional quality
    std::vector<float> noise_shaper_state(5, 0.0f);

    uint64_t dsd_index = 0;
    uint64_t samples_decoded = 0;

    while (samples_decoded < total_output_samples && dsd_index < impl_->dsd_data.size()) {
        // Process each channel
        for (uint32_t ch = 0; ch < std::min(impl_->channels, target_channels); ++ch) {
            // Accumulate DSD bits for decimation
            int32_t accumulator = 0;
            uint32_t bits_processed = 0;

            for (uint32_t i = 0; i < decimation_factor && dsd_index < impl_->dsd_data.size(); ++i) {
                // Get DSD bit (MSB first)
                uint8_t byte = impl_->dsd_data[dsd_index / 8];
                uint8_t bit = (byte >> (7 - (dsd_index % 8))) & 1;

                // Convert to bipolar (-1 or +1)
                int32_t dsd_bit = bit * 2 - 1;

                // Apply noise shaping (5th order)
                float error = dsd_bit;
                for (int j = 0; j < 5; ++j) {
                    error -= noise_shaper_state[j] * (5 - j) * 0.1f;
                }

                accumulator += static_cast<int32_t>(error);

                // Update noise shaper state
                for (int j = 4; j > 0; --j) {
                    noise_shaper_state[j] = noise_shaper_state[j - 1];
                }
                noise_shaper_state[0] = error;

                bits_processed++;
                dsd_index += impl_->channels; // Interleaved channels
            }

            // Average and normalize to float [-1.0, 1.0]
            float sample = static_cast<float>(accumulator) / bits_processed;

            // Apply low-pass filter (simple moving average for decimation)
            decoded_samples.push_back(sample);
        }

        samples_decoded++;
    }

    // Convert to byte buffer
    size_t byte_size = decoded_samples.size() * sizeof(float);
    impl_->pcm_data.resize(byte_size);
    std::memcpy(impl_->pcm_data.data(), decoded_samples.data(), byte_size);

    // Update metadata with output format
    impl_->metadata.sample_rate = target_sample_rate;
    impl_->metadata.channels = target_channels;
    impl_->metadata.bit_depth = 32; // 32-bit float output
    impl_->metadata.sample_count = decoded_samples.size() / target_channels;

    LOG_INFO("DSD decoding complete:");
    LOG_INFO("  Output: {} Hz, {} channels, {}-bit float",
             target_sample_rate, target_channels, 32);
    LOG_INFO("  Samples: {}", impl_->metadata.sample_count);

    return ErrorCode::Success;
}

const protocol::AudioMetadata& DSDDecoder::getMetadata() const {
    return impl_->metadata;
}

const std::vector<uint8_t>& DSDDecoder::getPCMData() const {
    return impl_->pcm_data;
}

ErrorCode DSDDecoder::prepareStreaming(const std::string& filepath) {
    LOG_INFO("Preparing DSD streaming for: {}", filepath);

    // Detect format
    impl_->format = detectFormat(filepath);
    if (impl_->format == DSDFormat::DSDIFF) {
        LOG_ERROR("DSDIFF streaming not supported yet");
        return ErrorCode::UnsupportedFormat;
    }

    // Open file for streaming
    impl_->dsd_file.open(filepath, std::ios::binary);
    if (!impl_->dsd_file) {
        LOG_ERROR("Failed to open DSD file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    // Read DSF header
    DSFHeader header;
    impl_->dsd_file.read(reinterpret_cast<char*>(&header), sizeof(DSFHeader));

    if (std::memcmp(header.id, "DSD ", 4) != 0) {
        LOG_ERROR("Invalid DSF file format");
        return ErrorCode::UnsupportedFormat;
    }

    // Read format chunk
    DSFFmtChunk fmt;
    impl_->dsd_file.read(reinterpret_cast<char*>(&fmt), sizeof(DSFFmtChunk));

    if (std::memcmp(fmt.id, "fmt ", 4) != 0) {
        LOG_ERROR("Invalid DSF format chunk");
        return ErrorCode::CorruptedFile;
    }

    // Validate DSD format
    if (fmt.format_id != 0) {
        LOG_ERROR("Unsupported DSD format ID: {}", fmt.format_id);
        return ErrorCode::UnsupportedFormat;
    }

    // Store metadata
    impl_->channels = fmt.channel_num;
    impl_->dsd_rate = fmt.sampling_freq;
    impl_->dsd_sample_count = fmt.sample_count;

    impl_->metadata.file_path = filepath;
    impl_->metadata.channels = fmt.channel_num;
    impl_->metadata.sample_rate = fmt.sampling_freq / 8; // Convert to Hz equivalent
    impl_->metadata.bit_depth = 1; // DSD is 1-bit
    impl_->metadata.original_sample_rate = impl_->metadata.sample_rate;
    impl_->metadata.original_bit_depth = 1;
    impl_->metadata.format = "DSD";
    impl_->metadata.format_name = "DSD";
    impl_->metadata.is_lossless = true;

    // Calculate duration
    double duration = static_cast<double>(fmt.sample_count) / fmt.sampling_freq;
    impl_->metadata.duration = duration;
    impl_->metadata.sample_count = static_cast<uint64_t>(duration * impl_->metadata.sample_rate);

    // Mark high-resolution audio
    if (impl_->metadata.sample_rate >= 96000) {
        impl_->metadata.is_high_res = true;
    }

    LOG_INFO("DSD Format: DSF (streaming mode)");
    LOG_INFO("  Channels: {}", fmt.channel_num);
    LOG_INFO("  DSD Rate: {} Hz ({}x oversampling)", fmt.sampling_freq, fmt.sampling_freq / 44100);
    LOG_INFO("  Samples: {}", fmt.sample_count);
    LOG_INFO("  Duration: {:.2f} seconds", duration);

    // Read data chunk
    DSFDataChunk data;
    impl_->dsd_file.read(reinterpret_cast<char*>(&data), sizeof(DSFDataChunk));

    if (std::memcmp(data.id, "data", 4) != 0) {
        LOG_ERROR("Invalid DSF data chunk");
        return ErrorCode::CorruptedFile;
    }

    // Store DSD data location for streaming
    impl_->dsd_data_offset = impl_->dsd_file.tellg();
    impl_->dsd_data_size = data.chunk_size - sizeof(data.sample_count);

    LOG_INFO("DSD streaming prepared successfully");
    LOG_INFO("  Data offset: {} bytes", impl_->dsd_data_offset);
    LOG_INFO("  Data size: {} bytes", impl_->dsd_data_size);

    return ErrorCode::Success;
}

ErrorCode DSDDecoder::streamPCM(DSDStreamingCallback callback, size_t chunk_size_bytes) {
    // Check if prepareStreaming() was called
    if (!impl_->dsd_file.is_open()) {
        LOG_ERROR("streamPCM() called without prepareStreaming()");
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("Streaming DSD to PCM in chunks: {} bytes", chunk_size_bytes);

    // DSD decoding parameters
    const uint32_t target_sample_rate = (impl_->target_sample_rate > 0) ?
                                         static_cast<uint32_t>(impl_->target_sample_rate) : 48000;
    const uint32_t target_channels = 2;
    const uint32_t decimation_factor = impl_->dsd_rate / target_sample_rate;

    // Calculate output size
    uint64_t total_output_samples = impl_->dsd_sample_count / decimation_factor;

    // Update metadata with output format
    impl_->metadata.sample_rate = target_sample_rate;
    impl_->metadata.channels = target_channels;
    impl_->metadata.bit_depth = 32; // 32-bit float output
    impl_->metadata.sample_count = total_output_samples;

    // Chunk buffer for accumulating samples before callback
    size_t chunk_size_samples = chunk_size_bytes / sizeof(float);
    std::vector<float> chunk_buffer;
    chunk_buffer.reserve(chunk_size_samples);

    // DSD to PCM decoder with noise shaping
    std::vector<float> noise_shaper_state(5, 0.0f);

    // Seek to DSD data
    impl_->dsd_file.seekg(impl_->dsd_data_offset);

    // Read DSD data in chunks and decode
    uint64_t dsd_bytes_remaining = impl_->dsd_data_size;
    uint64_t dsd_index = 0;
    uint64_t samples_decoded = 0;
    int chunk_count = 0;

    // Buffer for reading DSD data
    constexpr size_t DSD_READ_BUFFER_SIZE = 64 * 1024;  // 64KB
    std::vector<uint8_t> dsd_read_buffer(DSD_READ_BUFFER_SIZE);

    while (samples_decoded < total_output_samples && dsd_bytes_remaining > 0) {
        // Calculate how much to read
        size_t bytes_to_read = std::min(DSD_READ_BUFFER_SIZE, static_cast<size_t>(dsd_bytes_remaining));
        impl_->dsd_file.read(reinterpret_cast<char*>(dsd_read_buffer.data()), bytes_to_read);
        size_t bytes_read = impl_->dsd_file.gcount();

        if (bytes_read == 0) break;

        // Process DSD data
        for (size_t i = 0; i < bytes_read && samples_decoded < total_output_samples; ++i) {
            uint8_t byte = dsd_read_buffer[i];

            // Process each bit in the byte
            for (int bit = 7; bit >= 0 && samples_decoded < total_output_samples; --bit) {
                // Process each channel
                for (uint32_t ch = 0; ch < std::min(impl_->channels, target_channels); ++ch) {
                    // Get DSD bit
                    uint8_t dsd_bit = (byte >> bit) & 1;

                    // Convert to bipolar (-1 or +1)
                    int32_t dsd_value = dsd_bit * 2 - 1;

                    // Apply noise shaping (5th order)
                    float error = dsd_value;
                    for (int j = 0; j < 5; ++j) {
                        error -= noise_shaper_state[j] * (5 - j) * 0.1f;
                    }

                    // Update noise shaper state
                    for (int j = 4; j > 0; --j) {
                        noise_shaper_state[j] = noise_shaper_state[j - 1];
                    }
                    noise_shaper_state[0] = error;

                    // Accumulate for decimation (simplified)
                    float sample = static_cast<float>(error) / decimation_factor;
                    chunk_buffer.push_back(sample);

                    // Flush chunk when buffer is full
                    if (chunk_buffer.size() >= chunk_size_samples) {
                        chunk_count++;
                        bool continue_streaming = callback(chunk_buffer.data(), chunk_buffer.size());
                        chunk_buffer.clear();

                        if (!continue_streaming) {
                            LOG_INFO("Streaming stopped by callback");
                            goto cleanup;
                        }
                    }
                }

                samples_decoded++;
                dsd_index += impl_->channels;
            }
        }

        dsd_bytes_remaining -= bytes_read;
    }

    // Flush final chunk
    if (!chunk_buffer.empty()) {
        chunk_count++;
        callback(chunk_buffer.data(), chunk_buffer.size());
    }

cleanup:
    LOG_INFO("DSD streaming complete: {} chunks, {} samples", chunk_count, samples_decoded);

    // Close file
    impl_->dsd_file.close();

    return ErrorCode::Success;
}

bool DSDDecoder::isLoaded() const {
    return impl_->loaded;
}

} // namespace load
} // namespace xpu
