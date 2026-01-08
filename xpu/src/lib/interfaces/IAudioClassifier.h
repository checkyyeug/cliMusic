#ifndef XPU_I_AUDIO_CLASSIFIER_H
#define XPU_I_AUDIO_CLASSIFIER_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <vector>

namespace xpu {

/**
 * @brief Audio classification result
 */
struct ClassificationResult {
    std::string genre;
    std::string mood;
    std::string tempo_range;
    float confidence;
    std::vector<std::pair<std::string, float>> probabilities;  // All genre probabilities

    ClassificationResult()
        : confidence(0.0f) {}
};

/**
 * @brief Audio classifier interface (Phase 3)
 *
 * Provides audio classification capabilities (genre, mood, etc.)
 */
class IAudioClassifier {
public:
    virtual ~IAudioClassifier() = default;

    /**
     * @brief Classify audio file
     */
    virtual ErrorCode classify(const std::string& audio_file,
                               ClassificationResult& result) = 0;

    /**
     * @brief Classify from fingerprint cache
     */
    virtual ErrorCode classifyFromFingerprint(const std::string& cache_id,
                                              ClassificationResult& result) = 0;

    /**
     * @brief Batch classify multiple files
     */
    virtual ErrorCode batchClassify(const std::vector<std::string>& files,
                                    std::vector<ClassificationResult>& results) = 0;

    /**
     * @brief Get supported genres
     */
    virtual std::vector<std::string> getSupportedGenres() const = 0;

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
class AudioClassifierStub : public IAudioClassifier {
public:
    ErrorCode classify(const std::string& audio_file,
                      ClassificationResult& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode classifyFromFingerprint(const std::string& cache_id,
                                     ClassificationResult& result) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode batchClassify(const std::vector<std::string>& files,
                           std::vector<ClassificationResult>& results) override {
        return ErrorCode::NotImplemented;
    }

    std::vector<std::string> getSupportedGenres() const override {
        return {};
    }

    bool isAvailable() const override {
        return false;  // Not available in Phase 1
    }

    FeatureStatus getFeatureStatus() const override {
        return FeatureStatus::EXTENDED_V1;  // Phase 3
    }
};

} // namespace xpu

#endif // XPU_I_AUDIO_CLASSIFIER_H
