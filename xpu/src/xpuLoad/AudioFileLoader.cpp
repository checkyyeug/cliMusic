/**
 * @file AudioFileLoader.cpp
 * @brief Audio file loader implementation
 */

#include "AudioFileLoader.h"
#include "utils/Logger.h"
#include <cstring>
#include <string>
#include <codecvt>
#include <locale>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

using namespace xpu;

namespace xpu {
namespace load {

/**
 * @brief Clean and validate UTF-8 string
 * Removes invalid UTF-8 sequences and handles potential UTF-16 data
 * Detects UTF-16 LE/BE with or without BOM
 */
static std::string cleanUTF8(const char* input) {
    if (!input) return "";

    std::string result;
    const unsigned char* p = (const unsigned char*)input;
    size_t len = strlen((const char*)p);

    // Detect if this looks like UTF-16 LE (with or without BOM)
    // UTF-16 LE has pattern: low byte, high byte, low byte, high byte...
    // ASCII characters in UTF-16 LE appear as: ascii_byte, 0x00, ascii_byte, 0x00...
    bool is_utf16_le = false;
    bool has_bom_le = (len >= 2 && p[0] == 0xFF && p[1] == 0xFE);

    // Check for UTF-16 LE pattern (alternating byte, null for ASCII)
    if (!has_bom_le && len >= 4) {
        int utf16_le_score = 0;
        for (size_t i = 0; i < std::min(len, size_t(20)); i += 2) {
            // Check for pattern: printable char followed by null
            if (i + 1 < len && p[i] >= 32 && p[i] <= 126 && p[i + 1] == 0) {
                utf16_le_score++;
            }
        }
        // If most characters fit this pattern, it's likely UTF-16 LE
        is_utf16_le = (utf16_le_score >= 2);
    }

    if (has_bom_le || is_utf16_le) {
        // UTF-16 LE detected - convert ASCII characters only
        size_t start = has_bom_le ? 2 : 0;
        for (size_t i = start; i + 1 < len; i += 2) {
            unsigned char low = p[i];
            unsigned char high = p[i + 1];
            // Extract ASCII characters from UTF-16 LE
            if (high == 0 && low >= 32 && low <= 126) {
                result += low;
            } else if (high == 0 && low == 0) {
                break;  // Null terminator
            }
            // Skip non-ASCII UTF-16 characters
        }
        return result;
    }

    // Check for UTF-16 BE BOM (FE FF)
    bool has_bom_be = (len >= 2 && p[0] == 0xFE && p[1] == 0xFF);

    // Detect if this looks like UTF-16 BE (with or without BOM)
    // UTF-16 BE has pattern: high byte, low byte, high byte, low byte...
    // ASCII characters in UTF-16 BE appear as: 0x00, ascii_byte, 0x00, ascii_byte...
    bool is_utf16_be = false;
    if (!has_bom_be && len >= 4) {
        int utf16_be_score = 0;
        for (size_t i = 0; i < std::min(len, size_t(20)); i += 2) {
            // Check for pattern: null followed by printable char
            if (i + 1 < len && p[i] == 0 && p[i + 1] >= 32 && p[i + 1] <= 126) {
                utf16_be_score++;
            }
        }
        // If most characters fit this pattern, it's likely UTF-16 BE
        is_utf16_be = (utf16_be_score >= 2);
    }

    if (has_bom_be || is_utf16_be) {
        // UTF-16 BE detected - convert ASCII characters only
        size_t start = has_bom_be ? 2 : 0;
        for (size_t i = start; i + 1 < len; i += 2) {
            unsigned char high = p[i];
            unsigned char low = p[i + 1];
            // Extract ASCII characters from UTF-16 BE
            if (high == 0 && low >= 32 && low <= 126) {
                result += low;
            } else if (high == 0 && low == 0) {
                break;  // Null terminator
            }
            // Skip non-ASCII UTF-16 characters
        }
        return result;
    }

    // Process as UTF-8 or ASCII
    while (*p) {
        if (*p < 128) {
            // ASCII character (0-127) - always safe
            result += *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            // 2-byte UTF-8 sequence
            if (p[1] && (p[1] & 0xC0) == 0x80) {
                result += *p++;
                result += *p++;
            } else {
                // Invalid UTF-8, skip this byte
                p++;
            }
        } else if ((*p & 0xF0) == 0xE0) {
            // 3-byte UTF-8 sequence
            if (p[1] && p[2] && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
                result += *p++;
                result += *p++;
                result += *p++;
            } else {
                // Invalid UTF-8, skip this byte
                p++;
            }
        } else if ((*p & 0xF8) == 0xF0) {
            // 4-byte UTF-8 sequence
            if (p[1] && p[2] && p[3] && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
                result += *p++;
                result += *p++;
                result += *p++;
                result += *p++;
            } else {
                // Invalid UTF-8, skip this byte
                p++;
            }
        } else {
            // Invalid UTF-8 start byte, skip
            p++;
        }
    }

    return result;
}

/**
 * @brief Implementation class
 */
class AudioFileLoader::Impl {
public:
    protocol::AudioMetadata metadata;
    std::vector<uint8_t> pcm_data;
    bool loaded = false;
    int target_sample_rate = 48000;  // Default target sample rate
    int dsd_decimation = 16;  // Default DSD decimation factor (16, 32, or 64)
    int audio_stream_index = -1;     // Audio stream index in the file

