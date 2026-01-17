/**
 * @file DSDDecoder.h
 * @brief DSD (Direct Stream Digital) format decoder
 * Supports DSF and DSDIFF formats
 */

#ifndef XPU_LOAD_DSD_DECODER_H
#define XPU_LOAD_DSD_DECODER_H

#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include "../lib/audio/AudioFormat.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace xpu {
namespace load {

/**
 * @brief DSD format types
 */
enum class DSDFormat {
    None,    // Not a DSD format
    DSF,     // Sony DSF format
    DSDIFF   // Philips DSDIFF format
};

/**
 * @brief Callback type for streaming mode
 * @param chunk_data Pointer to chunk data (interleaved float samples)
 * @param chunk_samples Number of float samples in chunk
 * @return true to continue streaming, false to stop
 */
using DSDStreamingCallback = std::function<bool(const float* chunk_data, size_t chunk_samples)>;

/**
 * @brief DSD decoder class
 * Decodes DSD audio data and converts to PCM
 */
class DSDDecoder {
public:
    DSDDecoder();
    ~DSDDecoder();

    /**
     * @brief Set target sample rate for output
     * @param sample_rate Target sample rate (e.g., 48000, 96000)
     */
    void setTargetSampleRate(int sample_rate);

    /**
     * @brief Set DSD decimation factor for output
     * @param factor Decimation factor: 16, 32, or 64 (default: 16)
     *               Higher factors = lower quality but less CPU/memory
     *               For DSD64: /16 = 176.4kHz, /32 = 88.2kHz, /64 = 44.1kHz
     */
    void setDSDDecimation(int factor);

    /**
     * @brief Load DSD file (batch mode - loads entire file into memory)
     * @param filepath Path to DSD file (.dsf or .dff)
     * @return ErrorCode::Success on success
     */
    ErrorCode load(const std::string& filepath);

    /**
     * @brief Prepare for streaming (opens file and extracts metadata)
     * @param filepath Path to DSD file (.dsf or .dff)
     * @return ErrorCode::Success on success
     *
     * This method opens the file and extracts metadata WITHOUT decoding DSD data.
     * Call getMetadata() after this to retrieve the metadata.
     * Then call streamPCM() to start streaming PCM data.
     */
    ErrorCode prepareStreaming(const std::string& filepath);

    /**
     * @brief Stream PCM data using callback (requires prepareStreaming() first)
     * @param callback Callback function for each chunk
     * @param chunk_size_bytes Target chunk size (default: 64KB)
     * @return ErrorCode::Success on success
     *
     * NOTE: You MUST call prepareStreaming() before this method.
     */
    ErrorCode streamPCM(DSDStreamingCallback callback, size_t chunk_size_bytes = 64 * 1024);

    /**
     * @brief Get metadata (valid after load() or prepareStreaming())
     */
    const protocol::AudioMetadata& getMetadata() const;

    /**
     * @brief Get decoded PCM data (32-bit float) - batch mode only
     * Output is always 48kHz stereo for compatibility
     */
    const std::vector<uint8_t>& getPCMData() const;

    /**
     * @brief Check if file is loaded
     */
    bool isLoaded() const;

    /**
     * @brief Detect DSD format from file
     */
    static DSDFormat detectFormat(const std::string& filepath);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Parse DSF format header
     */
    ErrorCode parseDSF(const std::string& filepath);

    /**
     * @brief Parse DSDIFF format header
     */
    ErrorCode parseDSDIFF(const std::string& filepath);

    /**
     * @brief Decode DSD data to PCM
     * Uses 5th-order noise shaping for professional quality
     */
    ErrorCode decodeDSDToPCM();
};

} // namespace load
} // namespace xpu

#endif // XPU_LOAD_DSD_DECODER_H
