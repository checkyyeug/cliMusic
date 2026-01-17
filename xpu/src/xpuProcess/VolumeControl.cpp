/**
 * @file VolumeControl.cpp
 * @brief Volume control implementation
 */

#include "VolumeControl.h"
#include <algorithm>

using namespace xpu;

namespace xpu {
namespace process {

VolumeControl::VolumeControl()
    : volume_(1.0f) {}

void VolumeControl::setVolume(float volume) {
    // Clamp volume to valid range
    volume_ = std::max(0.0f, std::min(2.0f, volume));
}

float VolumeControl::getVolume() const {
    return volume_;
}

void VolumeControl::process(float* data, int frames, int channels) {
    if (volume_ == 1.0f) {
        return;  // No change needed
    }

    int samples = frames * channels;

    for (int i = 0; i < samples; ++i) {
        data[i] *= volume_;

        // Soft clipping to prevent distortion when volume > 1.0
        if (data[i] > 1.0f) {
            data[i] = 1.0f;
        } else if (data[i] < -1.0f) {
            data[i] = -1.0f;
        }
    }
}

} // namespace process
} // namespace xpu
