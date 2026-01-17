/**
 * @file OrchestrationManager.h
 * @brief Pipeline orchestration (xpuLoad → xpuIn2Wav → xpuPlay)
 */

#ifndef XPU_DAEMON_ORCHESTRATION_MANAGER_H
#define XPU_DAEMON_ORCHESTRATION_MANAGER_H

#include "protocol/ErrorCode.h"
#include "ProcessManager.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace xpu {
namespace daemon {

/**
 * @brief Pipeline stage
 */
enum class PipelineStage {
    Load,       // xpuLoad
    Convert,    // xpuIn2Wav
    Play,       // xpuPlay
    Process     // xpuProcess (optional)
};

/**
 * @brief Pipeline configuration
 */
struct PipelineConfig {
    bool use_fft_cache;
    bool use_dsp;
    std::string dsp_preset;
    int volume;

    PipelineConfig()
        : use_fft_cache(true)
        , use_dsp(false)
        , dsp_preset("flat")
        , volume(100) {}
};

/**
 * @brief Pipeline state
 */
enum class PipelineState {
    Idle,
    Running,
    Paused,
    Error
};

/**
 * @brief Orchestration manager
 */
class OrchestrationManager {
public:
    OrchestrationManager();
    ~OrchestrationManager();

    /**
     * @brief Initialize orchestration manager
     */
    ErrorCode initialize(std::shared_ptr<ProcessManager> proc_mgr);

    /**
     * @brief Start pipeline for a file
     */
    ErrorCode startPipeline(const std::string& file_path,
                           const PipelineConfig& config);

    /**
     * @brief Stop pipeline
     */
    ErrorCode stopPipeline();

    /**
     * @brief Pause pipeline
     */
    ErrorCode pausePipeline();

    /**
     * @brief Resume pipeline
     */
    ErrorCode resumePipeline();

    /**
     * @brief Get pipeline state
     */
    PipelineState getState() const;

    /**
     * @brief Monitor pipeline health
     */
    ErrorCode monitorPipeline();

    /**
     * @brief Get current file
     */
    std::string getCurrentFile() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Create pipeline stages
     */
    ErrorCode createPipelineStages(const std::string& file_path,
                                  const PipelineConfig& config);

    /**
     * @brief Setup pipes between stages
     */
    ErrorCode setupPipes();

    /**
     * @brief Cleanup pipeline
     */
    ErrorCode cleanupPipeline();

    /**
     * @brief Handle pipeline error
     */
    ErrorCode handlePipelineError(ErrorCode error);
};

} // namespace daemon
} // namespace xpu

#endif // XPU_DAEMON_ORCHESTRATION_MANAGER_H
