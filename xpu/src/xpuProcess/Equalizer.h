/**
 * @file Equalizer.h
 * @brief 3-band equalizer (Bass, Mid, Treble)
 */

#ifndef XPU_PROCESS_EQUALIZER_H
#define XPU_PROCESS_EQUALIZER_H

#include "protocol/ErrorCode.h"
#include <vector>

namespace xpu {
namespace process {

/**
 * @brief EQ preset
 */
enum class EQPreset {
    Flat,
    Rock,
    Pop,
    Classical,
    Jazz,
    Electronic
};

/**
 * @brief 3-band equalizer
 */
class Equalizer {
public:
    Equalizer();
    ~Equalizer() = default;

    /**
     * @brief Set gain for band
     * @param band 0 = Bass (low), 1 = Mid, 2 = Treble (high)
     * @param gain_db Gain in dB (-20 to +20)
     */
    ErrorCode setBandGain(int band, float gain_db);

    /**
     * @brief Get gain for band
     */
    float getBandGain(int band) const;

    /**
     * @brief Load EQ preset
     */
    ErrorCode loadPreset(EQPreset preset);

    /**
     * @brief Process audio data
     * @param data Interleaved float samples
     * @param frames Number of frames
     * @param channels Number of channels
     * @param sample_rate Sample rate
     */
    void process(float* data, int frames, int channels, int sample_rate);

    /**
     * @brief Reset all gains to flat
     */
    void reset();

private:
    float bass_gain_db_;    // Low frequencies (< 200 Hz)
    float mid_gain_db_;     // Mid frequencies (200 Hz - 2 kHz)
    float treble_gain_db_;  // High frequencies (> 2 kHz)

    // Filter states
    struct FilterState {
        float x1, x2;  // Input history
        float y1, y2;  // Output history

        FilterState() : x1(0), x2(0), y1(0), y2(0) {}
    };

    FilterState bass_filter_[2];
    FilterState mid_filter_[2];
    FilterState treble_filter_[2];

    /**
     * @brief Apply shelving filter
     */
    void applyShelvingFilter(float* data, int frames,
                            float gain_db, float frequency,
                            float sample_rate, FilterState* state);

    /**
     * @brief Apply peaking filter
     */
    void applyPeakingFilter(float* data, int frames,
                           float gain_db, float frequency,
                           float Q, float sample_rate, FilterState* state);
};

} // namespace process
} // namespace xpu

#endif // XPU_PROCESS_EQUALIZER_H