    // FFmpeg contexts
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;
};

AudioFileLoader::AudioFileLoader()
    : impl_(std::make_unique<Impl>()) {}

AudioFileLoader::~AudioFileLoader() {
    // Cleanup FFmpeg resources
    if (impl_->swr_ctx) {
        swr_free(&impl_->swr_ctx);
    }
    if (impl_->codec_ctx) {
        avcodec_free_context(&impl_->codec_ctx);
    }
    if (impl_->format_ctx) {
        avformat_close_input(&impl_->format_ctx);
    }
}

void AudioFileLoader::setTargetSampleRate(int sample_rate) {
    impl_->target_sample_rate = sample_rate;
    LOG_INFO("Target sample rate set to: {}", sample_rate);
}

void AudioFileLoader::setDSDDecimation(int factor) {
    if (factor != 16 && factor != 32 && factor != 64) {
        LOG_ERROR("Invalid DSD decimation factor: {}, must be 16, 32, or 64", factor);
        return;
    }
    impl_->dsd_decimation = factor;
    LOG_INFO("DSD decimation factor set to: {}", factor);
}

ErrorCode AudioFileLoader::load(const std::string& filepath) {
    LOG_INFO("Loading audio file: {}", filepath);

    // Open input file
    int ret = avformat_open_input(&impl_->format_ctx, filepath.c_str(), nullptr, nullptr);
    if (ret != 0) {
        LOG_ERROR("Failed to open file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    // Retrieve stream information
    ret = avformat_find_stream_info(impl_->format_ctx, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to find stream info");
        return ErrorCode::CorruptedFile;
    }

    // Find audio stream
    impl_->audio_stream_index = -1;
    for (unsigned int i = 0; i < impl_->format_ctx->nb_streams; ++i) {
        if (impl_->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            impl_->audio_stream_index = i;
            break;
        }
    }

    if (impl_->audio_stream_index == -1) {
        LOG_ERROR("No audio stream found");
        return ErrorCode::InvalidOperation;
    }

    // Get codec parameters
    AVCodecParameters* codec_par = impl_->format_ctx->streams[impl_->audio_stream_index]->codecpar;

    // Update metadata - preserve original file properties
    impl_->metadata.sample_rate = codec_par->sample_rate;
    impl_->metadata.channels = codec_par->ch_layout.nb_channels;

    // Get original bit depth from codec parameters
    // codec_par->format is the internal sample format, not the original bit depth
    // We need to check codec_par->bits_per_raw_sample or codec_par->bits_per_coded_sample
    int source_bit_depth = codec_par->bits_per_raw_sample;
    if (source_bit_depth == 0) {
        source_bit_depth = codec_par->bits_per_coded_sample;
    }
    if (source_bit_depth == 0) {
        // Fallback: estimate from sample format
        source_bit_depth = codec_par->format == AV_SAMPLE_FMT_FLTP ? 32 :
                            codec_par->format == AV_SAMPLE_FMT_S16 || codec_par->format == AV_SAMPLE_FMT_S16P ? 16 :
                            codec_par->format == AV_SAMPLE_FMT_S32 || codec_par->format == AV_SAMPLE_FMT_S32P ? 32 : 24;
    }
    impl_->metadata.bit_depth = source_bit_depth;

    // Calculate duration
    if (impl_->format_ctx->duration != AV_NOPTS_VALUE) {
        impl_->metadata.duration = static_cast<double>(impl_->format_ctx->duration) / AV_TIME_BASE;
        impl_->metadata.sample_count = static_cast<uint64_t>(
            impl_->metadata.duration * impl_->metadata.sample_rate);
    }

    // Extract metadata tags (title, artist, album, etc.)
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(impl_->format_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        std::string key = tag->key;
        std::string value = tag->value ? tag->value : "";

        if (key == "title") impl_->metadata.title = value;
        else if (key == "artist") impl_->metadata.artist = value;
        else if (key == "album") impl_->metadata.album = value;
        else if (key == "track") impl_->metadata.track_number = std::stoi(value);
        else if (key == "genre") impl_->metadata.genre = value;
        else if (key == "date") impl_->metadata.year = std::stoi(value);
    }

    // Detect audio format
    audio::AudioFormat format_enum = audio::AudioFormatUtils::formatFromExtension(filepath);
    impl_->metadata.format = audio::AudioFormatUtils::formatToString(format_enum);
    impl_->metadata.format_name = impl_->metadata.format;

    // Initialize decoder
    const AVCodec* codec = avcodec_find_decoder(codec_par->codec_id);
    if (!codec) {
        LOG_ERROR("Codec not found for codec_id: {}", codec_par->codec_id);
        return ErrorCode::UnsupportedFormat;
    }

    impl_->codec_ctx = avcodec_alloc_context3(codec);
    if (!impl_->codec_ctx) {
        LOG_ERROR("Failed to allocate codec context");
        return ErrorCode::OutOfMemory;
    }

    ret = avcodec_parameters_to_context(impl_->codec_ctx, codec_par);
    if (ret < 0) {
        LOG_ERROR("Failed to copy codec parameters");
        return ErrorCode::InvalidOperation;
    }

    ret = avcodec_open2(impl_->codec_ctx, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to open codec");
        return ErrorCode::InvalidOperation;
    }

    // Setup resampler to convert to standard format
    // Target: target_sample_rate Hz, stereo, 32-bit float planar
    // For DSD: if target_sample_rate is 0, use DSD rate / dsd_decimation
    // For non-DSD: if target_sample_rate is 0, keep the original sample rate
    int actual_target_rate;

    if (format_enum == audio::AudioFormat::DSD && impl_->target_sample_rate == 0) {
        // DSD with no target specified: use DSD rate / dsd_decimation
        actual_target_rate = impl_->codec_ctx->sample_rate / impl_->dsd_decimation;
    } else if (impl_->target_sample_rate > 0) {
        actual_target_rate = impl_->target_sample_rate;
    } else {
        actual_target_rate = impl_->codec_ctx->sample_rate;
    }

    LOG_INFO("Setting up resampler: requested_rate={}, actual_rate={}, original_rate={}",
             impl_->target_sample_rate, actual_target_rate, impl_->codec_ctx->sample_rate);

    AVChannelLayout target_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&impl_->swr_ctx,
                        &target_ch_layout,
                        AV_SAMPLE_FMT_FLTP,
                        actual_target_rate,
                        &impl_->codec_ctx->ch_layout,
                        impl_->codec_ctx->sample_fmt,
                        impl_->codec_ctx->sample_rate,
                        0, nullptr);

    if (!impl_->swr_ctx || swr_init(impl_->swr_ctx) < 0) {
        LOG_ERROR("Failed to initialize resampler");
        return ErrorCode::InvalidOperation;
    }

    // Decode audio data
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    // Allocate output buffer for resampling
    // Maximum output samples per input sample (upsample to highest rate)
    int max_output_samples = av_rescale_rnd(
        frame->nb_samples ? frame->nb_samples : 1024,
        actual_target_rate,
        impl_->codec_ctx->sample_rate,
        AV_ROUND_UP
    );

    // Use a buffer for planar float output
    std::vector<uint8_t> out_buffer[2];  // Stereo planar
    out_buffer[0].resize(max_output_samples * sizeof(float));
    out_buffer[1].resize(max_output_samples * sizeof(float));
    uint8_t* out_data[2] = { out_buffer[0].data(), out_buffer[1].data() };

    std::vector<float> decoded_samples;

    int packet_count = 0;
    int frame_count = 0;

    while (av_read_frame(impl_->format_ctx, packet) >= 0) {
        packet_count++;
        if (packet->stream_index == impl_->audio_stream_index) {
            int send_result = avcodec_send_packet(impl_->codec_ctx, packet);
            if (send_result == 0) {
                while (avcodec_receive_frame(impl_->codec_ctx, frame) == 0) {
                    frame_count++;

                    // Calculate output samples
                    int out_samples = av_rescale_rnd(
                        swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate) + frame->nb_samples,
                        actual_target_rate,
                        impl_->codec_ctx->sample_rate,
                        AV_ROUND_UP
                    );

                    // Resize buffer if needed
                    if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
                        out_buffer[0].resize(out_samples * sizeof(float));
                        out_buffer[1].resize(out_samples * sizeof(float));
                        out_data[0] = out_buffer[0].data();
                        out_data[1] = out_buffer[1].data();
                    }

                    // Resample using swr_convert
                    int converted_samples = swr_convert(
                        impl_->swr_ctx,
                        out_data, out_samples,
                        const_cast<const uint8_t**>(frame->data),
                        frame->nb_samples
                    );

                    if (converted_samples < 0) {
                        LOG_ERROR("swr_convert failed: {}", converted_samples);
                        continue;
                    }

                    // Convert planar to interleaved
                    float* left_channel = reinterpret_cast<float*>(out_data[0]);
                    float* right_channel = reinterpret_cast<float*>(out_data[1]);

                    for (int i = 0; i < converted_samples; ++i) {
                        decoded_samples.push_back(left_channel[i]);
                        decoded_samples.push_back(right_channel[i]);
                    }
                }
            } else {
                LOG_ERROR("avcodec_send_packet failed: {}", send_result);
            }
        }
        av_packet_unref(packet);
    }

