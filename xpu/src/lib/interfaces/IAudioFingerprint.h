#ifndef XPU_I_AUDIO_FINGERPRINT_H
#define XPU_I_AUDIO_FINGERPRINT_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <vector>

namespace xpu {

/**
 * @brief Fingerprint data structure
 */
struct FingerprintData {
    std::string fingerprint_id;
    std::vector<uint8_t> data;
    int version;
    std::string algorithm;  // "chromaprint", "acoustid", etc.

    FingerprintData()
        : version(1)
        , algorithm("chromaprint") {}
};

/**
 * @brief Metadata from online database
 */
struct OnlineMetadata {
    std::string musicbrainz_id;
    std::string acoustid_id;
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string genre;
    double confidence;

    OnlineMetadata()
        : confidence(0.0) {}
};

/**
 * @brief Audio fingerprint interface (Phase 3)
 *
 * Provides audio fingerprinting capabilities for music identification
 */
class IAudioFingerprint {
public:
    virtual ~IAudioFingerprint() = default;

    /**
     * @brief Compute fingerprint from audio file
     */
    virtual ErrorCode computeFingerprint(const std::string& audio_file,
                                         FingerprintData& result) = 0;

    /**
     * @brief Get fingerprint from cache
     */
    virtual ErrorCode fingerprintFromCache(const std::string& cache_id,
                                           FingerprintData& result) = 0;

    /**
     * @brief Compare two fingerprints
     * @return Similarity score (0.0 to 1.0)
     */
    virtual ErrorCode compareFingerprints(const FingerprintData& fp1,
                                          const FingerprintData& fp2,
                                          float& similarity) = 0;

    /**
     * @brief Query online database with fingerprint
     */
    virtual ErrorCode queryOnlineDatabase(const FingerprintData& fp,
                                          OnlineMetadata& metadata) = 0;

    /**
     * @brief Check if interface is available
     * @return false in Phase 1, true in Phase 3+
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
class AudioFingerprintStub : public IAudioFingerprint {
public:
    ErrorCode computeFingerprint(const std::string& audio_file,
                                 FingerprintData& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode fingerprintFromCache(const std::string& cache_id,
                                   FingerprintData& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode compareFingerprints(const FingerprintData& fp1,
                                  const FingerprintData& fp2,
                                  float& similarity) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode queryOnlineDatabase(const FingerprintData& fp,
                                  OnlineMetadata& metadata) override {
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

#endif // XPU_I_AUDIO_FINGERPRINT_H
