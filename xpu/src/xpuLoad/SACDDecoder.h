/**
 * @file SACDDecoder.h
 * @brief DSD decoder using foo_input_sacd.dll
 */

#ifndef XPU_LOAD_SACD_DECODER_H
#define XPU_LOAD_SACD_DECODER_H

#include "protocol/ErrorCode.h"
#include "audio/AudioFormat.h"
#include "protocol/Protocol.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace xpu {
namespace load {

/**
 * @brief Callback type for streaming mode
 * @param chunk_data Pointer to chunk data (interleaved float samples)
 * @param chunk_samples Number of float samples in chunk
 * @return true to continue streaming, false to stop
 */
using StreamingCallback = std::function<bool(const float* chunk_data, size_t chunk_samples)>;

/**
 * @brief DSD decoder using foo_input_sacd.dll
 *
 * This decoder uses the foo_input_sacd.dll plugin from foobar2000
 * to decode DSD files (DSF/DSDIFF) to PCM.
 *
 * foo_input_sacd.dll advantages:
 * - High quality DSD decoding
 * - Proper handling of DSD metadata
 * - Better than FFmpeg's dsd2pcm for some files
 */
class SACDDecoder {
public:
    SACDDecoder();
    ~SACDDecoder();

    /**
     * @brief Set target sample rate for output
     * @param sample_rate Target sample rate (0 = DSD_rate/32, e.g., 88200 for DSD64)
     */
    void setTargetSampleRate(int sample_rate);

    /**
     * @brief Prepare for streaming (opens file and extracts metadata)
     * @param filepath Path to DSD file (.dsf or .dff)
     * @return ErrorCode::Success on success, error code otherwise
     */
    ErrorCode prepareStreaming(const std::string& filepath);

    /**
     * @brief Stream PCM data using callback
     * @param callback Callback function for each chunk
     * @param chunk_size_bytes Target chunk size (default: 64KB)
     * @return ErrorCode::Success on success, error code otherwise
     */
    ErrorCode streamPCM(StreamingCallback callback, size_t chunk_size_bytes = 64 * 1024);

    /**
     * @brief Get metadata (valid after prepareStreaming())
     */
    const protocol::AudioMetadata& getMetadata() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace load
} // namespace xpu

#endif // XPU_LOAD_SACD_DECODER_H
