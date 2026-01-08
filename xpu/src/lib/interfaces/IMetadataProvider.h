#ifndef XPU_I_METADATA_PROVIDER_H
#define XPU_I_METADATA_PROVIDER_H

#include "protocol/ErrorCode.h"
#include "protocol/Protocol.h"
#include "interfaces/FeatureStatus.h"
#include "interfaces/IAudioFingerprint.h"
#include <string>
#include <map>

namespace xpu {

/**
 * @brief MusicBrainz metadata
 */
struct MusicBrainzMetadata {
    std::string recording_id;
    std::string artist_id;
    std::string release_id;
    std::string title;
    std::string artist;
    std::string album;
    std::string date;
    std::string country;
    std::vector<std::string> genres;
    std::vector<std::string> artist_credits;

    MusicBrainzMetadata() = default;
};

/**
 * @brief Acoustid metadata
 */
struct AcoustidMetadata {
    std::string acoustid_id;
    float score;
    std::vector<std::string> recording_ids;
    std::map<std::string, std::string> metadata;

    AcoustidMetadata()
        : score(0.0f) {}
};

/**
 * @brief Metadata provider interface (Phase 3)
 *
 * Provides online database metadata lookup
 */
class IMetadataProvider {
public:
    virtual ~IMetadataProvider() = default;

    /**
     * @brief Query MusicBrainz database
     */
    virtual ErrorCode queryMusicBrainz(const std::string& fingerprint,
                                       MusicBrainzMetadata& result) = 0;

    /**
     * @brief Query Acoustid database
     */
    virtual ErrorCode queryAcoustid(const std::string& fingerprint,
                                    AcoustidMetadata& result) = 0;

    /**
     * @brief Enrich metadata from multiple sources
     */
    virtual ErrorCode enrichMetadata(const std::string& audio_file,
                                      protocol::AudioMetadata& metadata) = 0;

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
class MetadataProviderStub : public IMetadataProvider {
public:
    ErrorCode queryMusicBrainz(const std::string& fingerprint,
                               MusicBrainzMetadata& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode queryAcoustid(const std::string& fingerprint,
                            AcoustidMetadata& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode enrichMetadata(const std::string& audio_file,
                             protocol::AudioMetadata& metadata) override {
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

#endif // XPU_I_METADATA_PROVIDER_H
