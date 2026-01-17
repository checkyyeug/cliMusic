/**
 * @file AudioFileLoader.h
 * @brief Audio file loader implementation
 */

#ifndef XPU_LOAD_AUDIO_FILE_LOADER_H
#define XPU_LOAD_AUDIO_FILE_LOADER_H

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
 * @brief Audio file loader class
 */
class AudioFileLoader {
public:
    AudioFileLoader();
    ~AudioFileLoader();

    /**
     * @brief Set target sample rate for output
     * @param sample_rate Target sample rate (0 = keep original, e.g., 48000, 96000)
     */
    void setTargetSampleRate(int sample_rate);

    /**
     * @brief Set DSD decimation factor for output
     * @param factor Decimation factor: 16, 32, or 64 (default: 16)
     *               Higher factors = lower quality but less CPU/memory
     *               For DSD64: /16 = 176.4kHz, /32 = 88.2kHz, /64 = 44.1kHz
     * @note Only affects DSD files when target_sample_rate is 0
     */
    void setDSDDecimation(int factor);

    /**
     * @brief Load audio file (batch mode - loads entire file into memory)
     */
    ErrorCode load(const std::string& filepath);

    /**
     * @brief Prepare for streaming (opens file and extracts metadata)
     * @param filepath Path to audio file
     * @return ErrorCode::Success on success, error code otherwise
     *
     * This method opens the file and extracts metadata WITHOUT decoding PCM data.
     * Call getMetadata() after this to retrieve the metadata.
     * Then call streamPCM() to start streaming PCM data.
     *
     * Example workflow:
     *   1. prepareStreaming(filepath) - Opens file and extracts metadata
     *   2. getMetadata() - Retrieve metadata
     *   3. Output metadata to stdout
     *   4. streamPCM(callback) - Stream PCM data
     */
    ErrorCode prepareStreaming(const std::string& filepath);

    /**
     * @brief Stream PCM data using callback (requires prepareStreaming() first)
     * @param callback Callback function for each chunk
     * @param chunk_size_bytes Target chunk size (default: 64KB)
     * @return ErrorCode::Success on success, error code otherwise
     *
     * NOTE: You MUST call prepareStreaming() before this method.
     */
    ErrorCode streamPCM(StreamingCallback callback, size_t chunk_size_bytes = 64 * 1024);

    /**
     * @brief Load and stream audio file in chunks (legacy one-shot method)
     * @param filepath Path to audio file
     * @param callback Callback function for each chunk
     * @param chunk_size_bytes Target chunk size (default: 64KB)
     * @return ErrorCode::Success on success, error code otherwise
     *
     * NOTE: This is a legacy method that combines prepareStreaming() + streamPCM().
     * The caller is responsible for outputting metadata before calling this method.
     *
     * Recommended workflow for new code:
     *   1. prepareStreaming(filepath) - Extract metadata
     *   2. getMetadata() - Retrieve metadata
     *   3. Output metadata to stdout
     *   4. streamPCM(callback) - Stream PCM data
     */
    ErrorCode loadStreaming(const std::string& filepath,
                           StreamingCallback callback,
                           size_t chunk_size_bytes = 64 * 1024);

    /**
     * @brief Get metadata (valid after load() or prepareStreaming())
     */
    const protocol::AudioMetadata& getMetadata() const;

    /**
     * @brief Get PCM data
     */
    const std::vector<uint8_t>& getPCMData() const;

    /**
     * @brief Check if file is loaded
     */
    bool isLoaded() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace load
} // namespace xpu

#endif // XPU_LOAD_AUDIO_FILE_LOADER_H
