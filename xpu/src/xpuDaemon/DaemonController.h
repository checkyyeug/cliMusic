/**
 * @file DaemonController.h
 * @brief Daemon lifecycle management
 */

#ifndef XPU_DAEMON_DAEMON_CONTROLLER_H
#define XPU_DAEMON_DAEMON_CONTROLLER_H

#include "protocol/ErrorCode.h"
#include <string>
#include <memory>
#include <functional>

namespace xpu {
namespace daemon {

/**
 * @brief Daemon state
 */
enum class DaemonState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error
};

/**
 * @brief Daemon controller
 */
class DaemonController {
public:
    DaemonController();
    ~DaemonController();

    /**
     * @brief Initialize daemon
     */
    ErrorCode initialize(const std::string& pid_file);

    /**
     * @brief Start daemon (detach from terminal)
     */
    ErrorCode start();

    /**
     * @brief Stop daemon gracefully
     */
    ErrorCode stop();

    /**
     * @brief Get daemon state
     */
    DaemonState getState() const;

    /**
     * @brief Check if daemon is running
     */
    bool isRunning() const;

    /**
     * @brief Get PID file path
     */
    std::string getPIDFilePath() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Daemonize process
     */
    ErrorCode daemonize();

    /**
     * @brief Write PID file
     */
    ErrorCode writePIDFile();

    /**
     * @brief Remove PID file
     */
    ErrorCode removePIDFile();

    /**
     * @brief Check if another instance is running
     */
    bool isInstanceRunning();
};

} // namespace daemon
} // namespace xpu

#endif // XPU_DAEMON_DAEMON_CONTROLLER_H
