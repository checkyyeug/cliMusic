/**
 * @file VolumeControl.h
 * @brief Volume control (0-200%)
 */

#ifndef XPU_PROCESS_VOLUME_CONTROL_H
#define XPU_PROCESS_VOLUME_CONTROL_H

#include "protocol/ErrorCode.h"
#include <vector>

namespace xpu {
namespace process {

/**
 * @brief Volume control processor
 */
class VolumeControl {
public:
    VolumeControl();
    ~VolumeControl() = default;

    /**
     * @brief Set volume (0.0 = 0%, 1.0 = 100%, 2.0 = 200%)
     */
    void setVolume(float volume);

    /**
     * @brief Get current volume
     */
    float getVolume() const;

    /**
     * @brief Process audio data
     * @param data Interleaved float samples
     * @param frames Number of frames
     * @param channels Number of channels
     */
    void process(float* data, int frames, int channels);

private:
    float volume_;  // Linear gain (0.0 - 2.0)
};

} // namespace process
} // namespace xpu

#endif // XPU_PROCESS_VOLUME_CONTROL_H
