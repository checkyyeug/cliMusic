/**
 * @file CacheManager.cpp
 * @brief Cache management implementation
 */

#include "CacheManager.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <fstream>
#include <nlohmann/json.hpp>

using namespace xpu;
using json = nlohmann::json;

namespace xpu {
namespace in2wav {

std::string CacheManager::getMetaFilePath(const std::string& cache_id) {
    return utils::PlatformUtils::getCacheDirectory() + "/" + cache_id + "_meta.json";
}

std::string CacheManager::getMagnitudeFilePath(const std::string& cache_id) {
    return utils::PlatformUtils::getCacheDirectory() + "/" + cache_id + "_magnitude.bin";
}

std::string CacheManager::getPhaseFilePath(const std::string& cache_id) {
    return utils::PlatformUtils::getCacheDirectory() + "/" + cache_id + "_phase.bin";
}

ErrorCode CacheManager::loadMetadata(const std::string& cache_id,
                                     FFTCacheMetadata& metadata) {
    std::string meta_path = getMetaFilePath(cache_id);

    std::ifstream file(meta_path);
    if (!file.is_open()) {
        return ErrorCode::FileReadError;
    }

    try {
        json j;
        file >> j;

        metadata.fft_size = j.value("fft_size", 2048);
        metadata.overlap_ratio = j.value("overlap_ratio", 2);
        metadata.window_function = j.value("window_function", "hann");
        metadata.sample_rate = j.value("sample_rate", 44100.0);
        metadata.channels = j.value("channels", 2);
        metadata.sample_count = j.value("sample_count", 0);
        metadata.version = j.value("version", "1.0");

        return ErrorCode::Success;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse cache metadata: {}", e.what());
        return ErrorCode::CacheCorrupted;
    }
}

ErrorCode CacheManager::saveMetadata(const std::string& cache_id,
                                     const FFTCacheMetadata& metadata) {
    std::string meta_path = getMetaFilePath(cache_id);

    try {
        json j;
        j["fft_size"] = metadata.fft_size;
        j["overlap_ratio"] = metadata.overlap_ratio;
        j["window_function"] = metadata.window_function;
        j["sample_rate"] = metadata.sample_rate;
        j["channels"] = metadata.channels;
        j["sample_count"] = metadata.sample_count;
        j["version"] = metadata.version;

        // Write to temp file first, then rename (atomic)
        std::string temp_path = meta_path + ".tmp";
        std::ofstream file(temp_path);
        if (!file.is_open()) {
            return ErrorCode::FileWriteError;
        }

        file << j.dump(2);
        file.close();

        // Atomic rename
        std::rename(temp_path.c_str(), meta_path.c_str());

        return ErrorCode::Success;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save cache metadata: {}", e.what());
        return ErrorCode::FileWriteError;
    }
}

ErrorCode CacheManager::initialize(const std::string& cache_dir) {
    // Ensure cache directory exists
    utils::PlatformUtils::ensureDirectories();

    LOG_INFO("Cache manager initialized: {}", cache_dir);
    return ErrorCode::Success;
}

bool CacheManager::hasCache(const std::string& cache_id) {
    std::string meta_path = getMetaFilePath(cache_id);
    std::ifstream file(meta_path);
    return file.good();
}

ErrorCode CacheManager::loadCache(const std::string& cache_id,
                                  FFTResult& result) {
    LOG_INFO("Loading FFT from cache: {}", cache_id);

    // Load metadata
    FFTCacheMetadata metadata;
    ErrorCode err = loadMetadata(cache_id, metadata);
    if (err != ErrorCode::Success) {
        return err;
    }

    // Load magnitude data
    std::string mag_path = getMagnitudeFilePath(cache_id);
    std::ifstream mag_file(mag_path, std::ios::binary);
    if (!mag_file.is_open()) {
        return ErrorCode::FileReadError;
    }

    result.magnitude.resize(metadata.fft_size / 2 + 1);
    mag_file.read(reinterpret_cast<char*>(result.magnitude.data()),
                  result.magnitude.size() * sizeof(float));
    mag_file.close();

    // Load phase data
    std::string phase_path = getPhaseFilePath(cache_id);
    std::ifstream phase_file(phase_path, std::ios::binary);
    if (!phase_file.is_open()) {
        return ErrorCode::FileReadError;
    }

    result.phase.resize(metadata.fft_size / 2 + 1);
    phase_file.read(reinterpret_cast<char*>(result.phase.data()),
                    result.phase.size() * sizeof(float));
    phase_file.close();

    result.fft_size = metadata.fft_size;

    LOG_INFO("FFT cache loaded successfully");
    return ErrorCode::Success;
}

ErrorCode CacheManager::saveCache(const std::string& cache_id,
                                  const FFTResult& result) {
    LOG_INFO("Saving FFT to cache: {}", cache_id);

    // Save metadata
    FFTCacheMetadata metadata;
    metadata.fft_size = result.fft_size;
    ErrorCode err = saveMetadata(cache_id, metadata);
    if (err != ErrorCode::Success) {
        return err;
    }

    // Save magnitude data
    std::string mag_path = getMagnitudeFilePath(cache_id);
    std::string mag_temp = mag_path + ".tmp";
    std::ofstream mag_file(mag_temp, std::ios::binary);
    if (!mag_file.is_open()) {
        return ErrorCode::FileWriteError;
    }

    mag_file.write(reinterpret_cast<const char*>(result.magnitude.data()),
                   result.magnitude.size() * sizeof(float));
    mag_file.close();

    std::rename(mag_temp.c_str(), mag_path.c_str());

    // Save phase data
    std::string phase_path = getPhaseFilePath(cache_id);
    std::string phase_temp = phase_path + ".tmp";
    std::ofstream phase_file(phase_temp, std::ios::binary);
    if (!phase_file.is_open()) {
        return ErrorCode::FileWriteError;
    }

    phase_file.write(reinterpret_cast<const char*>(result.phase.data()),
                     result.phase.size() * sizeof(float));
    phase_file.close();

    std::rename(phase_temp.c_str(), phase_path.c_str());

    LOG_INFO("FFT cache saved successfully");
    return ErrorCode::Success;
}

ErrorCode CacheManager::deleteCache(const std::string& cache_id) {
    std::string meta_path = getMetaFilePath(cache_id);
    std::string mag_path = getMagnitudeFilePath(cache_id);
    std::string phase_path = getPhaseFilePath(cache_id);

    std::remove(meta_path.c_str());
    std::remove(mag_path.c_str());
    std::remove(phase_path.c_str());

    LOG_INFO("FFT cache deleted: {}", cache_id);
    return ErrorCode::Success;
}

ErrorCode CacheManager::clearAllCache() {
    // TODO: Implement clearing all cache entries
    LOG_INFO("Clearing all FFT cache");
    return ErrorCode::NotImplemented;
}

uint64_t CacheManager::getCacheSize() {
    // TODO: Implement cache size calculation
    return 0;
}

bool CacheManager::validateCache(const std::string& cache_id) {
    // TODO: Implement cache validation
    return hasCache(cache_id);
}

} // namespace in2wav
} // namespace xpu
