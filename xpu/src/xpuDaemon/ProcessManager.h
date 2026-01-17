/**
 * @file ProcessManager.h
 * @brief Child process management
 */

#ifndef XPU_DAEMON_PROCESS_MANAGER_H
#define XPU_DAEMON_PROCESS_MANAGER_H

#include "protocol/ErrorCode.h"
#include "utils/PlatformUtils.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

#ifdef PLATFORM_LINUX
#include <sys/types.h>
typedef process_id_t process_id_t;
#elif defined(PLATFORM_WINDOWS)
#include <windows.h>
typedef DWORD process_id_t;
#elif defined(PLATFORM_MACOS)
#include <sys/types.h>
typedef process_id_t process_id_t;
#endif

namespace xpu {
namespace daemon {

/**
 * @brief Process type
 */
enum class ProcessType {
    xpuLoad,
    xpuIn2Wav,
    xpuPlay,
    xpuQueue,
    xpuProcess
};

/**
 * @brief Process information
 */
struct ProcessInfo {
    process_id_t pid;
    ProcessType type;
    std::string executable;
    std::vector<std::string> args;
    bool is_running;
    int exit_code;

    ProcessInfo()
        : pid(0)
        , type(ProcessType::xpuLoad)
        , is_running(false)
        , exit_code(0) {}
};

/**
 * @brief Process manager
 */
class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    /**
     * @brief Spawn a process
     */
    ErrorCode spawnProcess(ProcessType type,
                          const std::string& input_file,
                          const std::vector<std::string>& args);

    /**
     * @brief Terminate a process
     */
    ErrorCode terminateProcess(process_id_t pid);

    /**
     * @brief Get process info
     */
    ErrorCode getProcessInfo(process_id_t pid, ProcessInfo& info) const;

    /**
     * @brief Get all processes
     */
    std::vector<ProcessInfo> getAllProcesses() const;

    /**
     * @brief Monitor processes (check status)
     */
    ErrorCode monitorProcesses();

    /**
     * @brief Get process count by type
     */
    int getProcessCount(ProcessType type) const;

    /**
     * @brief Check if any process is running
     */
    bool hasRunningProcesses() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Get executable path for process type
     */
    std::string getExecutablePath(ProcessType type) const;

    /**
     * @brief Check if process is alive
     */
    bool isProcessAlive(process_id_t pid) const;
};

} // namespace daemon
} // namespace xpu

#endif // XPU_DAEMON_PROCESS_MANAGER_H
