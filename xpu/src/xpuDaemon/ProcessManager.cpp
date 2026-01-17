/**
 * @file ProcessManager.cpp
 * @brief Process manager implementation
 */

#include "ProcessManager.h"
#include "utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <signal.h>
    #include <spawn.h>
#endif

#include <cstring>

using namespace xpu;

namespace xpu {
namespace daemon {

/**
 * @brief Implementation class
 */
class ProcessManager::Impl {
public:
    std::map<process_id_t, ProcessInfo> processes;
    int next_pid;
};

ProcessManager::ProcessManager()
    : impl_(std::make_unique<Impl>()) {
    impl_->next_pid = 1000;
}

ProcessManager::~ProcessManager() {
    // Terminate all running processes
    for (auto& pair : impl_->processes) {
        if (pair.second.is_running) {
            terminateProcess(pair.first);
        }
    }
}

ErrorCode ProcessManager::spawnProcess(ProcessType type,
                                       const std::string& input_file,
                                       const std::vector<std::string>& args) {
    (void)input_file;  // Suppress unused parameter warning
    
    ProcessInfo info;
    info.type = type;
    info.executable = getExecutablePath(type);
    info.args = args;

#ifdef PLATFORM_WINDOWS
    // Windows: Use CreateProcess
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Build command line
    std::string cmdLine = info.executable;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }

    // Create process
    BOOL success = CreateProcessA(
        NULL,                   // Application name
        const_cast<char*>(cmdLine.c_str()),  // Command line
        NULL,                   // Process attributes
        NULL,                   // Thread attributes
        FALSE,                  // Inherit handles
        0,                      // Creation flags
        NULL,                   // Environment
        NULL,                   // Working directory
        &si,                    // Startup info
        &pi                     // Process information
    );

    if (!success) {
        LOG_ERROR("Failed to create process: {}", info.executable);
        return ErrorCode::InvalidOperation;
    }

    process_id_t pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    // Prepare spawn attributes
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);

    // Prepare spawn attributes
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    // Build argument list
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(info.executable.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    // Spawn process
    process_id_t pid;
    int ret = posix_spawnp(&pid, info.executable.c_str(),
                          &file_actions, &attr, argv.data(), nullptr);

    posix_spawn_file_actions_destroy(&file_actions);
    posix_spawnattr_destroy(&attr);

    if (ret != 0) {
        LOG_ERROR("Failed to spawn process: {}", strerror(ret));
        return ErrorCode::InvalidOperation;
    }
#endif

    info.pid = pid;
    info.is_running = true;
    impl_->processes[pid] = info;

    LOG_INFO("Spawned process: {} (PID: {})", info.executable, pid);
    return ErrorCode::Success;
}

ErrorCode ProcessManager::terminateProcess(process_id_t pid) {
    auto it = impl_->processes.find(pid);
    if (it == impl_->processes.end()) {
        LOG_ERROR("Process not found: {}", pid);
        return ErrorCode::InvalidOperation;
    }

    if (!it->second.is_running) {
        LOG_WARNING("Process already stopped: {}", pid);
        return ErrorCode::Success;
    }

#ifdef PLATFORM_WINDOWS
    // Windows: Use TerminateProcess
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) {
        LOG_ERROR("Failed to open process {}", pid);
        return ErrorCode::InvalidOperation;
    }

    if (!TerminateProcess(hProcess, 0)) {
        LOG_ERROR("Failed to terminate process {}", pid);
        CloseHandle(hProcess);
        return ErrorCode::InvalidOperation;
    }

    WaitForSingleObject(hProcess, 5000);  // Wait up to 5 seconds
    CloseHandle(hProcess);
    it->second.exit_code = 0;
#else
    // Unix: Send SIGTERM
    if (kill(pid, SIGTERM) != 0) {
        LOG_ERROR("Failed to send SIGTERM to process {}", pid);
        return ErrorCode::InvalidOperation;
    }

    // Wait for process to terminate
    int status;
    int ret = waitpid(pid, &status, 0);
    if (ret > 0) {
        if (WIFEXITED(status)) {
            it->second.exit_code = WEXITSTATUS(status);
        }
    }
#endif

    it->second.is_running = false;

    LOG_INFO("Terminated process: {} (PID: {})", it->second.executable, pid);
    return ErrorCode::Success;
}

ErrorCode ProcessManager::getProcessInfo(process_id_t pid, ProcessInfo& info) const {
    auto it = impl_->processes.find(pid);
    if (it == impl_->processes.end()) {
        return ErrorCode::InvalidOperation;
    }

    info = it->second;
    return ErrorCode::Success;
}

std::vector<ProcessInfo> ProcessManager::getAllProcesses() const {
    std::vector<ProcessInfo> processes;
    for (const auto& pair : impl_->processes) {
        processes.push_back(pair.second);
    }
    return processes;
}

ErrorCode ProcessManager::monitorProcesses() {
    for (auto& pair : impl_->processes) {
        if (pair.second.is_running) {
            // Check if process is still alive
            if (!isProcessAlive(pair.first)) {
                // Process has terminated
                pair.second.is_running = false;

#ifdef PLATFORM_WINDOWS
                // Windows: Get exit code via GetExitCodeProcess
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pair.first);
                if (hProcess) {
                    DWORD exitCode;
                    if (GetExitCodeProcess(hProcess, &exitCode)) {
                        pair.second.exit_code = exitCode;
                    }
                    CloseHandle(hProcess);
                }
#else
                // Unix: Get exit code via waitpid
                int status;
                if (waitpid(pair.first, &status, WNOHANG) > 0) {
                    if (WIFEXITED(status)) {
                        pair.second.exit_code = WEXITSTATUS(status);
                    }
                }
#endif

                LOG_INFO("Process {} (PID: {}) terminated with exit code {}",
                        pair.second.executable, pair.first, pair.second.exit_code);
            }
        }
    }

    return ErrorCode::Success;
}

int ProcessManager::getProcessCount(ProcessType type) const {
    int count = 0;
    for (const auto& pair : impl_->processes) {
        if (pair.second.type == type && pair.second.is_running) {
            count++;
        }
    }
    return count;
}

bool ProcessManager::hasRunningProcesses() const {
    for (const auto& pair : impl_->processes) {
        if (pair.second.is_running) {
            return true;
        }
    }
    return false;
}

std::string ProcessManager::getExecutablePath(ProcessType type) const {
    switch (type) {
        case ProcessType::xpuLoad: return "xpuLoad";
        case ProcessType::xpuIn2Wav: return "xpuIn2Wav";
        case ProcessType::xpuPlay: return "xpuPlay";
        case ProcessType::xpuQueue: return "xpuQueue";
        case ProcessType::xpuProcess: return "xpuProcess";
        default: return "";
    }
}

bool ProcessManager::isProcessAlive(process_id_t pid) const {
#ifndef PLATFORM_WINDOWS
    return (kill(pid, 0) == 0);
#else
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess) {
        DWORD exit_code;
        if (GetExitCodeProcess(hProcess, &exit_code)) {
            CloseHandle(hProcess);
            return (exit_code == STILL_ACTIVE);
        }
        CloseHandle(hProcess);
    }
    return false;
#endif
}

} // namespace daemon
} // namespace xpu
