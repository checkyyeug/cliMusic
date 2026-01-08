#ifndef XPU_AUDIO_PROPERTIES_H
#define XPU_AUDIO_PROPERTIES_H

#include "AudioFormat.h"
#include <string>

namespace xpu {
namespace audio {

/**
 * @brief Audio properties container
 */
struct AudioProperties {
    AudioFormatInfo format_info;
    uint64_t total_samples;
    double duration;
    double bitrate;
    bool is_vbr;  // Variable bitrate
    int bits_per_sample;

    AudioProperties()
        : total_samples(0)
        , duration(0.0)
        , bitrate(0.0)
        , is_vbr(false)
        , bits_per_sample(0) {}
};

/**
 * @brief Audio properties calculator
 */
class AudioPropertiesCalculator {
public:
    /**
     * @brief Calculate bitrate from format and duration
     */
    static double calculateBitrate(const AudioFormatInfo& format_info,
                                    double duration,
                                    uint64_t file_size) {
        if (duration <= 0.0) return 0.0;

        // bitrate (bps) = file_size (bytes) * 8 / duration (seconds)
        double bitrate_bps = (file_size * 8.0) / duration;
        return bitrate_bps / 1000.0;  // Convert to kbps
    }

    /**
     * @brief Calculate file size from bitrate and duration
     */
    static uint64_t calculateFileSize(double bitrate_kbps,
                                      double duration) {
        // file_size (bytes) = bitrate (kbps) * duration (s) * 1000 / 8
        return static_cast<uint64_t>((bitrate_kbps * duration * 1000.0) / 8.0);
    }

    /**
     * @brief Check if format is high-resolution
     */
    static bool isHighResolution(const AudioFormatInfo& format_info) {
        // High-resolution: sample rate > 48kHz OR bit depth > 16-bit
        return format_info.sample_rate > 48000 ||
               format_info.bit_depth > 16;
    }

    /**
     * @brief Check if format is ultra-high-resolution
     */
    static bool isUltraHighResolution(const AudioFormatInfo& format_info) {
        // Ultra-high-resolution: sample rate >= 96kHz AND bit depth >= 24-bit
        return format_info.sample_rate >= 96000 &&
               format_info.bit_depth >= 24;
    }

    /**
     * @brief Check if format is professional grade
     */
    static bool isProfessionalGrade(const AudioFormatInfo& format_info) {
        // Professional: sample rate >= 384kHz AND bit depth >= 24-bit
        return format_info.sample_rate >= 384000 &&
               format_info.bit_depth >= 24;
    }
};

} // namespace audio
} // namespace xpu

#endif // XPU_AUDIO_PROPERTIES_H
