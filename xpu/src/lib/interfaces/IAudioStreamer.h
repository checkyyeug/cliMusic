#ifndef XPU_I_AUDIO_STREAMER_H
#define XPU_I_AUDIO_STREAMER_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <memory>

namespace xpu {

/**
 * @brief Opaque stream handle
 */
using StreamHandle = void*;

/**
 * @brief Stream status structure
 */
struct StreamStatus {
    bool is_active;
    int current_connections;
    int port;
    std::string protocol;      // "http", "icecast", "rtmp", etc.
    uint64_t bytes_sent;
    double uptime_seconds;

    StreamStatus()
        : is_active(false)
        , current_connections(0)
        , port(0)
        , bytes_sent(0)
        , uptime_seconds(0.0) {}
};

/**
 * @brief Audio stream data
 */
struct AudioStream {
    const uint8_t* data;
    size_t size;
    int sample_rate;
    int channels;
    int bit_depth;

    AudioStream()
        : data(nullptr)
        , size(0)
        , sample_rate(44100)
        , channels(2)
        , bit_depth(16) {}
};

/**
 * @brief Audio streamer interface (Phase 4)
 *
 * Provides network streaming capabilities
 */
class IAudioStreamer {
public:
    virtual ~IAudioStreamer() = default;

    /**
     * @brief Create stream server
     */
    virtual ErrorCode createStreamServer(int port,
                                         StreamHandle& handle) = 0;

    /**
     * @brief Start streaming
     */
    virtual ErrorCode startStream(const StreamHandle& handle) = 0;

    /**
     * @brief Stop streaming
     */
    virtual ErrorCode stopStream(const StreamHandle& handle) = 0;

    /**
     * @brief Broadcast to multicast address
     */
    virtual ErrorCode broadcastMulticast(const StreamHandle& handle,
                                         const std::string& multicast_address) = 0;

    /**
     * @brief Get stream status
     */
    virtual ErrorCode getStreamStatus(const StreamHandle& handle,
                                      StreamStatus& status) = 0;

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
class AudioStreamerStub : public IAudioStreamer {
public:
    ErrorCode createStreamServer(int port,
                                 StreamHandle& handle) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode startStream(const StreamHandle& handle) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode stopStream(const StreamHandle& handle) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode broadcastMulticast(const StreamHandle& handle,
                                 const std::string& multicast_address) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode getStreamStatus(const StreamHandle& handle,
                              StreamStatus& status) override {
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

#endif // XPU_I_AUDIO_STREAMER_H