    LOG_INFO("Read {} packets, decoded {} frames", packet_count, frame_count);

    // Flush decoder
    avcodec_send_packet(impl_->codec_ctx, nullptr);
    while (avcodec_receive_frame(impl_->codec_ctx, frame) == 0) {
        // Calculate output samples for flush
        int out_samples = av_rescale_rnd(
            swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate) + frame->nb_samples,
            actual_target_rate,
            impl_->codec_ctx->sample_rate,
            AV_ROUND_UP
        );

        // Resize buffer if needed
        if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
            out_buffer[0].resize(out_samples * sizeof(float));
            out_buffer[1].resize(out_samples * sizeof(float));
            out_data[0] = out_buffer[0].data();
            out_data[1] = out_buffer[1].data();
        }

        // Resample using swr_convert
        int converted_samples = swr_convert(
            impl_->swr_ctx,
            out_data, out_samples,
            const_cast<const uint8_t**>(frame->data),
            frame->nb_samples
        );

        if (converted_samples < 0) {
            LOG_ERROR("swr_convert failed during flush: {}", converted_samples);
            continue;
        }

        // Convert planar to interleaved
        float* left_channel = reinterpret_cast<float*>(out_data[0]);
        float* right_channel = reinterpret_cast<float*>(out_data[1]);

