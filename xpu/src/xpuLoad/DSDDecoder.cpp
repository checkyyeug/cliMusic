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
    uint64_t chunk_size;     // Total chunk size (always 28)
    uint32_t padding1;       // Padding (4 bytes)
    uint64_t file_size;      // Total file size
    uint32_t padding2;       // Padding (4 bytes)
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
 * @brief DSDIFF format header
 */
struct DSDIFFHeader {
    char id[4];              // 'F', 'R', 'M', '8'
    uint32_t chunk_size;     // Chunk size (excluding header)
    char type[4];            // 'D', 'S', 'D', ' '
};

/**
 * @brief DSDIFF chunk header
 */
struct DSDIFFChunkHeader {
    char id[4];              // Chunk ID (e.g., 'prop', 'DSD ')
    uint32_t chunk_size;     // Chunk size (excluding header)
};

/**
 * @brief DSDIFF property chunk (simplified)
 */
struct DSDIFFPropChunk {
    char id[4];              // 'p', 'r', 'o', 'p'
    uint32_t chunk_size;     // Chunk size
    uint16_t version;        // Version
    uint32_t sample_rate;    // Sample rate (Hz)
    uint16_t channels;       // Number of channels
    uint16_t bits_per_sample;// Bits per sample (always 1 for DSD)
    uint32_t sample_count;   // Total sample count
    uint16_t channel_type;   // Channel type
    uint16_t reserved;       // Reserved
};

/**
 * @brief DSDIFF DSD data chunk
 */
struct DSDIFFDataChunk {
    char id[4];              // 'D', 'S', 'D', ' '
    uint32_t chunk_size;     // Chunk size (excluding header)
    uint32_t data_size;      // Actual DSD data size
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
    int dsd_decimation = 16;  // Default DSD decimation factor (16, 32, or 64)

