#ifndef XPU_FEATURE_STATUS_H
#define XPU_FEATURE_STATUS_H

namespace xpu {

/**
 * @brief Feature status marker for XPU modules
 *
 * Used to track which phase each feature belongs to
 */
enum class FeatureStatus : int {
    // Phase 1: Core functionality
    CORE_V1 = 1,

    // Phase 2: AI-Native integration
    API_V1 = 2,

    // Phase 3: Extended modules
    EXTENDED_V1 = 3,

    // Phase 4: Network and distributed
    DISTRIBUTED_V1 = 4,

    // Phase 5: Advanced features
    ADVANCED_V2 = 5,

    // Experimental features
    EXPERIMENTAL = 99
};

/**
 * @brief Get feature status as string
 */
inline const char* featureStatusToString(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::CORE_V1: return "CORE_V1";
        case FeatureStatus::API_V1: return "API_V1";
        case FeatureStatus::EXTENDED_V1: return "EXTENDED_V1";
        case FeatureStatus::DISTRIBUTED_V1: return "DISTRIBUTED_V1";
        case FeatureStatus::ADVANCED_V2: return "ADVANCED_V2";
        case FeatureStatus::EXPERIMENTAL: return "EXPERIMENTAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Check if feature is available in current phase
 */
inline bool isFeatureAvailable(FeatureStatus feature, FeatureStatus current_phase) {
    return feature <= current_phase;
}

} // namespace xpu

#endif // XPU_FEATURE_STATUS_H