        for (int i = 0; i < converted_samples; ++i) {
            decoded_samples.push_back(left_channel[i]);
            decoded_samples.push_back(right_channel[i]);
        }
    }

    // Flush any remaining samples in resampler
    while (true) {
        int out_samples = av_rescale_rnd(
            swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate),
            actual_target_rate,
            impl_->codec_ctx->sample_rate,
            AV_ROUND_UP
        );

        if (out_samples == 0) break;

        // Resize buffer if needed
        if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
            out_buffer[0].resize(out_samples * sizeof(float));
            out_buffer[1].resize(out_samples * sizeof(float));
            out_data[0] = out_buffer[0].data();
            out_data[1] = out_buffer[1].data();
        }

        int converted_samples = swr_convert(
            impl_->swr_ctx,
            out_data, out_samples,
            nullptr, 0
        );

        if (converted_samples <= 0) break;

        // Convert planar to interleaved
        float* left_channel = reinterpret_cast<float*>(out_data[0]);
        float* right_channel = reinterpret_cast<float*>(out_data[1]);

        for (int i = 0; i < converted_samples; ++i) {
            decoded_samples.push_back(left_channel[i]);
            decoded_samples.push_back(right_channel[i]);
        }
    }

    // Copy to PCM data buffer (32-bit float)
    size_t byte_size = decoded_samples.size() * sizeof(float);
    LOG_INFO("Decoded samples: {} floats ({} bytes)", decoded_samples.size(), byte_size);
    impl_->pcm_data.resize(byte_size);
    std::memcpy(impl_->pcm_data.data(), decoded_samples.data(), byte_size);

    // Store original properties before overwriting (for high-res detection)
    int original_sample_rate = impl_->metadata.sample_rate;
    int original_bit_depth = impl_->metadata.bit_depth;
    int original_channels = impl_->metadata.channels;

    // Update metadata with actual output format (for PCM data)
    impl_->metadata.sample_rate = actual_target_rate;  // Output format (or original if target was 0)
    impl_->metadata.channels = 2;
    impl_->metadata.bit_depth = 32;  // Output is always 32-bit float
    impl_->metadata.sample_count = decoded_samples.size() / 2;

    // Store original properties for reference
    impl_->metadata.original_sample_rate = original_sample_rate;
    impl_->metadata.original_bit_depth = original_bit_depth;
    impl_->metadata.is_high_res = (original_sample_rate >= 96000);

    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);

    impl_->loaded = true;
    LOG_INFO("Audio file loaded successfully");
    LOG_INFO("  Format: {} Hz, {} channels, {}-bit",
             impl_->metadata.sample_rate,
             impl_->metadata.channels,
             impl_->metadata.bit_depth);
    LOG_INFO("  Duration: {:.2f} seconds", impl_->metadata.duration);

    return ErrorCode::Success;
}

