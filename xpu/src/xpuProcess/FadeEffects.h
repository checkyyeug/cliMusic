/**
 * @file FadeEffects.h
 * @brief Fade effects (fade-in, fade-out, cross-fade)
 */

#ifndef XPU_PROCESS_FADE_EFFECTS_H
#define XPU_PROCESS_FADE_EFFECTS_H

#include "protocol/ErrorCode.h"
#include <vector>

namespace xpu {
namespace process {

/**
 * @brief Fade type
 */
enum class FadeType {
    In,      // Fade from silence to full
    Out,     // Fade from full to silence
    Cross    // Cross-fade between two tracks
};

/**
 * @brief Fade processor
 */
class FadeEffects {
public:
    FadeEffects();
    ~FadeEffects() = default;

    /**
     * @brief Configure fade effect
     * @param type Fade type
     * @param duration_ms Duration in milliseconds
     * @param sample_rate Sample rate
     */
    ErrorCode configure(FadeType type, int duration_ms, int sample_rate);

    /**
     * @brief Process audio data with fade
     * @param data Interleaved float samples
     * @param frames Number of frames
     * @param channels Number of channels
     */
    void process(float* data, int frames, int channels);

    /**
     * @brief Reset fade state
     */
    void reset();

    /**
     * @brief Check if fade is complete
     */
    bool isComplete() const;

private:
    FadeType type_;
    int duration_samples_;
    int current_sample_;
    int channels_;
    bool complete_;

    /**
     * @brief Get fade gain for current position
     */
    float getFadeGain() const;
};

} // namespace process
} // namespace xpu

#endif // XPU_PROCESS_FADE_EFFECTS_H
