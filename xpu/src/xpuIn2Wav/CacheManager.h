/**
 * @file CacheManager.h
 * @brief FFT cache management
 */

#ifndef XPU_IN2WAV_CACHE_MANAGER_H
#define XPU_IN2WAV_CACHE_MANAGER_H

#include "protocol/ErrorCode.h"
#include "FFTEngine.h"
#include <string>
#include <map>

namespace xpu {
namespace in2wav {

/**
 * @brief Cache manager for FFT results
 */
class CacheManager {
public:
    /**
     * @brief Initialize cache manager
     */
    static ErrorCode initialize(const std::string& cache_dir);

    /**
     * @brief Check if cache exists
     */
    static bool hasCache(const std::string& cache_id);

    /**
     * @brief Load from cache
     */
    static ErrorCode loadCache(const std::string& cache_id,
                              FFTResult& result);

    /**
     * @brief Save to cache
     */
    static ErrorCode saveCache(const std::string& cache_id,
                              const FFTResult& result);

    /**
     * @brief Delete cache entry
     */
    static ErrorCode deleteCache(const std::string& cache_id);

    /**
     * @brief Clear all cache
     */
    static ErrorCode clearAllCache();

    /**
     * @brief Get cache size
     */
    static uint64_t getCacheSize();

    /**
     * @brief Validate cache
     */
    static bool validateCache(const std::string& cache_id);

private:
    /**
     * @brief Get cache file paths
     */
    static std::string getMetaFilePath(const std::string& cache_id);
    static std::string getMagnitudeFilePath(const std::string& cache_id);
    static std::string getPhaseFilePath(const std::string& cache_id);

    /**
     * @brief Load metadata
     */
    static ErrorCode loadMetadata(const std::string& cache_id,
                                  FFTCacheMetadata& metadata);

    /**
     * @brief Save metadata
     */
    static ErrorCode saveMetadata(const std::string& cache_id,
                                  const FFTCacheMetadata& metadata);
};

} // namespace in2wav
} // namespace xpu

#endif // XPU_IN2WAV_CACHE_MANAGER_H