ErrorCode AudioFileLoader::prepareStreaming(const std::string& filepath) {
    LOG_INFO("Preparing streaming for audio file: {}", filepath);

    // Open input file
    int ret = avformat_open_input(&impl_->format_ctx, filepath.c_str(), nullptr, nullptr);
    if (ret != 0) {
        LOG_ERROR("Failed to open file: {}", filepath);
        return ErrorCode::FileReadError;
    }

    // Retrieve stream information (this is fast - doesn't decode entire file)
    ret = avformat_find_stream_info(impl_->format_ctx, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to find stream info");
        return ErrorCode::CorruptedFile;
    }

    // Find audio stream
    impl_->audio_stream_index = -1;
    for (unsigned int i = 0; i < impl_->format_ctx->nb_streams; ++i) {
        if (impl_->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            impl_->audio_stream_index = i;
            break;
        }
    }

    if (impl_->audio_stream_index == -1) {
        LOG_ERROR("No audio stream found");
        return ErrorCode::InvalidOperation;
    }

    // Get codec parameters
    AVCodecParameters* codec_par = impl_->format_ctx->streams[impl_->audio_stream_index]->codecpar;

    // Update metadata - preserve original file properties
    impl_->metadata.file_path = filepath;
    int original_sample_rate = codec_par->sample_rate;
    impl_->metadata.original_sample_rate = original_sample_rate;
    impl_->metadata.channels = codec_par->ch_layout.nb_channels;

    // Get original bit depth
    int source_bit_depth = codec_par->bits_per_raw_sample;
    if (source_bit_depth == 0) {
        source_bit_depth = codec_par->bits_per_coded_sample;
    }

    // Detect audio format
    audio::AudioFormat format_enum = audio::AudioFormatUtils::formatFromExtension(filepath);

    // For DSD format, the original bit depth is always 1-bit
    // codec_par->bits_per_coded_sample might be 8 (byte storage), but actual DSD is 1-bit
    if (format_enum == audio::AudioFormat::DSD) {
        source_bit_depth = 1;  // DSD is always 1-bit
    } else if (source_bit_depth == 0) {
        source_bit_depth = codec_par->format == AV_SAMPLE_FMT_FLTP ? 32 :
                            codec_par->format == AV_SAMPLE_FMT_S16 || codec_par->format == AV_SAMPLE_FMT_S16P ? 16 :
                            codec_par->format == AV_SAMPLE_FMT_S32 || codec_par->format == AV_SAMPLE_FMT_S32P ? 32 : 24;
    }

    impl_->metadata.original_bit_depth = source_bit_depth;

    // Calculate output sample rate and bit depth
    // For DSD: PCM rate = DSD rate / dsd_decimation, output is 32-bit float
    // For non-DSD: use target_sample_rate if specified, otherwise keep original
    int output_sample_rate;
    if (format_enum == audio::AudioFormat::DSD) {
        // DSD: PCM sample rate = DSD rate / dsd_decimation (e.g., DSD64: 2822400/16 = 176400 Hz)
        if (impl_->target_sample_rate > 0) {
            output_sample_rate = impl_->target_sample_rate;
        } else {
            output_sample_rate = original_sample_rate / impl_->dsd_decimation;  // Default: DSD/16
        }
        impl_->metadata.bit_depth = 32;  // Output is always 32-bit float
    } else {
        // Non-DSD: use target sample rate or keep original
        if (impl_->target_sample_rate > 0) {
            output_sample_rate = impl_->target_sample_rate;
        } else {
            output_sample_rate = original_sample_rate;
        }
        impl_->metadata.bit_depth = 32;  // Output is always 32-bit float
    }

    impl_->metadata.sample_rate = output_sample_rate;

    // Calculate duration
    if (impl_->format_ctx->duration != AV_NOPTS_VALUE) {
        impl_->metadata.duration = static_cast<double>(impl_->format_ctx->duration) / AV_TIME_BASE;
        impl_->metadata.sample_count = static_cast<uint64_t>(
            impl_->metadata.duration * impl_->metadata.sample_rate);
    }

    // Extract metadata tags
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(impl_->format_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        std::string key = tag->key;
        const char* value_ptr = tag->value;

        if (!value_ptr) continue;

        // Clean UTF-8 encoding to avoid display issues
        std::string value = cleanUTF8(value_ptr);

        if (key == "title") impl_->metadata.title = value;
        else if (key == "artist") impl_->metadata.artist = value;
        else if (key == "album") impl_->metadata.album = value;
        else if (key == "track") {
            try {
                impl_->metadata.track_number = std::stoi(value);
            } catch (...) {
                // Ignore conversion errors
            }
        }
        else if (key == "genre") impl_->metadata.genre = value;
        else if (key == "date") {
            try {
                impl_->metadata.year = std::stoi(value);
            } catch (...) {
                impl_->metadata.year = value;  // Keep as string if conversion fails
            }
        }
    }

    // Set format information
    impl_->metadata.format = audio::AudioFormatUtils::formatToString(format_enum);
    impl_->metadata.format_name = impl_->metadata.format;

    // Mark as lossless based on format
    impl_->metadata.is_lossless = (format_enum == audio::AudioFormat::FLAC ||
                                    format_enum == audio::AudioFormat::WAV ||
                                    format_enum == audio::AudioFormat::ALAC ||
                                    format_enum == audio::AudioFormat::DSD);

    // Mark high-resolution audio
    if (impl_->metadata.sample_rate >= 96000) {
        impl_->metadata.is_high_res = true;
    }

    LOG_INFO("Streaming prepared successfully");
    LOG_INFO("  Format: {} Hz, {} channels, {}-bit",
             impl_->metadata.sample_rate,
             impl_->metadata.channels,
             impl_->metadata.bit_depth);
    LOG_INFO("  Duration: {:.2f} seconds", impl_->metadata.duration);

    return ErrorCode::Success;
}

