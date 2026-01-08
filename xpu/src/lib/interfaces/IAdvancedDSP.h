#ifndef XPU_I_ADVANCED_DSP_H
#define XPU_I_ADVANCED_DSP_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <vector>
#include <memory>

namespace xpu {

// Forward declarations
class AudioBuffer;

/**
 * @brief Reverb parameters
 */
struct ReverbParams {
    float room_size;      // 0.0 to 1.0
    float damping;        // 0.0 to 1.0
    float wet_level;      // 0.0 to 1.0
    float dry_level;      // 0.0 to 1.0
    float width;          // 0.0 to 1.0
    float freeze_mode;    // 0.0 or 1.0

    ReverbParams()
        : room_size(0.5f)
        , damping(0.5f)
        , wet_level(0.3f)
        , dry_level(0.7f)
        , width(1.0f)
        , freeze_mode(0.0f) {}
};

/**
 * @brief Chorus parameters
 */
struct ChorusParams {
    float rate;           // Hz (0.1 to 10.0)
    float depth;          // 0.0 to 1.0
    float feedback;       // 0.0 to 1.0
    int delay;            // ms (typically 20-50)

    ChorusParams()
        : rate(1.5f)
        , depth(0.5f)
        , feedback(0.5f)
        , delay(25) {}
};

/**
 * @brief Tube amplifier model
 */
enum class TubeModel : int {
    TwelveAX7 = 0,   // Classic preamp tube
    EL34 = 1,        // Power tube (British sound)
    SixL6 = 2,       // Power tube (American sound)
    EL84 = 3         // Power tube (Class A)
};

/**
 * @brief Tube amplifier parameters
 */
struct TubeParams {
    TubeModel model;
    float drive;          // 0.0 to 1.0
    float bass;           // 0.0 to 1.0
    float mid;            // 0.0 to 1.0
    float treble;         // 0.0 to 1.0
    float presence;       // 0.0 to 1.0
    float master;         // 0.0 to 1.0

    TubeParams()
        : model(TubeModel::TwelveAX7)
        , drive(0.5f)
        , bass(0.5f)
        , mid(0.5f)
        , treble(0.5f)
        , presence(0.5f)
        , master(0.7f) {}
};

/**
 * @brief Phaser parameters
 */
struct PhaserParams {
    float rate;           // Hz (0.1 to 10.0)
    float depth;          // 0.0 to 1.0
    float feedback;       // 0.0 to 1.0
    int stages;           // Number of stages (2-12)

    PhaserParams()
        : rate(0.5f)
        , depth(0.5f)
        , feedback(0.7f)
        , stages(4) {}
};

/**
 * @brief Flanger parameters
 */
struct FlangerParams {
    float rate;           // Hz (0.1 to 10.0)
    float depth;          // 0.0 to 1.0
    float feedback;       // 0.0 to 1.0
    int delay;            // ms (typically 1-10)

    FlangerParams()
        : rate(0.5f)
        , depth(0.5f)
        , feedback(0.7f)
        , delay(5) {}
};

/**
 * @brief Equalizer parameters (advanced)
 */
struct EQParams {
    std::vector<float> bands;  // Band gains in dB
    std::vector<float> frequencies;  // Band frequencies
    std::vector<float> q_factors;    // Q factors for each band

    EQParams() = default;
};

/**
 * @brief Audio buffer structure
 */
class AudioBuffer {
public:
    std::vector<float> data;
    int channels;
    int sample_rate;
    size_t frame_count;

    AudioBuffer()
        : channels(2)
        , sample_rate(44100)
        , frame_count(0) {}
};

/**
 * @brief Advanced DSP interface (Phase 3)
 *
 * Provides advanced audio effects processing
 */
class IAdvancedDSP {
public:
    virtual ~IAdvancedDSP() = default;

    /**
     * @brief Apply reverb effect
     */
    virtual ErrorCode applyReverb(const AudioBuffer& input,
                                  AudioBuffer& output,
                                  const ReverbParams& params) = 0;

    /**
     * @brief Apply chorus effect
     */
    virtual ErrorCode applyChorus(const AudioBuffer& input,
                                  AudioBuffer& output,
                                  const ChorusParams& params) = 0;

    /**
     * @brief Apply tube amplifier simulation
     */
    virtual ErrorCode applyTubeAmp(const AudioBuffer& input,
                                   AudioBuffer& output,
                                   const TubeParams& params) = 0;

    /**
     * @brief Apply phaser effect
     */
    virtual ErrorCode applyPhaser(const AudioBuffer& input,
                                  AudioBuffer& output,
                                  const PhaserParams& params) = 0;

    /**
     * @brief Apply flanger effect
     */
    virtual ErrorCode applyFlanger(const AudioBuffer& input,
                                   AudioBuffer& output,
                                   const FlangerParams& params) = 0;

    /**
     * @brief Apply advanced equalizer
     */
    virtual ErrorCode applyEQ(const AudioBuffer& input,
                              AudioBuffer& output,
                              const EQParams& params) = 0;

    /**
     * @brief Get supported tube models
     */
    virtual std::vector<TubeModel> getSupportedTubeModels() const = 0;

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
class AdvancedDSPStub : public IAdvancedDSP {
public:
    ErrorCode applyReverb(const AudioBuffer& input,
                          AudioBuffer& output,
                          const ReverbParams& params) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode applyChorus(const AudioBuffer& input,
                          AudioBuffer& output,
                          const ChorusParams& params) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode applyTubeAmp(const AudioBuffer& input,
                           AudioBuffer& output,
                           const TubeParams& params) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode applyPhaser(const AudioBuffer& input,
                          AudioBuffer& output,
                          const PhaserParams& params) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode applyFlanger(const AudioBuffer& input,
                           AudioBuffer& output,
                           const FlangerParams& params) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode applyEQ(const AudioBuffer& input,
                      AudioBuffer& output,
                      const EQParams& params) override {
        return ErrorCode::NotImplemented;
    }

    std::vector<TubeModel> getSupportedTubeModels() const override {
        return {
            TubeModel::TwelveAX7,
            TubeModel::EL34,
            TubeModel::SixL6,
            TubeModel::EL84
        };
    }

    bool isAvailable() const override {
        return false;  // Not available in Phase 1
    }

    FeatureStatus getFeatureStatus() const override {
        return FeatureStatus::EXTENDED_V1;  // Phase 3
    }
};

} // namespace xpu

#endif // XPU_I_ADVANCED_DSP_H
