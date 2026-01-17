/**
 * @file FFTEngine.h
 * @brief FFT computation engine with caching
 *
 * Core performance module: 10-100x speedup target
 */

#ifndef XPU_IN2WAV_FFT_ENGINE_H
#define XPU_IN2WAV_FFT_ENGINE_H

#include "protocol/ErrorCode.h"
#include <string>
#include <vector>
#include <complex>
#include <memory>

extern "C" {
#include <fftw3.h>
}

namespace xpu {
namespace in2wav {

/**
 * @brief FFT cache metadata
 */
struct FFTCacheMetadata {
    int fft_size;
    int overlap_ratio;
    std::string window_function;
    double sample_rate;
    int channels;
    int64_t sample_count;
    std::string version;  // Cache format version
    bool simd_enabled;
    int num_threads;

    FFTCacheMetadata()
        : fft_size(2048)
        , overlap_ratio(2)  // 50% overlap
        , window_function("hann")
        , sample_rate(44100.0)
        , channels(2)
        , sample_count(0)
        , version("1.0")
        , simd_enabled(false)
        , num_threads(1) {}
};

/**
 * @brief FFT computation result
 */
struct FFTResult {
    std::vector<float> magnitude;  // dB scale
    std::vector<float> phase;      // Radians
    std::vector<float> frequencies; // Hz
    int fft_size;

    FFTResult()
        : fft_size(2048) {
        magnitude.resize(fft_size / 2 + 1);
        phase.resize(fft_size / 2 + 1);
        frequencies.resize(fft_size / 2 + 1);
    }
};

/**
 * @brief FFT cache entry
 */
struct FFTCacheEntry {
    FFTCacheMetadata metadata;
    std::vector<float> magnitude;
    std::vector<float> phase;
    std::string cache_id;

    bool isValid() const;
};

/**
 * @brief FFT engine with caching
 *
 * Performance targets:
 * - First run: <30s for 5-minute song
 * - Cached run: <3s (10x minimum, 10-100x target)
 */
class FFTEngine {
public:
    FFTEngine();
    ~FFTEngine();

    /**
     * @brief Initialize FFT engine
     */
    ErrorCode initialize(int fft_size = 2048);

    /**
     * @brief Compute FFT for audio data
     */
    ErrorCode computeFFT(const std::vector<float>& audio_data,
                         int sample_rate,
                         FFTResult& result);

    /**
     * @brief Compute FFT with caching
     */
    ErrorCode computeFFTWithCache(const std::string& cache_id,
                                   const std::vector<float>& audio_data,
                                   int sample_rate,
                                   FFTResult& result);

    /**
     * @brief Load FFT from cache
     */
    ErrorCode loadFromCache(const std::string& cache_id,
                            FFTResult& result);

    /**
     * @brief Save FFT to cache
     */
    ErrorCode saveToCache(const std::string& cache_id,
                          const FFTResult& result);

    /**
     * @brief Check if cache exists and is valid
     */
    bool hasValidCache(const std::string& cache_id) const;

    /**
     * @brief Clear cache
     */
    ErrorCode clearCache(const std::string& cache_id);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        int hit_count;
        int miss_count;
        double hit_rate;
        uint64_t total_cache_size;

        CacheStats()
            : hit_count(0)
            , miss_count(0)
            , hit_rate(0.0)
            , total_cache_size(0) {}
    };

    CacheStats getCacheStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Generate cache key from audio data
     */
    std::string generateCacheKey(const std::vector<float>& audio_data,
                                 int sample_rate,
                                 int fft_size) const;

    /**
     * @brief Apply window function
     */
    void applyWindow(const std::vector<float>& input,
                     std::vector<float>& output,
                     const std::string& window) const;

    /**
     * @brief Convert magnitude to dB
     */
    float magnitudeToDB(float magnitude) const;
};

} // namespace in2wav
} // namespace xpu

#endif // XPU_IN2WAV_FFT_ENGINE_H