ErrorCode AudioFileLoader::streamPCM(StreamingCallback callback, size_t chunk_size_bytes) {
    // Check if prepareStreaming() was called
    if (!impl_->format_ctx || impl_->audio_stream_index == -1) {
        LOG_ERROR("streamPCM() called without prepareStreaming()");
        return ErrorCode::InvalidOperation;
    }

    LOG_INFO("Streaming PCM data in chunks: {} bytes", chunk_size_bytes);

    // Get codec parameters
    AVCodecParameters* codec_par = impl_->format_ctx->streams[impl_->audio_stream_index]->codecpar;

    // Initialize decoder
    const AVCodec* codec = avcodec_find_decoder(codec_par->codec_id);
    if (!codec) {
        LOG_ERROR("Codec not found for codec_id: {}", codec_par->codec_id);
        return ErrorCode::UnsupportedFormat;
    }

    impl_->codec_ctx = avcodec_alloc_context3(codec);
    if (!impl_->codec_ctx) {
        LOG_ERROR("Failed to allocate codec context");
        return ErrorCode::OutOfMemory;
    }

    int ret = avcodec_parameters_to_context(impl_->codec_ctx, codec_par);
    if (ret < 0) {
        LOG_ERROR("Failed to copy codec parameters");
        return ErrorCode::InvalidOperation;
    }

    ret = avcodec_open2(impl_->codec_ctx, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to open codec");
        return ErrorCode::InvalidOperation;
    }

    // Setup resampler
    // For DSD: if target_sample_rate is 0, use DSD rate / dsd_decimation
    // For non-DSD: if target_sample_rate is 0, use original rate
    int actual_target_rate;
    audio::AudioFormat format_enum = audio::AudioFormatUtils::formatFromExtension(impl_->metadata.file_path);

    if (format_enum == audio::AudioFormat::DSD && impl_->target_sample_rate == 0) {
        // DSD with no target specified: use DSD rate / dsd_decimation
        actual_target_rate = impl_->codec_ctx->sample_rate / impl_->dsd_decimation;
    } else if (impl_->target_sample_rate > 0) {
        actual_target_rate = impl_->target_sample_rate;
    } else {
        actual_target_rate = impl_->codec_ctx->sample_rate;
    }

    LOG_INFO("Setting up resampler: requested_rate={}, actual_rate={}, original_rate={}",
             impl_->target_sample_rate, actual_target_rate, impl_->codec_ctx->sample_rate);

    AVChannelLayout target_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&impl_->swr_ctx,
                        &target_ch_layout,
                        AV_SAMPLE_FMT_FLTP,
                        actual_target_rate,
                        &impl_->codec_ctx->ch_layout,
                        impl_->codec_ctx->sample_fmt,
                        impl_->codec_ctx->sample_rate,
                        0, nullptr);

    if (!impl_->swr_ctx || swr_init(impl_->swr_ctx) < 0) {
        LOG_ERROR("Failed to initialize resampler");
        return ErrorCode::InvalidOperation;
    }

    // Verify that actual_target_rate matches the expected output sample rate
    // (they should match since prepareStreaming already calculated this)
    if (actual_target_rate != impl_->metadata.sample_rate) {
        LOG_WARN("Sample rate mismatch: prepareStreaming set {}, but streamPCM calculated {}",
                 impl_->metadata.sample_rate, actual_target_rate);
    }

    // Decode and stream in chunks
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    // Pre-allocated output buffer for resampling
    int max_output_samples = av_rescale_rnd(
        frame->nb_samples ? frame->nb_samples : 1024,
        actual_target_rate,
        impl_->codec_ctx->sample_rate,
        AV_ROUND_UP
    );

    std::vector<uint8_t> out_buffer[2];  // Stereo planar
    out_buffer[0].resize(max_output_samples * sizeof(float));
    out_buffer[1].resize(max_output_samples * sizeof(float));
    uint8_t* out_data[2] = { out_buffer[0].data(), out_buffer[1].data() };

    // Chunk buffer for accumulating samples before callback
    size_t chunk_size_samples = chunk_size_bytes / sizeof(float);
    std::vector<float> chunk_buffer;
    chunk_buffer.reserve(chunk_size_samples);

    int packet_count = 0;
    int frame_count = 0;
    int chunk_count = 0;

    auto flush_chunk = [&]() {
        if (!chunk_buffer.empty()) {
            chunk_count++;
            LOG_DEBUG("Sending chunk {}: {} samples ({} bytes)",
                     chunk_count, chunk_buffer.size(), chunk_buffer.size() * sizeof(float));
            bool continue_streaming = callback(chunk_buffer.data(), chunk_buffer.size());
            chunk_buffer.clear();
            return continue_streaming;
        }
        return true;
    };

    // Main decoding loop
    while (av_read_frame(impl_->format_ctx, packet) >= 0) {
        packet_count++;
        if (packet->stream_index == impl_->audio_stream_index) {
            int send_result = avcodec_send_packet(impl_->codec_ctx, packet);
            if (send_result == 0) {
                while (avcodec_receive_frame(impl_->codec_ctx, frame) == 0) {
                    frame_count++;

                    // Calculate output samples
                    int out_samples = av_rescale_rnd(
                        swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate) + frame->nb_samples,
                        actual_target_rate,
                        impl_->codec_ctx->sample_rate,
                        AV_ROUND_UP
                    );

                    // Resize buffer if needed
                    if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
                        out_buffer[0].resize(out_samples * sizeof(float));
                        out_buffer[1].resize(out_samples * sizeof(float));
                        out_data[0] = out_buffer[0].data();
                        out_data[1] = out_buffer[1].data();
                    }

                    // Resample
                    int converted_samples = swr_convert(
                        impl_->swr_ctx,
                        out_data, out_samples,
                        const_cast<const uint8_t**>(frame->data),
                        frame->nb_samples
                    );

                    if (converted_samples < 0) {
                        LOG_ERROR("swr_convert failed: {}", converted_samples);
                        continue;
                    }

                    // Convert planar to interleaved
                    float* left_channel = reinterpret_cast<float*>(out_data[0]);
                    float* right_channel = reinterpret_cast<float*>(out_data[1]);

                    for (int i = 0; i < converted_samples; ++i) {
                        chunk_buffer.push_back(left_channel[i]);
                        chunk_buffer.push_back(right_channel[i]);

                        // Flush chunk when buffer is full
                        if (chunk_buffer.size() >= chunk_size_samples) {
                            if (!flush_chunk()) {
                                LOG_INFO("Streaming stopped by callback");
                                av_packet_unref(packet);
                                goto cleanup;
                            }
                        }
                    }
                }
            } else {
                LOG_ERROR("avcodec_send_packet failed: {}", send_result);
            }
        }
        av_packet_unref(packet);
    }

    // Flush decoder
    avcodec_send_packet(impl_->codec_ctx, nullptr);
    while (avcodec_receive_frame(impl_->codec_ctx, frame) == 0) {
        // Calculate output samples for flush
        int out_samples = av_rescale_rnd(
            swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate) + frame->nb_samples,
            actual_target_rate,
            impl_->codec_ctx->sample_rate,
            AV_ROUND_UP
        );

        // Resize buffer if needed
        if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
            out_buffer[0].resize(out_samples * sizeof(float));
            out_buffer[1].resize(out_samples * sizeof(float));
            out_data[0] = out_buffer[0].data();
            out_data[1] = out_buffer[1].data();
        }

        // Resample
        int converted_samples = swr_convert(
            impl_->swr_ctx,
            out_data, out_samples,
            const_cast<const uint8_t**>(frame->data),
            frame->nb_samples
        );

        if (converted_samples < 0) {
            LOG_ERROR("swr_convert failed during flush: {}", converted_samples);
            continue;
        }

        // Convert planar to interleaved
        float* left_channel = reinterpret_cast<float*>(out_data[0]);
        float* right_channel = reinterpret_cast<float*>(out_data[1]);

        for (int i = 0; i < converted_samples; ++i) {
            chunk_buffer.push_back(left_channel[i]);
            chunk_buffer.push_back(right_channel[i]);

            // Flush chunk when buffer is full
            if (chunk_buffer.size() >= chunk_size_samples) {
                if (!flush_chunk()) {
                    LOG_INFO("Streaming stopped by callback during flush");
                    goto cleanup;
                }
            }
        }
    }

    // Flush any remaining samples in resampler
    while (true) {
        int out_samples = av_rescale_rnd(
            swr_get_delay(impl_->swr_ctx, impl_->codec_ctx->sample_rate),
            actual_target_rate,
            impl_->codec_ctx->sample_rate,
            AV_ROUND_UP
        );

        if (out_samples == 0) break;

        // Resize buffer if needed
        if (static_cast<size_t>(out_samples) > out_buffer[0].size() / sizeof(float)) {
            out_buffer[0].resize(out_samples * sizeof(float));
            out_buffer[1].resize(out_samples * sizeof(float));
            out_data[0] = out_buffer[0].data();
            out_data[1] = out_buffer[1].data();
        }

        int converted_samples = swr_convert(impl_->swr_ctx, out_data, out_samples, nullptr, 0);
        if (converted_samples == 0) break;

        // Convert planar to interleaved
        float* left_channel = reinterpret_cast<float*>(out_data[0]);
        float* right_channel = reinterpret_cast<float*>(out_data[1]);

        for (int i = 0; i < converted_samples; ++i) {
            chunk_buffer.push_back(left_channel[i]);
            chunk_buffer.push_back(right_channel[i]);

            // Flush chunk when buffer is full
            if (chunk_buffer.size() >= chunk_size_samples) {
                if (!flush_chunk()) {
                    LOG_INFO("Streaming stopped by callback during resampler flush");
                    goto cleanup;
                }
            }
        }
    }

    // Flush final chunk
    flush_chunk();

cleanup:
    av_frame_free(&frame);
    av_packet_free(&packet);

    LOG_INFO("Streaming complete: {} packets, {} frames, {} chunks",
             packet_count, frame_count, chunk_count);

    return ErrorCode::Success;
}

ErrorCode AudioFileLoader::loadStreaming(const std::string& filepath,
                                         StreamingCallback callback,
                                         size_t chunk_size_bytes) {
    LOG_INFO("Loading audio file in streaming mode (legacy one-shot): {}", filepath);
    LOG_INFO("  Chunk size: {} bytes", chunk_size_bytes);

    // Prepare streaming (extract metadata)
    ErrorCode ret = prepareStreaming(filepath);
    if (ret != ErrorCode::Success) {
        return ret;
    }

    // Stream PCM data
    return streamPCM(callback, chunk_size_bytes);
}

const protocol::AudioMetadata& AudioFileLoader::getMetadata() const {
    return impl_->metadata;
}

const std::vector<uint8_t>& AudioFileLoader::getPCMData() const {
    return impl_->pcm_data;
}

bool AudioFileLoader::isLoaded() const {
    return impl_->loaded;
}

} // namespace load
} // namespace xpu