    uint32_t dsd_rate = 0;       // DSD sample rate (e.g., 2822400 for DSD64)
    uint32_t channels = 0;
    uint64_t dsd_sample_count = 0;
    uint32_t block_size = 0;     // Block size per channel (for DSF format)

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

void DSDDecoder::setDSDDecimation(int factor) {
    if (factor != 16 && factor != 32 && factor != 64) {
        LOG_ERROR("Invalid DSD decimation factor: {}, must be 16, 32, or 64", factor);
        return;
    }
    impl_->dsd_decimation = factor;
    LOG_INFO("DSD decimation factor set to: {}", factor);
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
    // Store ORIGINAL DSD rate (for information)
    impl_->metadata.original_sample_rate = fmt.sampling_freq;
    // OUTPUT sample rate will be set by decodeDSDToPCM()
    impl_->metadata.sample_rate = 0;
    impl_->metadata.bit_depth = 1; // DSD is 1-bit
    impl_->metadata.original_bit_depth = 1;
    impl_->metadata.format = "DSD";
    impl_->metadata.format_name = "DSD";

    // Calculate duration
    double duration = static_cast<double>(fmt.sample_count) / fmt.sampling_freq;
    impl_->metadata.duration = duration;
    // Sample count will be updated by decodeDSDToPCM after conversion
    impl_->metadata.sample_count = 0;

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
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open DSDIFF file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    // Read DSDIFF header
    DSDIFFHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DSDIFFHeader));

    if (std::memcmp(header.id, "FRM8", 4) != 0 || std::memcmp(header.type, "DSD ", 4) != 0) {
        LOG_ERROR("Invalid DSDIFF file format");
        return ErrorCode::UnsupportedFormat;
    }

    LOG_INFO("DSDIFF Format: DSDIFF");
    LOG_INFO("  Chunk size: {} bytes", header.chunk_size);

    // Read chunks until we find 'prop' and 'DSD ' chunks
    bool found_prop = false;
    bool found_data = false;

    while (!found_data && file.good()) {
        DSDIFFChunkHeader chunk_header;
        file.read(reinterpret_cast<char*>(&chunk_header), sizeof(DSDIFFChunkHeader));

        if (!file.good()) break;

        // Convert chunk_size from big-endian
        uint32_t chunk_size = ((chunk_header.chunk_size >> 24) & 0xFF) |
                             ((chunk_header.chunk_size >> 8) & 0xFF00) |
                             ((chunk_header.chunk_size << 8) & 0xFF0000) |
                             ((chunk_header.chunk_size << 24) & 0xFF000000);

        LOG_DEBUG("Found chunk: {:.4s}, size: {} bytes", chunk_header.id, chunk_size);

        if (std::memcmp(chunk_header.id, "prop", 4) == 0) {
            // Property chunk
            if (chunk_size < sizeof(DSDIFFPropChunk) - 8) {  // -8 for id and size
                LOG_ERROR("Invalid prop chunk size");
                return ErrorCode::CorruptedFile;
            }

            DSDIFFPropChunk prop;
            file.read(reinterpret_cast<char*>(&prop.id), 4);
            file.read(reinterpret_cast<char*>(&prop.chunk_size), 4);
            file.read(reinterpret_cast<char*>(&prop.version), 2);
            file.read(reinterpret_cast<char*>(&prop.sample_rate), 4);
            file.read(reinterpret_cast<char*>(&prop.channels), 2);
            file.read(reinterpret_cast<char*>(&prop.bits_per_sample), 2);
            file.read(reinterpret_cast<char*>(&prop.sample_count), 4);
            file.read(reinterpret_cast<char*>(&prop.channel_type), 2);
            file.read(reinterpret_cast<char*>(&prop.reserved), 2);

            // Convert from big-endian
            prop.version = ((prop.version >> 8) & 0xFF) | ((prop.version << 8) & 0xFF00);
            prop.sample_rate = ((prop.sample_rate >> 24) & 0xFF) |
                             ((prop.sample_rate >> 8) & 0xFF00) |
                             ((prop.sample_rate << 8) & 0xFF0000) |
                             ((prop.sample_rate << 24) & 0xFF000000);
            prop.channels = ((prop.channels >> 8) & 0xFF) | ((prop.channels << 8) & 0xFF00);
            prop.sample_count = ((prop.sample_count >> 24) & 0xFF) |
                               ((prop.sample_count >> 8) & 0xFF00) |
                               ((prop.sample_count << 8) & 0xFF0000) |
                               ((prop.sample_count << 24) & 0xFF000000);

            // Store metadata
            impl_->channels = prop.channels;
            impl_->dsd_rate = prop.sample_rate;
            impl_->dsd_sample_count = prop.sample_count;

            impl_->metadata.channels = prop.channels;
            // Store ORIGINAL DSD rate (for information)
            impl_->metadata.original_sample_rate = prop.sample_rate;
            // OUTPUT sample rate will be set by decodeDSDToPCM()
            impl_->metadata.sample_rate = 0;
            impl_->metadata.bit_depth = 1; // DSD is 1-bit
            impl_->metadata.original_bit_depth = 1;
            impl_->metadata.format = "DSDIFF";
            impl_->metadata.format_name = "DSDIFF";

            // Calculate duration
            double duration = static_cast<double>(prop.sample_count) / prop.sample_rate;
            impl_->metadata.duration = duration;
            // Sample count will be updated by decodeDSDToPCM after conversion
            impl_->metadata.sample_count = 0;

            LOG_INFO("DSDIFF Properties:");
            LOG_INFO("  Version: {}", prop.version);
            LOG_INFO("  Channels: {}", prop.channels);
            LOG_INFO("  DSD Rate: {} Hz ({}x oversampling)", prop.sample_rate, prop.sample_rate / 44100);
            LOG_INFO("  Samples: {}", prop.sample_count);
            LOG_INFO("  Duration: {:.2f} seconds", duration);

            found_prop = true;

            // Skip remaining prop chunk data
            uint32_t remaining = chunk_size - (sizeof(DSDIFFPropChunk) - 8);
            if (remaining > 0) {
                file.seekg(remaining, std::ios::cur);
            }

        } else if (std::memcmp(chunk_header.id, "DSD ", 4) == 0) {
            // DSD data chunk
            if (!found_prop) {
                LOG_ERROR("DSD data chunk found before prop chunk");
                return ErrorCode::CorruptedFile;
            }

            // Read data_size field
            uint32_t data_size;
            file.read(reinterpret_cast<char*>(&data_size), 4);

            // Convert from big-endian
            data_size = ((data_size >> 24) & 0xFF) |
                       ((data_size >> 8) & 0xFF00) |
                       ((data_size << 8) & 0xFF0000) |
                       ((data_size << 24) & 0xFF000000);

            // Store DSD data location
            impl_->dsd_data_offset = file.tellg();

            // For DSDIFF, the data_size includes the actual DSD data
            // The chunk_size includes the data_size field (4 bytes)
            impl_->dsd_data_size = data_size;

            LOG_INFO("DSD Data Chunk:");
            LOG_INFO("  Data offset: {} bytes", impl_->dsd_data_offset);
            LOG_INFO("  Data size: {} bytes", impl_->dsd_data_size);

            found_data = true;

            // Don't read the actual data here - it will be streamed later
            break;

        } else {
            // Skip unknown chunks
            LOG_DEBUG("Skipping chunk: {:.4s}, size: {} bytes", chunk_header.id, chunk_size);
            file.seekg(chunk_size, std::ios::cur);
        }
    }

    if (!found_prop || !found_data) {
        LOG_ERROR("DSDIFF file missing required chunks");
        return ErrorCode::CorruptedFile;
    }

    // Decode DSD to PCM
    ErrorCode ret = decodeDSDToPCM();
    if (ret != ErrorCode::Success) {
        return ret;
    }

    impl_->loaded = true;
    LOG_INFO("DSDIFF file loaded and decoded successfully");

    return ErrorCode::Success;
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

    // Division by zero protection
    if (target_sample_rate == 0) {
        LOG_ERROR("Invalid target sample rate: 0");
        return ErrorCode::InvalidArgument;
    }

    const uint32_t decimation_factor = impl_->dsd_rate / target_sample_rate;

    // Validate decimation factor
    if (decimation_factor == 0) {
        LOG_ERROR("Invalid decimation factor: 0 (dsd_rate={}, target_sample_rate={})",
                  impl_->dsd_rate, target_sample_rate);
        return ErrorCode::InvalidArgument;
    }

    if (decimation_factor > impl_->dsd_rate) {
        LOG_ERROR("Decimation factor {} exceeds DSD rate {}, check sample rates",
                  decimation_factor, impl_->dsd_rate);
        return ErrorCode::InvalidArgument;
    }

    // Calculate output size
    uint64_t total_output_samples = impl_->dsd_sample_count / decimation_factor;

    // Temporary buffer for decoded samples
    std::vector<float> decoded_samples;
    decoded_samples.reserve(total_output_samples * target_channels);

    uint64_t dsd_index = 0;
    uint64_t samples_decoded = 0;

    while (samples_decoded < total_output_samples && dsd_index < impl_->dsd_data.size()) {
        // Process each channel - use actual channel count from file
        // Don't hardcode to 2, support multi-channel DSD files properly
        const uint32_t actual_channels = std::min(impl_->channels, target_channels);

        for (uint32_t ch = 0; ch < actual_channels; ++ch) {
            // Accumulate DSD bits for decimation
            int32_t accumulator = 0;
            uint32_t bits_processed = 0;

            for (uint32_t i = 0; i < decimation_factor && dsd_index < impl_->dsd_data.size(); ++i) {
                // Get DSD bit (MSB first)
                uint8_t byte = impl_->dsd_data[dsd_index / 8];
                uint8_t bit = (byte >> (7 - (dsd_index % 8))) & 1;

                // Convert to bipolar (-1 or +1) and accumulate
                // Note: No noise shaping during DSD decoding - that's only for encoding
                int32_t dsd_bit = bit * 2 - 1;
                accumulator += dsd_bit;

                bits_processed++;
                dsd_index += impl_->channels; // Interleaved channels (use actual file channel count)
            }

            // Average and normalize to float [-1.0, 1.0]
            // DSD signal typically has low amplitude, so we apply a gain factor
            // Standard DSD decoders apply gain to match PCM levels
            float sample = static_cast<float>(accumulator) / bits_processed;

            // Apply gain compensation for DSD
            // DSD signals typically have lower RMS than PCM, so we boost the signal
            // This gain factor (64x or +36dB) brings DSD to comparable levels with PCM
            // Using conservative gain to avoid clipping
            const float dsd_gain = 64.0f;
            sample *= dsd_gain;

            // Clamp to [-1.0, 1.0] to prevent clipping
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;

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

    // Open file for streaming
    impl_->dsd_file.open(filepath, std::ios::binary);
    if (!impl_->dsd_file) {
        LOG_ERROR("Failed to open DSD file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    if (impl_->format == DSDFormat::DSF) {
        // DSF format parsing
        LOG_INFO("Detected DSF format");

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
        impl_->block_size = fmt.block_size;

        // Use configurable decimation factor for DSD conversion
        // Default: /16 (high quality), /32 (if target > 352kHz), /64 (lower quality)
        // For DSD64 with /16: 2822400 / 16 = 176400 Hz
        // For DSD64 with /32: 2822400 / 32 = 88200 Hz
        // For DSD64 with /64: 2822400 / 64 = 44100 Hz
        const uint32_t intermediate_sample_rate = impl_->dsd_rate / impl_->dsd_decimation;

        LOG_INFO("Using DSD decimation factor: {}", impl_->dsd_decimation);
        LOG_INFO("Intermediate sample rate: {} Hz (DSD rate {} / {})",
                 intermediate_sample_rate, impl_->dsd_rate, impl_->dsd_decimation);

        impl_->metadata.file_path = filepath;
        impl_->metadata.channels = fmt.channel_num;
        // Store ORIGINAL DSD rate (for information)
        impl_->metadata.original_sample_rate = fmt.sampling_freq;
        // Store OUTPUT sample rate (what streamPCM will actually produce)
        impl_->metadata.sample_rate = intermediate_sample_rate;
        impl_->metadata.bit_depth = 32; // 32-bit float output from streamPCM
        impl_->metadata.original_bit_depth = 1; // DSD is 1-bit
        impl_->metadata.format = "DSD";
        impl_->metadata.format_name = "DSD";
        impl_->metadata.is_lossless = true;

        // Calculate duration based on DSD sample count and rate
        double duration = static_cast<double>(fmt.sample_count) / fmt.sampling_freq;
        impl_->metadata.duration = duration;
        // Sample count will be updated by streamPCM after actual conversion
        impl_->metadata.sample_count = 0;

        // Mark high-resolution audio
        if (impl_->metadata.sample_rate >= 96000) {
            impl_->metadata.is_high_res = true;
        }

        LOG_INFO("DSD Format: DSF (streaming mode)");
        LOG_INFO("  Channels: {}", fmt.channel_num);
        LOG_INFO("  DSD Rate: {} Hz ({}x oversampling)", fmt.sampling_freq, fmt.sampling_freq / 44100);
        LOG_INFO("  Samples: {}", fmt.sample_count);
        LOG_INFO("  Block size: {} bytes per channel", fmt.block_size);
        LOG_INFO("  Duration: {:.2f} seconds", duration);
        LOG_INFO("  Output metadata: sample_rate={} Hz (DSD64), original_sample_rate={} Hz",
                 impl_->metadata.sample_rate, impl_->metadata.original_sample_rate);

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

    } else if (impl_->format == DSDFormat::DSDIFF) {
        // DSDIFF format parsing
        LOG_INFO("Detected DSDIFF format");

        // Read DSDIFF header
        DSDIFFHeader header;
        impl_->dsd_file.read(reinterpret_cast<char*>(&header), sizeof(DSDIFFHeader));

        if (std::memcmp(header.id, "FRM8", 4) != 0 || std::memcmp(header.type, "DSD ", 4) != 0) {
            LOG_ERROR("Invalid DSDIFF file format");
            return ErrorCode::UnsupportedFormat;
        }

        LOG_INFO("DSDIFF Format: DSDIFF");
        LOG_INFO("  Chunk size: {} bytes", header.chunk_size);

        // Read chunks until we find 'prop' and 'DSD ' chunks
        bool found_prop = false;
        bool found_data = false;

        while (!found_data && impl_->dsd_file.good()) {
            DSDIFFChunkHeader chunk_header;
            impl_->dsd_file.read(reinterpret_cast<char*>(&chunk_header), sizeof(DSDIFFChunkHeader));

            if (!impl_->dsd_file.good()) break;

            // Convert chunk_size from big-endian
            uint32_t chunk_size = ((chunk_header.chunk_size >> 24) & 0xFF) |
                                 ((chunk_header.chunk_size >> 8) & 0xFF00) |
                                 ((chunk_header.chunk_size << 8) & 0xFF0000) |
                                 ((chunk_header.chunk_size << 24) & 0xFF000000);

            LOG_DEBUG("Found chunk: {:.4s}, size: {} bytes", chunk_header.id, chunk_size);

            if (std::memcmp(chunk_header.id, "prop", 4) == 0) {
                // Property chunk
                DSDIFFPropChunk prop;
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.id), 4);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.chunk_size), 4);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.version), 2);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.sample_rate), 4);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.channels), 2);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.bits_per_sample), 2);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.sample_count), 4);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.channel_type), 2);
                impl_->dsd_file.read(reinterpret_cast<char*>(&prop.reserved), 2);

                // Convert from big-endian
                prop.version = ((prop.version >> 8) & 0xFF) | ((prop.version << 8) & 0xFF00);
                prop.sample_rate = ((prop.sample_rate >> 24) & 0xFF) |
                                 ((prop.sample_rate >> 8) & 0xFF00) |
                                 ((prop.sample_rate << 8) & 0xFF0000) |
                                 ((prop.sample_rate << 24) & 0xFF000000);
                prop.channels = ((prop.channels >> 8) & 0xFF) | ((prop.channels << 8) & 0xFF00);
                prop.sample_count = ((prop.sample_count >> 24) & 0xFF) |
                                   ((prop.sample_count >> 8) & 0xFF00) |
                                   ((prop.sample_count << 8) & 0xFF0000) |
                                   ((prop.sample_count << 24) & 0xFF000000);

                // Store metadata
                impl_->channels = prop.channels;
                impl_->dsd_rate = prop.sample_rate;
                impl_->dsd_sample_count = prop.sample_count;

                // Use configurable decimation factor for DSD conversion
                const uint32_t intermediate_sample_rate = impl_->dsd_rate / impl_->dsd_decimation;

                LOG_INFO("Using DSD decimation factor: {}", impl_->dsd_decimation);
                LOG_INFO("Intermediate sample rate: {} Hz (DSD rate {} / {})",
                         intermediate_sample_rate, impl_->dsd_rate, impl_->dsd_decimation);

                impl_->metadata.file_path = filepath;
                impl_->metadata.channels = prop.channels;
                // Store ORIGINAL DSD rate (for information)
                impl_->metadata.original_sample_rate = prop.sample_rate;
                // Store OUTPUT sample rate (what streamPCM will actually produce)
                impl_->metadata.sample_rate = intermediate_sample_rate;
                impl_->metadata.bit_depth = 32; // 32-bit float output from streamPCM
                impl_->metadata.original_bit_depth = 1; // DSD is 1-bit
                impl_->metadata.format = "DSDIFF";
                impl_->metadata.format_name = "DSDIFF";
                impl_->metadata.is_lossless = true;

                // Calculate duration based on DSD sample count and rate
                double duration = static_cast<double>(prop.sample_count) / prop.sample_rate;
                impl_->metadata.duration = duration;
                // Sample count will be updated by streamPCM after actual conversion
                impl_->metadata.sample_count = 0;

                // Mark high-resolution audio
                if (impl_->metadata.sample_rate >= 96000) {
                    impl_->metadata.is_high_res = true;
                }

                LOG_INFO("DSDIFF Properties:");
                LOG_INFO("  Version: {}", prop.version);
                LOG_INFO("  Channels: {}", prop.channels);
                LOG_INFO("  DSD Rate: {} Hz ({}x oversampling)", prop.sample_rate, prop.sample_rate / 44100);
                LOG_INFO("  Samples: {}", prop.sample_count);
                LOG_INFO("  Duration: {:.2f} seconds", duration);

                found_prop = true;

                // Skip remaining prop chunk data
                uint32_t remaining = chunk_size - (sizeof(DSDIFFPropChunk) - 8);
                if (remaining > 0) {
                    impl_->dsd_file.seekg(remaining, std::ios::cur);
                }

            } else if (std::memcmp(chunk_header.id, "DSD ", 4) == 0) {
                // DSD data chunk
                if (!found_prop) {
                    LOG_ERROR("DSD data chunk found before prop chunk");
                    return ErrorCode::CorruptedFile;
                }

                // Read data_size field
                uint32_t data_size;
                impl_->dsd_file.read(reinterpret_cast<char*>(&data_size), 4);

                // Convert from big-endian
                data_size = ((data_size >> 24) & 0xFF) |
                           ((data_size >> 8) & 0xFF00) |
                           ((data_size << 8) & 0xFF0000) |
                           ((data_size << 24) & 0xFF000000);

                // Store DSD data location for streaming
                impl_->dsd_data_offset = impl_->dsd_file.tellg();
                impl_->dsd_data_size = data_size;

                LOG_INFO("DSD Data Chunk:");
                LOG_INFO("  Data offset: {} bytes", impl_->dsd_data_offset);
                LOG_INFO("  Data size: {} bytes", impl_->dsd_data_size);

                found_data = true;
                break;

            } else {
                // Skip unknown chunks
                LOG_DEBUG("Skipping chunk: {:.4s}, size: {} bytes", chunk_header.id, chunk_size);
                impl_->dsd_file.seekg(chunk_size, std::ios::cur);
            }
        }

        if (!found_prop || !found_data) {
            LOG_ERROR("DSDIFF file missing required chunks");
            return ErrorCode::CorruptedFile;
        }

        LOG_INFO("DSDIFF streaming prepared successfully");

    } else {
        LOG_ERROR("Unknown DSD format");
        return ErrorCode::UnsupportedFormat;
    }

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
    const uint32_t target_channels = 2;

    // Use configurable decimation factor for DSD conversion
    // The decimation factor determines how much we reduce the DSD sample rate
    // Default: /16 (high quality), /32 (if target > 352kHz), /64 (lower quality)
    const uint32_t intermediate_sample_rate = impl_->dsd_rate / impl_->dsd_decimation;

    LOG_INFO("Using DSD decimation factor: {}", impl_->dsd_decimation);
    LOG_INFO("Intermediate sample rate: {} Hz (DSD rate {} / {})",
             intermediate_sample_rate, impl_->dsd_rate, impl_->dsd_decimation);

    // Division by zero protection for intermediate_sample_rate
    if (intermediate_sample_rate == 0) {
        LOG_ERROR("Invalid intermediate sample rate: 0");
        return ErrorCode::InvalidArgument;
    }

    if (impl_->dsd_rate == 0) {
        LOG_ERROR("Invalid DSD rate: 0");
        return ErrorCode::InvalidArgument;
    }

    // Calculate actual decimation factor based on DSD rate and intermediate rate
    const uint32_t decimation_factor = impl_->dsd_decimation;

    // Validate decimation factor
    if (decimation_factor == 0) {
        LOG_ERROR("Invalid decimation factor: 0 (dsd_decimation={})",
                  impl_->dsd_decimation);
        return ErrorCode::InvalidArgument;
    }

    if (decimation_factor > impl_->dsd_rate) {
        LOG_ERROR("Decimation factor {} exceeds DSD rate {}",
                  decimation_factor, impl_->dsd_rate);
        return ErrorCode::InvalidArgument;
    }

    // Calculate output size (impl_->dsd_sample_count is per-channel DSD samples)
    // Output frames = (DSD samples per channel / decimation factor)
    // Note: We don't divide by target_channels because dsd_sample_count is already per-channel
    uint64_t total_output_frames = impl_->dsd_sample_count / decimation_factor;

    // Update metadata with output format
    // Note: We output at intermediate_sample_rate, and let xpuIn2Wav handle final resampling
    impl_->metadata.sample_rate = intermediate_sample_rate;
    impl_->metadata.channels = target_channels;
    impl_->metadata.bit_depth = 32; // 32-bit float output
    impl_->metadata.sample_count = total_output_frames * target_channels;

    // Chunk buffer for accumulating samples before callback
    size_t chunk_size_samples = chunk_size_bytes / sizeof(float);

    // Validate chunk_size_samples
    if (chunk_size_samples == 0) {
        LOG_ERROR("Invalid chunk size: {} bytes (results in 0 samples)", chunk_size_bytes);
        return ErrorCode::InvalidArgument;
    }

    // Prevent excessively large chunks that could cause memory issues
    const size_t MAX_CHUNK_SAMPLES = 10 * 1024 * 1024;  // 10M samples = 40MB
    if (chunk_size_samples > MAX_CHUNK_SAMPLES) {
        LOG_ERROR("Chunk size {} samples exceeds maximum {} samples",
                  chunk_size_samples, MAX_CHUNK_SAMPLES);
        return ErrorCode::InvalidArgument;
    }

    std::vector<float> chunk_buffer;
    // Reserve with extra buffer to prevent frequent reallocations
    chunk_buffer.reserve(chunk_size_samples + 100);  // Add safety margin

    // Seek to DSD data and read all DSD data into memory
    // Note: For true streaming we would read in chunks, but for simplicity
    // we load all DSD data here since we already have the file open
    impl_->dsd_file.seekg(impl_->dsd_data_offset);

    // Memory allocation safety: add size limit to prevent excessive memory usage
    // DSD1024 stereo: 45.1584 MHz × 2 channels = ~90 MB/minute
    // 1GB limit supports ~11 minutes of DSD1024 audio
    const size_t MAX_DSD_MEMORY = 1024 * 1024 * 1024;  // 1GB limit for DSD1024 support
    if (impl_->dsd_data_size > MAX_DSD_MEMORY) {
        LOG_ERROR("DSD data size {} bytes ({} MB) exceeds maximum allowed {} MB",
                  impl_->dsd_data_size, impl_->dsd_data_size / (1024 * 1024),
                  MAX_DSD_MEMORY / (1024 * 1024));
        return ErrorCode::OutOfMemory;
    }

    // Check for empty data
    if (impl_->dsd_data_size == 0) {
        LOG_ERROR("DSD data size is 0, nothing to decode");
        return ErrorCode::InvalidOperation;
    }

    // Try-catch for memory allocation failures
    std::vector<uint8_t> dsd_data;
    try {
        dsd_data.resize(impl_->dsd_data_size);
    } catch (const std::bad_alloc& e) {
        LOG_ERROR("Memory allocation failed for DSD data (requested {} bytes): {}",
                  impl_->dsd_data_size, e.what());
        return ErrorCode::OutOfMemory;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during DSD data allocation: {}", e.what());
        return ErrorCode::OutOfMemory;
    }

    // Read DSD data
    impl_->dsd_file.read(reinterpret_cast<char*>(dsd_data.data()), impl_->dsd_data_size);
    size_t dsd_bytes_read = impl_->dsd_file.gcount();

    if (dsd_bytes_read != impl_->dsd_data_size) {
        LOG_ERROR("Failed to read complete DSD data: expected {}, got {}",
                 impl_->dsd_data_size, dsd_bytes_read);
        return ErrorCode::FileReadError;
    }

    // Close file since we have all data in memory
    impl_->dsd_file.close();

    LOG_INFO("DSD data loaded: {} bytes", dsd_data.size());

    // Decode DSD to PCM (same logic as batch mode)
    uint64_t samples_decoded = 0;
    int chunk_count = 0;

    // Note: DSF format stores channels separately (not bit-interleaved)
    // For stereo: all channel 0 data comes first, then all channel 1 data
    size_t channel_data_size = dsd_data.size() / target_channels;  // Per-channel data size in bytes

    // Safety check for channel_data_size to avoid division by zero
    if (target_channels == 0 || channel_data_size == 0) {
        LOG_ERROR("Invalid channel configuration: target_channels={}, channel_data_size={}",
                  target_channels, channel_data_size);
        return ErrorCode::InvalidOperation;
    }

    while (samples_decoded < total_output_frames) {
        // Process each channel
        for (uint32_t ch = 0; ch < target_channels && samples_decoded < total_output_frames; ++ch) {
            // Calculate the bit index for this channel
            // DSF format: channel 0 data first, then channel 1 data, etc.
            size_t channel_bit_index = ch * channel_data_size * 8 + (samples_decoded * decimation_factor);

            // Enhanced boundary checking to prevent buffer overflow
            // Check 1: channel_bit_index must not exceed channel data bounds
            size_t max_channel_bit_index = (ch + 1) * channel_data_size * 8;
            if (channel_bit_index >= max_channel_bit_index) {
                LOG_WARN("Channel {} bit index {} exceeds channel bounds {}, ending stream",
                         ch, channel_bit_index, max_channel_bit_index);
                goto streaming_complete;
            }

            // Check 2: ensure we don't read beyond total DSD data
            if (channel_bit_index + decimation_factor > dsd_data.size() * 8) {
                LOG_WARN("Insufficient DSD data: need {} bits, available {} bits",
                         channel_bit_index + decimation_factor, dsd_data.size() * 8);
                goto streaming_complete;
            }

            // Check 3: prevent integer overflow in bit index calculation
            if (samples_decoded > (UINT64_MAX - decimation_factor) / decimation_factor) {
                LOG_ERROR("Integer overflow prevented at samples_decoded={}", samples_decoded);
                goto streaming_complete;
            }

            // Accumulate DSD bits for decimation
            int32_t accumulator = 0;
            uint32_t bits_processed = 0;
            int ones_count = 0;  // Debug: count how many bits are 1

            // Accumulate decimation_factor DSD bits
            for (uint32_t i = 0; i < decimation_factor; ++i) {
                // Get DSD bit - DSF format uses MSB-first bit order
                // Validate byte index before accessing
                size_t byte_index = channel_bit_index / 8;
                if (byte_index >= dsd_data.size()) {
                    LOG_ERROR("Byte index {} out of bounds (data size {})",
                              byte_index, dsd_data.size());
                    goto streaming_complete;
                }

                uint8_t byte = dsd_data[byte_index];

                // Extract bit using MSB-first order (bit 7 is first, bit 0 is last)
                // This is the standard bit order for DSF format
                uint8_t bit = (byte >> (7 - (channel_bit_index % 8))) & 1;

                // Convert to bipolar (-1 or +1) and accumulate
                // Note: No noise shaping during DSD decoding - that's only for encoding
                int32_t dsd_bit = bit * 2 - 1;
                accumulator += dsd_bit;
                if (bit == 1) ones_count++;

                bits_processed++;
                channel_bit_index++;  // Move to next bit in this channel
            }

            // Average and normalize to float [-1.0, 1.0]
            // DSD signal typically has low amplitude, so we apply a gain factor
            // Standard DSD decoders apply gain to match PCM levels
            float sample = static_cast<float>(accumulator) / bits_processed;

            // Apply gain compensation for DSD
            // DSD signals typically have lower RMS than PCM, so we boost the signal
            // This gain factor (64x or +36dB) brings DSD to comparable levels with PCM
            // Using conservative gain to avoid clipping
            const float dsd_gain = 64.0f;
            sample *= dsd_gain;

            // Clamp to [-1.0, 1.0] to prevent clipping
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;

            // Debug: Log first few samples with detailed bit information
            if (samples_decoded < 5) {
                LOG_INFO("DSD decoded [ch={}, frame={}]: acc={}, ones={}, bits={}, normalized={}",
                         ch, samples_decoded, accumulator, ones_count, bits_processed, sample);
                // Log first few bytes for inspection
                if (samples_decoded == 0 && ch == 0) {
                    size_t byte_offset = (ch * channel_data_size * 8) / 8;
                    LOG_INFO("  First 8 bytes at channel offset {}: {:#02x} {:#02x} {:#02x} {:#02x} {:#02x} {:#02x} {:#02x} {:#02x}",
                             byte_offset,
                             dsd_data[byte_offset], dsd_data[byte_offset+1], dsd_data[byte_offset+2], dsd_data[byte_offset+3],
                             dsd_data[byte_offset+4], dsd_data[byte_offset+5], dsd_data[byte_offset+6], dsd_data[byte_offset+7]);
                }
            }

            // Add sample to chunk buffer with bounds checking
            if (chunk_buffer.size() >= chunk_buffer.capacity()) {
                // This shouldn't happen with proper reservation, but protect anyway
                LOG_ERROR("Chunk buffer overflow: size {} >= capacity {}",
                          chunk_buffer.size(), chunk_buffer.capacity());
                return ErrorCode::BufferOverrun;
            }

            chunk_buffer.push_back(sample);

            // Flush chunk when buffer is full (using > to handle exact size match)
            if (chunk_buffer.size() >= chunk_size_samples) {
                chunk_count++;
                if (chunk_count <= 5) {
                    LOG_INFO("Output chunk {}: {} samples ({} bytes)",
                            chunk_count, chunk_buffer.size(),
                            chunk_buffer.size() * sizeof(float));
                }
                bool continue_streaming = callback(chunk_buffer.data(), chunk_buffer.size());
                chunk_buffer.clear();

                if (!continue_streaming) {
                    LOG_INFO("Streaming stopped by callback");
                    return ErrorCode::Success;
                }
            }
        }

        samples_decoded++;
    }

streaming_complete:

    // Flush final chunk
    if (!chunk_buffer.empty()) {
        chunk_count++;
        callback(chunk_buffer.data(), chunk_buffer.size());
    }

    LOG_INFO("DSD streaming complete: {} chunks, {} output samples", chunk_count, samples_decoded * target_channels);

    return ErrorCode::Success;
}

bool DSDDecoder::isLoaded() const {
    return impl_->loaded;
}

} // namespace load
} // namespace xpu
