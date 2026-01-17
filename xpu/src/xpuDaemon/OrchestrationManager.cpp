/**
 * @file OrchestrationManager.cpp
 * @brief Orchestration manager implementation
 */

#include "OrchestrationManager.h"
#include "utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <signal.h>
#endif

using namespace xpu;

namespace xpu {
namespace daemon {

/**
 * @brief Pipeline stage info
 */
struct PipelineStageInfo {
#ifdef PLATFORM_WINDOWS
    DWORD pid;
#else
    pid_t pid;
#endif
    int read_fd;
    int write_fd;
    ProcessType type;

    PipelineStageInfo()
        : pid(0)
        , read_fd(-1)
        , write_fd(-1)
        , type(ProcessType::xpuLoad) {}
};

/**
 * @brief Implementation class
 */
class OrchestrationManager::Impl {
public:
    std::shared_ptr<ProcessManager> proc_mgr;
    PipelineState state;
    std::string current_file;
    std::map<PipelineStage, PipelineStageInfo> stages;
    PipelineConfig current_config;

    Impl()
        : state(PipelineState::Idle) {}
};

OrchestrationManager::OrchestrationManager()
    : impl_(std::make_unique<Impl>()) {}

OrchestrationManager::~OrchestrationManager() {
    cleanupPipeline();
}

ErrorCode OrchestrationManager::initialize(std::shared_ptr<ProcessManager> proc_mgr) {
    impl_->proc_mgr = proc_mgr;
    LOG_INFO("Orchestration manager initialized");
    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::startPipeline(const std::string& file_path,
                                             const PipelineConfig& config) {
    if (impl_->state != PipelineState::Idle) {
        LOG_ERROR("Pipeline not idle");
        return ErrorCode::InvalidState;
    }

    impl_->current_file = file_path;
    impl_->current_config = config;
    impl_->state = PipelineState::Running;

    LOG_INFO("Starting pipeline for: {}", file_path);

    // Create pipeline stages
    ErrorCode ret = createPipelineStages(file_path, config);
    if (ret != ErrorCode::Success) {
        impl_->state = PipelineState::Error;
        return handlePipelineError(ret);
    }

    // Setup pipes
    ret = setupPipes();
    if (ret != ErrorCode::Success) {
        impl_->state = PipelineState::Error;
        return handlePipelineError(ret);
    }

    // Start xpuLoad process
    std::vector<std::string> load_args;
    load_args.push_back(file_path);

    ret = impl_->proc_mgr->spawnProcess(ProcessType::xpuLoad, file_path, load_args);
    if (ret != ErrorCode::Success) {
        impl_->state = PipelineState::Error;
        return handlePipelineError(ret);
    }

    // Store process info
    auto processes = impl_->proc_mgr->getAllProcesses();
    if (!processes.empty()) {
        impl_->stages[PipelineStage::Load].pid = processes.back().pid;
    }

    LOG_INFO("Pipeline started successfully");
    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::stopPipeline() {
    if (impl_->state == PipelineState::Idle) {
        return ErrorCode::Success;
    }

    LOG_INFO("Stopping pipeline");

#ifdef PLATFORM_WINDOWS
    // Windows stub implementation
    // TODO: Implement proper Windows process termination
    for (auto& pair : impl_->stages) {
        if (pair.second.pid > 0) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pair.second.pid);
            if (hProcess) {
                TerminateProcess(hProcess, 0);
                WaitForSingleObject(hProcess, 1000);
                CloseHandle(hProcess);
            }
        }
    }
#else
    // Send SIGTERM to all pipeline processes
    for (auto& pair : impl_->stages) {
        if (pair.second.pid > 0) {
            kill(pair.second.pid, SIGTERM);
        }
    }

    // Wait for processes to terminate
    for (auto& pair : impl_->stages) {
        if (pair.second.pid > 0) {
            int status;
            waitpid(pair.second.pid, &status, 0);
        }
    }
#endif

    cleanupPipeline();
    impl_->state = PipelineState::Idle;

    LOG_INFO("Pipeline stopped");
    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::pausePipeline() {
    if (impl_->state != PipelineState::Running) {
        return ErrorCode::InvalidState;
    }

#ifdef PLATFORM_WINDOWS
    // Windows: Not directly supported, use SuspendThread
    // TODO: Implement proper Windows pause functionality
    LOG_WARNING("Pipeline pause not fully implemented on Windows");
#else
    // Send SIGSTOP to playback process
    auto it = impl_->stages.find(PipelineStage::Play);
    if (it != impl_->stages.end() && it->second.pid > 0) {
        kill(it->second.pid, SIGSTOP);
    }
#endif

    impl_->state = PipelineState::Paused;
    LOG_INFO("Pipeline paused");
    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::resumePipeline() {
    if (impl_->state != PipelineState::Paused) {
        return ErrorCode::InvalidState;
    }

#ifdef PLATFORM_WINDOWS
    // Windows: Not directly supported, use ResumeThread
    // TODO: Implement proper Windows resume functionality
    LOG_WARNING("Pipeline resume not fully implemented on Windows");
#else
    // Send SIGCONT to playback process
    auto it = impl_->stages.find(PipelineStage::Play);
    if (it != impl_->stages.end() && it->second.pid > 0) {
        kill(it->second.pid, SIGCONT);
    }
#endif

    impl_->state = PipelineState::Running;
    LOG_INFO("Pipeline resumed");
    return ErrorCode::Success;
}

PipelineState OrchestrationManager::getState() const {
    return impl_->state;
}

ErrorCode OrchestrationManager::monitorPipeline() {
    if (impl_->state == PipelineState::Idle) {
        return ErrorCode::Success;
    }

    // Monitor all pipeline processes
    for (auto& pair : impl_->stages) {
        if (pair.second.pid > 0) {
            // Check if process is still running
            ProcessInfo info;
            ErrorCode ret = impl_->proc_mgr->getProcessInfo(pair.second.pid, info);
            if (ret == ErrorCode::Success && !info.is_running) {
                // Process terminated unexpectedly
                if (info.exit_code != 0) {
                    LOG_ERROR("Pipeline stage {} terminated with error code {}",
                             static_cast<int>(pair.first), info.exit_code);
                    impl_->state = PipelineState::Error;
                    return handlePipelineError(ErrorCode::AudioBackendError);
                }
            }
        }
    }

    return ErrorCode::Success;
}

std::string OrchestrationManager::getCurrentFile() const {
    return impl_->current_file;
}

ErrorCode OrchestrationManager::createPipelineStages(const std::string& file_path,
                                                     const PipelineConfig& config) {
    (void)file_path;  // Suppress unused parameter warning
    
    // Initialize stage info
    impl_->stages[PipelineStage::Load] = PipelineStageInfo();
    impl_->stages[PipelineStage::Load].type = ProcessType::xpuLoad;

    impl_->stages[PipelineStage::Convert] = PipelineStageInfo();
    impl_->stages[PipelineStage::Convert].type = ProcessType::xpuIn2Wav;

    impl_->stages[PipelineStage::Play] = PipelineStageInfo();
    impl_->stages[PipelineStage::Play].type = ProcessType::xpuPlay;

    if (config.use_dsp) {
        impl_->stages[PipelineStage::Process] = PipelineStageInfo();
        impl_->stages[PipelineStage::Process].type = ProcessType::xpuProcess;
    }

    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::setupPipes() {
    // Create pipes between stages
    // xpuLoad | xpuIn2Wav | xpuPlay

#ifdef PLATFORM_WINDOWS
    // Windows: Use anonymous pipes or named pipes
    // TODO: Implement proper Windows pipe creation
    HANDLE read1, write1, read2, write2;

    // Create pipe 1 (Load -> Convert)
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&read1, &write1, &sa, 0)) {
        LOG_ERROR("Failed to create pipe 1");
        return ErrorCode::InvalidOperation;
    }

    // Create pipe 2 (Convert -> Play)
    if (!CreatePipe(&read2, &write2, &sa, 0)) {
        LOG_ERROR("Failed to create pipe 2");
        CloseHandle(read1);
        CloseHandle(write1);
        return ErrorCode::InvalidOperation;
    }

    // Store pipe handles as fds (cast to int for now, will need proper handling)
    impl_->stages[PipelineStage::Load].write_fd = reinterpret_cast<intptr_t>(write1);
    impl_->stages[PipelineStage::Convert].read_fd = reinterpret_cast<intptr_t>(read1);
    impl_->stages[PipelineStage::Convert].write_fd = reinterpret_cast<intptr_t>(write2);
    impl_->stages[PipelineStage::Play].read_fd = reinterpret_cast<intptr_t>(read2);
#else
    int pipe1[2], pipe2[2];

    if (pipe(pipe1) < 0) {
        LOG_ERROR("Failed to create pipe 1");
        return ErrorCode::InvalidOperation;
    }

    if (pipe(pipe2) < 0) {
        LOG_ERROR("Failed to create pipe 2");
        close(pipe1[0]);
        close(pipe1[1]);
        return ErrorCode::InvalidOperation;
    }

    // Store pipe fds
    impl_->stages[PipelineStage::Load].write_fd = pipe1[1];
    impl_->stages[PipelineStage::Convert].read_fd = pipe1[0];
    impl_->stages[PipelineStage::Convert].write_fd = pipe2[1];
    impl_->stages[PipelineStage::Play].read_fd = pipe2[0];
#endif

    LOG_INFO("Pipes setup complete");
    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::cleanupPipeline() {
    // Close all pipe fds
    for (auto& pair : impl_->stages) {
#ifdef PLATFORM_WINDOWS
        if (pair.second.read_fd >= 0) {
            HANDLE h = reinterpret_cast<HANDLE>(pair.second.read_fd);
            CloseHandle(h);
            pair.second.read_fd = -1;
        }
        if (pair.second.write_fd >= 0) {
            HANDLE h = reinterpret_cast<HANDLE>(pair.second.write_fd);
            CloseHandle(h);
            pair.second.write_fd = -1;
        }
#else
        if (pair.second.read_fd >= 0) {
            close(pair.second.read_fd);
            pair.second.read_fd = -1;
        }
        if (pair.second.write_fd >= 0) {
            close(pair.second.write_fd);
            pair.second.write_fd = -1;
        }
#endif
    }

    impl_->stages.clear();
    impl_->current_file.clear();

    return ErrorCode::Success;
}

ErrorCode OrchestrationManager::handlePipelineError(ErrorCode error) {
    LOG_ERROR("Pipeline error: {}", static_cast<int>(error));

    // Cleanup pipeline
    cleanupPipeline();

    return error;
}

} // namespace daemon
} // namespace xpu
