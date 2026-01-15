/**
 * @file FormatConverter.h
 * @brief Audio format conversion implementation
 */

#ifndef XPU_IN2WAV_FORMAT_CONVERTER_H
#define XPU_IN2WAV_FORMAT_CONVERTER_H

#include "protocol/ErrorCode.h"
#include "audio/AudioFormat.h"
#include <string>
#include <vector>
#include <memory>

namespace xpu {
namespace in2wav {

/**
 * @brief Streaming resampler for real-time processing
 */
class StreamingResampler {
public:
    StreamingResampler();
    ~StreamingResampler();

    /**
     * @brief Initialize the resampler
     */
    ErrorCode init(int input_rate, int output_rate, int channels, const char* quality = "medium");

    /**
     * @brief Process a chunk of audio data
     * @param input Input audio frames (interleaved)
     * @param input_frames Number of input frames
     * @param output Output buffer (will be resized)
     * @return Number of output frames generated
     */
    ErrorCode process(const float* input, int input_frames, std::vector<float>& output);

    /**
     * @brief Flush remaining data
     */
    ErrorCode flush(std::vector<float>& output);

    /**
     * @brief Check if resampling is needed
     */
    bool isActive() const { return input_rate_ != output_rate_; }

    /**
     * @brief Get ratios
     */
    double getRatio() const { return ratio_; }

private:
    int input_rate_;
    int output_rate_;
    int channels_;
    double ratio_;
    SRC_STATE* src_state_;
    bool initialized_;
};

/**
 * @brief Format converter class
 */
class FormatConverter {
public:
    /**
     * @brief Convert audio to WAV format from file
     */
    static ErrorCode convertToWAV(const std::string& input_file,
                                   const std::string& output_file,
                                   int sample_rate,
                                   int bit_depth,
                                   int channels,
                                   const char* quality = "medium");

    /**
     * @brief Convert audio to WAV format from stdin
     * Reads xpuLoad output format: [JSON metadata][8-byte size header][PCM data]
     */
    static ErrorCode convertStdinToWAV(const std::string& output_file,
                                      int sample_rate,
                                      int bit_depth,
                                      int channels,
                                      const char* quality = "medium");

    /**
     * @brief Convert audio to WAV format from stdin and output to stdout
     * Reads xpuLoad output format and outputs xpuPlay compatible format
     * Output format: [JSON metadata][8-byte size header][PCM data]
     */
    static ErrorCode convertStdinToStdout(int sample_rate,
                                         int bit_depth,
                                         int channels,
                                         const char* quality = "medium");

    /**
     * @brief Apply resampling
     */
    static ErrorCode resample(const std::vector<float>& input,
                              int input_rate,
                              int output_rate,
                              std::vector<float>& output,
                              const char* quality = "medium");

    /**
     * @brief Convert bit depth
     */
    static ErrorCode convertBitDepth(const std::vector<float>& input,
                                      int input_bits,
                                      int output_bits,
                                      std::vector<uint8_t>& output);
};

} // namespace in2wav
} // namespace xpu

#endif // XPU_IN2WAV_FORMAT_CONVERTER_H
