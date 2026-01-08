#ifndef XPU_I_DISTRIBUTED_CACHE_H
#define XPU_I_DISTRIBUTED_CACHE_H

#include "protocol/ErrorCode.h"
#include "interfaces/FeatureStatus.h"
#include <string>
#include <vector>

namespace xpu {

/**
 * @brief Cache node information
 */
struct CacheNode {
    std::string node_id;
    std::string address;
    int port;
    bool is_online;
    uint64_t total_cache_size;
    uint64_t used_cache_size;

    CacheNode()
        : port(0)
        , is_online(false)
        , total_cache_size(0)
        , used_cache_size(0) {}
};

/**
 * @brief Distributed cache interface (Phase 4)
 *
 * Provides distributed FFT cache synchronization
 */
class IDistributedCache {
public:
    virtual ~IDistributedCache() = default;

    /**
     * @brief Synchronize cache to multiple nodes
     */
    virtual ErrorCode syncCache(const std::string& cache_id,
                                const std::vector<std::string>& nodes) = 0;

    /**
     * @brief Replicate cache to target node
     */
    virtual ErrorCode replicateCache(const std::string& cache_id,
                                     const std::string& target_node) = 0;

    /**
     * @brief Get list of cache nodes
     */
    virtual ErrorCode getCacheNodes(std::vector<CacheNode>& nodes) = 0;

    /**
     * @brief Discover cache nodes on network
     */
    virtual ErrorCode discoverNodes(std::vector<CacheNode>& nodes) = 0;

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
class DistributedCacheStub : public IDistributedCache {
public:
    ErrorCode syncCache(const std::string& cache_id,
                        const std::vector<std::string>& nodes) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode replicateCache(const std::string& cache_id,
                             const std::string& target_node) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode getCacheNodes(std::vector<CacheNode>& nodes) override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode discoverNodes(std::vector<CacheNode>& nodes) override {
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

#endif // XPU_I_DISTRIBUTED_CACHE_H
