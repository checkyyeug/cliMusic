#ifndef XPU_AUDIO_FORMAT_H
#define XPU_AUDIO_FORMAT_H

#include <string>
#include <cstdint>

namespace xpu {
namespace audio {

/**
 * @brief Audio format enumeration
 */
enum class AudioFormat : int {
    Unknown,
    FLAC,
    WAV,
    ALAC,
    MP3,
    AAC,
    OGG,
    OPUS,
    DSD,      // DSF/DSD format
    DSDIFF,   // DSDIFF format
    AIFF,
    AIFC
};

/**
 * @brief Audio sample format
 */
enum class SampleFormat : int {
    Unknown,
    UInt8,
    Int16,
    Int24,
    Int32,
    Float32,
    Float64,
    DSD1      // 1-bit DSD
};

/**
 * @brief Audio format information
 */
struct AudioFormatInfo {
    AudioFormat format;
    SampleFormat sample_format;
    int sample_rate;
    int bit_depth;
    int channels;
    bool is_big_endian;
    bool is_signed;
    bool is_floating_point;
    int frame_size;  // bytes per frame
    int block_align;

    AudioFormatInfo()
        : format(AudioFormat::Unknown)
        , sample_format(SampleFormat::Unknown)
        , sample_rate(0)
        , bit_depth(0)
        , channels(0)
        , is_big_endian(false)
        , is_signed(true)
        , is_floating_point(false)
        , frame_size(0)
        , block_align(0) {}
};

/**
 * @brief Audio format utilities
 */
class AudioFormatUtils {
public:
    /**
     * @brief Get format from file extension
     */
    static AudioFormat formatFromExtension(const std::string& filepath) {
        size_t dot_pos = filepath.find_last_of('.');
        if (dot_pos == std::string::npos) {
            return AudioFormat::Unknown;
        }

        std::string ext = filepath.substr(dot_pos + 1);

        // Convert to lowercase
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(c));
        }

        if (ext == "flac") return AudioFormat::FLAC;
        if (ext == "wav" || ext == "wave") return AudioFormat::WAV;
        if (ext == "alac") return AudioFormat::ALAC;
        if (ext == "m4a") return AudioFormat::ALAC;
        if (ext == "mp3") return AudioFormat::MP3;
        if (ext == "aac") return AudioFormat::AAC;
        if (ext == "ogg") return AudioFormat::OGG;
        if (ext == "opus") return AudioFormat::OPUS;
        if (ext == "dsf") return AudioFormat::DSD;
        if (ext == "dsd") return AudioFormat::DSD;
        if (ext == "dff") return AudioFormat::DSDIFF;
        if (ext == "aiff" || ext == "aif") return AudioFormat::AIFF;
        if (ext == "aifc") return AudioFormat::AIFC;

        return AudioFormat::Unknown;
    }

    /**
     * @brief Get format name
     */
    static std::string formatToString(AudioFormat format) {
        switch (format) {
            case AudioFormat::FLAC: return "FLAC";
            case AudioFormat::WAV: return "WAV";
            case AudioFormat::ALAC: return "ALAC";
            case AudioFormat::MP3: return "MP3";
            case AudioFormat::AAC: return "AAC";
            case AudioFormat::OGG: return "OGG";
            case AudioFormat::OPUS: return "OPUS";
            case AudioFormat::DSD: return "DSD";
            case AudioFormat::DSDIFF: return "DSDIFF";
            case AudioFormat::AIFF: return "AIFF";
            case AudioFormat::AIFC: return "AIFC";
            default: return "Unknown";
        }
    }

    /**
     * @brief Get sample format name
     */
    static std::string sampleFormatToString(SampleFormat format) {
        switch (format) {
            case SampleFormat::UInt8: return "UInt8";
            case SampleFormat::Int16: return "Int16";
            case SampleFormat::Int24: return "Int24";
            case SampleFormat::Int32: return "Int32";
            case SampleFormat::Float32: return "Float32";
            case SampleFormat::Float64: return "Float64";
            case SampleFormat::DSD1: return "DSD1";
            default: return "Unknown";
        }
    }

    /**
     * @brief Get bytes per sample for sample format
     */
    static int getBytesPerSample(SampleFormat format) {
        switch (format) {
            case SampleFormat::UInt8: return 1;
            case SampleFormat::Int16: return 2;
            case SampleFormat::Int24: return 3;
            case SampleFormat::Int32: return 4;
            case SampleFormat::Float32: return 4;
            case SampleFormat::Float64: return 8;
            case SampleFormat::DSD1: return 1;  // 1 bit per sample
            default: return 0;
        }
    }

    /**
     * @brief Check if format is lossless
     */
    static bool isLossless(AudioFormat format) {
        return format == AudioFormat::FLAC ||
               format == AudioFormat::WAV ||
               format == AudioFormat::ALAC ||
               format == AudioFormat::DSD ||
               format == AudioFormat::DSDIFF ||
               format == AudioFormat::AIFF ||
               format == AudioFormat::AIFC;
    }

    /**
     * @brief Check if format supports high sample rates
     */
    static bool supportsHighSampleRate(AudioFormat format) {
        return format == AudioFormat::FLAC ||
               format == AudioFormat::WAV ||
               format == AudioFormat::ALAC ||
               format == AudioFormat::DSD ||
               format == AudioFormat::DSDIFF;
    }

    /**
     * @brief Calculate frame size
     */
    static int calculateFrameSize(int channels, SampleFormat sample_format) {
        return channels * getBytesPerSample(sample_format);
    }

    /**
     * @brief Calculate duration from sample count
     */
    static double calculateDuration(uint64_t sample_count, int sample_rate) {
        if (sample_rate == 0) return 0.0;
        return static_cast<double>(sample_count) / sample_rate;
    }

    /**
     * @brief Calculate sample count from duration
     */
    static uint64_t calculateSampleCount(double duration, int sample_rate) {
        return static_cast<uint64_t>(duration * sample_rate);
    }

    /**
     * @brief Get supported formats string
     */
    static std::string getSupportedFormats() {
        return "FLAC,WAV,ALAC,MP3,AAC,OGG,OPUS,DSD,DSDIFF,AIFF,AIFC";
    }

    /**
     * @brief Check if high resolution audio
     */
    static bool isHighResolution(int sample_rate, int bit_depth) {
        return sample_rate > 48000 || bit_depth > 16;
    }

    /**
     * @brief Check if standard sample rate
     * @note Includes standard rates and high-resolution rates up to DSD1024 (2.8224 MHz)
     * @param sample_rate Sample rate in Hz to check
     * @return true if the sample rate is a standard/common rate
     */
    static bool isStandardSampleRate(int sample_rate) {
        return sample_rate == 44100 || sample_rate == 48000 ||
               sample_rate == 88200 || sample_rate == 96000 ||
               sample_rate == 176400 || sample_rate == 192000 ||
               sample_rate == 352800 || sample_rate == 384000 ||
               sample_rate == 705600 || sample_rate == 768000 ||
               sample_rate == 1411200 || sample_rate == 2822400;
    }

    /**
     * @brief Check if valid bit depth
     */
    static bool isValidBitDepth(int bit_depth) {
        return bit_depth == 8 || bit_depth == 16 || bit_depth == 24 ||
               bit_depth == 32 || bit_depth == 64;
    }

    /**
     * @brief Check if valid channel count
     */
    static bool isValidChannelCount(int channels) {
        return channels >= 1 && channels <= 8;
    }
};

} // namespace audio
} // namespace xpu

#endif // XPU_AUDIO_FORMAT_H
