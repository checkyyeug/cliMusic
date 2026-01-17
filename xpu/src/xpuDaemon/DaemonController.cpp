/**
 * @file DaemonController.cpp
 * @brief Daemon controller implementation
 */

#include "DaemonController.h"
#include "utils/Logger.h"
#include <fstream>
#include <cstring>

#ifdef PLATFORM_LINUX
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#endif

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace xpu;

namespace xpu {
namespace daemon {

/**
 * @brief Implementation class
 */
class DaemonController::Impl {
public:
    DaemonState state;
    std::string pid_file;
#ifdef PLATFORM_WINDOWS
    DWORD pid;
#else
    pid_t pid;
#endif

    Impl()
        : state(DaemonState::Stopped)
        , pid(0) {}
};

DaemonController::DaemonController()
    : impl_(std::make_unique<Impl>()) {}

DaemonController::~DaemonController() {
    if (impl_->state == DaemonState::Running) {
        stop();
    }
}

ErrorCode DaemonController::initialize(const std::string& pid_file) {
    impl_->pid_file = pid_file;

    // Check if another instance is running
    if (isInstanceRunning()) {
        LOG_ERROR("Another instance is already running");
        return ErrorCode::InvalidState;
    }

    LOG_INFO("Daemon controller initialized");
    return ErrorCode::Success;
}

ErrorCode DaemonController::start() {
    if (impl_->state != DaemonState::Stopped) {
        LOG_ERROR("Daemon is not in stopped state");
        return ErrorCode::InvalidState;
    }

    impl_->state = DaemonState::Starting;

    // Daemonize process
    ErrorCode ret = daemonize();
    if (ret != ErrorCode::Success) {
        impl_->state = DaemonState::Error;
        return ret;
    }

    // Write PID file
    ret = writePIDFile();
    if (ret != ErrorCode::Success) {
        impl_->state = DaemonState::Error;
        return ret;
    }

    impl_->state = DaemonState::Running;
    impl_->pid = _getpid();

    LOG_INFO("Daemon started with PID: {}", impl_->pid);
    return ErrorCode::Success;
}

ErrorCode DaemonController::stop() {
    if (impl_->state != DaemonState::Running) {
        LOG_ERROR("Daemon is not running");
        return ErrorCode::InvalidState;
    }

    impl_->state = DaemonState::Stopping;

    // Remove PID file
    removePIDFile();

    impl_->state = DaemonState::Stopped;
    LOG_INFO("Daemon stopped");

    return ErrorCode::Success;
}

DaemonState DaemonController::getState() const {
    return impl_->state;
}

bool DaemonController::isRunning() const {
    return impl_->state == DaemonState::Running;
}

std::string DaemonController::getPIDFilePath() const {
    return impl_->pid_file;
}

ErrorCode DaemonController::daemonize() {
#ifndef PLATFORM_WINDOWS
    // Fork first time
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("First fork failed");
        return ErrorCode::InvalidOperation;
    }

    if (pid > 0) {
        // Parent exits
        exit(0);
    }

    // Create new session
    pid_t sid = setsid();
    if (sid < 0) {
        LOG_ERROR("setsid failed");
        return ErrorCode::InvalidOperation;
    }

    // Fork second time
    pid = fork();
    if (pid < 0) {
        LOG_ERROR("Second fork failed");
        return ErrorCode::InvalidOperation;
    }

    if (pid > 0) {
        // Parent exits
        exit(0);
    }

    // Change working directory to root
    if (chdir("/") < 0) {
        LOG_WARNING("Failed to change working directory");
    }

    // Reset file mode
    umask(0);

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect to /dev/null
    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_RDWR);   // stdout
    open("/dev/null", O_RDWR);   // stderr

    LOG_INFO("Process daemonized");
    return ErrorCode::Success;
#else
    // Windows: run as service (simplified)
    LOG_INFO("Windows daemon mode not fully implemented");
    return ErrorCode::Success;
#endif
}

ErrorCode DaemonController::writePIDFile() {
    std::ofstream pid_file(impl_->pid_file);
    if (!pid_file.is_open()) {
        LOG_ERROR("Failed to open PID file: {}", impl_->pid_file);
        return ErrorCode::FileWriteError;
    }

    pid_file << _getpid() << std::endl;
    pid_file.close();

    LOG_INFO("PID file written: {}", impl_->pid_file);
    return ErrorCode::Success;
}

ErrorCode DaemonController::removePIDFile() {
    if (std::remove(impl_->pid_file.c_str()) != 0) {
        LOG_WARNING("Failed to remove PID file: {}", impl_->pid_file);
        return ErrorCode::FileWriteError;
    }

    LOG_INFO("PID file removed");
    return ErrorCode::Success;
}

bool DaemonController::isInstanceRunning() {
    std::ifstream pid_file(impl_->pid_file);
    if (!pid_file.is_open()) {
        return false;
    }

#ifdef PLATFORM_WINDOWS
    DWORD pid;
    pid_file >> pid;
#else
    pid_t pid;
    pid_file >> pid;
#endif
    pid_file.close();

    // Check if process is running
#ifndef PLATFORM_WINDOWS
    return (kill(pid, 0) == 0);
#else
    // Windows: use OpenProcess
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess) {
        CloseHandle(hProcess);
        return true;
    }
    return false;
#endif
}

} // namespace daemon
} // namespace xpu
