#ifndef XPU_I_NETWORK_AUDIO_H
#define XPU_I_NETWORK_AUDIO_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include "interfaces/IAudioStreamer.h"
#include <string>
#include <vector>

namespace xpu {

/**
 * @brief DLNA configuration
 */
struct DLNAConfig {
    std::string device_name;
    bool enabled;
    int port;
    std::string uuid;

    DLNAConfig()
        : enabled(false)
        , port(0) {}
};

/**
 * @brief AirPlay configuration
 */
struct AirPlayConfig {
    std::string device_name;
    bool enabled;
    int port;
    std::string password;

    AirPlayConfig()
        : enabled(false)
        , port(0) {}
};

/**
 * @brief Network device information
 */
struct NetworkDevice {
    std::string device_id;
    std::string name;
    std::string type;        // "dlna", "airplay", "chromecast"
    std::string address;
    int port;
    bool is_available;

    NetworkDevice()
        : port(0)
        , is_available(false) {}
};

/**
 * @brief Network audio interface (Phase 4)
 *
 * Provides network audio playback (DLNA, AirPlay)
 */
class INetworkAudio {
public:
    virtual ~INetworkAudio() = default;

    /**
     * @brief Start DLNA server
     */
    virtual ErrorCode startDLNAServer(const DLNAConfig& config) = 0;

    /**
     * @brief Start AirPlay server
     */
    virtual ErrorCode startAirPlayServer(const AirPlayConfig& config) = 0;

    /**
     * @brief Discover network devices
     */
    virtual ErrorCode discoverDevices(std::vector<NetworkDevice>& devices) = 0;

    /**
     * @brief Push audio to network device
     */
    virtual ErrorCode pushToDevice(const NetworkDevice& device,
                                    const AudioStream& stream) = 0;

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
class NetworkAudioStub : public INetworkAudio {
public:
    ErrorCode startDLNAServer(const DLNAConfig& config) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode startAirPlayServer(const AirPlayConfig& config) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode discoverDevices(std::vector<NetworkDevice>& devices) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode pushToDevice(const NetworkDevice& device,
                           const AudioStream& stream) override {
        return ErrorCode::NotImplemented;
    }

    bool isAvailable() const override {
        return false;  // Not available in Phase 1
    }

    FeatureStatus getFeatureStatus() const override {
        return FeatureStatus::DISTRIBUTED_V1;  // Phase 4
    }
};

} // namespace xpu

#endif // XPU_I_NETWORK_AUDIO_H
