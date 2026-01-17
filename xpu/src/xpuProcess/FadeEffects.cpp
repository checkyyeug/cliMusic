/**
 * @file FadeEffects.cpp
 * @brief Fade effects implementation
 */

#include "FadeEffects.h"
#include <cmath>

using namespace xpu;

namespace xpu {
namespace process {

FadeEffects::FadeEffects()
    : type_(FadeType::In)
    , duration_samples_(0)
    , current_sample_(0)
    , channels_(2)
    , complete_(false) {}

ErrorCode FadeEffects::configure(FadeType type, int duration_ms, int sample_rate) {
    type_ = type;
    duration_samples_ = (duration_ms * sample_rate) / 1000;
    current_sample_ = 0;
    complete_ = false;

    return ErrorCode::Success;
}

void FadeEffects::process(float* data, int frames, int channels) {
    channels_ = channels;

    for (int i = 0; i < frames; ++i) {
        if (complete_) {
            // Fade complete, apply final gain
            float final_gain = (type_ == FadeType::Out) ? 0.0f : 1.0f;
            for (int ch = 0; ch < channels; ++ch) {
                data[i * channels + ch] *= final_gain;
            }
        } else {
            // Apply fade gain
            float gain = getFadeGain();
            for (int ch = 0; ch < channels; ++ch) {
                data[i * channels + ch] *= gain;
            }

            current_sample_++;

            // Check if fade is complete
            if (current_sample_ >= duration_samples_) {
                complete_ = true;
            }
        }
    }
}

void FadeEffects::reset() {
    current_sample_ = 0;
    complete_ = false;
}

bool FadeEffects::isComplete() const {
    return complete_;
}

float FadeEffects::getFadeGain() const {
    if (duration_samples_ == 0) {
        return 1.0f;
    }

    float progress = static_cast<float>(current_sample_) / duration_samples_;
    progress = std::max(0.0f, std::min(1.0f, progress));

    // Linear fade
    if (type_ == FadeType::In) {
        return progress;  // 0.0 to 1.0
    } else {
        return 1.0f - progress;  // 1.0 to 0.0
    }
}

} // namespace process
} // namespace xpu
