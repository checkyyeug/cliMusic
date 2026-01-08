#ifndef XPU_I_AUDIO_VISUALIZER_H
#define XPU_I_AUDIO_VISUALIZER_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <vector>

namespace xpu {

/**
 * @brief Spectrum data structure
 */
struct SpectrumData {
    std::vector<float> frequencies;
    std::vector<float> magnitudes;  // dB
    int fft_size;
    float min_freq;
    float max_freq;

    SpectrumData()
        : fft_size(2048)
        , min_freq(20.0f)
        , max_freq(20000.0f) {}
};

/**
 * @brief Waveform data structure
 */
struct WaveformData {
    std::vector<float> samples;
    int resolution;
    double start_time;
    double duration;

    WaveformData()
        : resolution(1024)
        , start_time(0.0)
        , duration(0.0) {}
};

/**
 * @brief Envelope data structure
 */
struct EnvelopeData {
    std::vector<float> attack;
    std::vector<float> decay;
    std::vector<float> sustain;
    std::vector<float> release;

    EnvelopeData() = default;
};

/**
 * @brief Visualization type
 */
enum class VisualizationType : int {
    Spectrum = 0,
    Waveform = 1,
    Spectrogram = 2,
    Envelope = 3
};

/**
 * @brief Image data structure
 */
struct ImageData {
    int width;
    int height;
    int channels;  // RGB = 3, RGBA = 4
    std::vector<uint8_t> pixels;

    ImageData()
        : width(800)
        , height(600)
        , channels(4) {}
};

/**
 * @brief Audio visualizer interface (Phase 3)
 *
 * Provides audio visualization capabilities
 */
class IAudioVisualizer {
public:
    virtual ~IAudioVisualizer() = default;

    /**
     * @brief Get spectrum data from cache
     */
    virtual ErrorCode getSpectrumData(const std::string& cache_id,
                                      SpectrumData& result,
                                      size_t resolution = 1024) = 0;

    /**
     * @brief Get waveform data from cache
     */
    virtual ErrorCode getWaveformData(const std::string& cache_id,
                                      WaveformData& result,
                                      size_t resolution = 1024) = 0;

    /**
     * @brief Get envelope data from cache
     */
    virtual ErrorCode getEnvelopeData(const std::string& cache_id,
                                      EnvelopeData& result) = 0;

    /**
     * @brief Generate visualization image
     */
    virtual ErrorCode generateVisualization(const std::string& cache_id,
                                            VisualizationType type,
                                            ImageData& result) = 0;

    /**
     * @brief Check if interface is available
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get feature status
     */
    virtual FeatureStatus getFeatureStatus() const = 0;
};

/**
 * @brief Stub implementation for Phase 1
 */
class AudioVisualizerStub : public IAudioVisualizer {
public:
    ErrorCode getSpectrumData(const std::string& cache_id,
                              SpectrumData& result,
                              size_t resolution = 1024) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode getWaveformData(const std::string& cache_id,
                              WaveformData& result,
                              size_t resolution = 1024) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode getEnvelopeData(const std::string& cache_id,
                              EnvelopeData& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode generateVisualization(const std::string& cache_id,
                                    VisualizationType type,
                                    ImageData& result) override {
        return ErrorCode::NotImplemented;
    }

    bool isAvailable() const override {
        return false;  // Not available in Phase 1
    }

    FeatureStatus getFeatureStatus() const override {
        return FeatureStatus::EXTENDED_V1;  // Phase 3
    }
};

} // namespace xpu

#endif // XPU_I_AUDIO_VISUALIZER_H
