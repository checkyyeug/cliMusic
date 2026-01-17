/**
 * @file xpuDaemon.cpp
 * @brief Background daemon with orchestration - XPU Module 6
 *
 * Manages process lifecycle and orchestrates pipeline
 * Supports: Linux (systemd), macOS (launchd), Windows (service)
 * MCP Mode: Provides MCP (Model Context Protocol) server for AI integration
 */

#include "DaemonController.h"
#include "OrchestrationManager.h"
#include "MCPServer.h"
#include "protocol/ErrorCode.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <atomic>
#include <memory>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
typedef pid_t process_id_t;
#endif

#ifdef PLATFORM_WINDOWS
#include <windows.h>
typedef DWORD process_id_t;
#endif

#ifdef PLATFORM_MACOS
#include <unistd.h>
#include <sys/types.h>
typedef pid_t process_id_t;
#endif

using namespace xpu;
using namespace xpu::daemon;

// Global state
static std::atomic<bool> g_running(false);
static std::atomic<bool> g_reload_config(false);

/**
 * @brief Signal handlers
 */
#ifdef PLATFORM_WINDOWS
void signalHandler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        LOG_INFO("Received shutdown signal");
        g_running = false;
    }
}
#else
void signalHandler(int signal) {
    switch (signal) {
        case SIGTERM:
        case SIGINT:
            LOG_INFO("Received shutdown signal");
            g_running = false;
            break;
        case SIGHUP:
            LOG_INFO("Received reload signal");
            g_reload_config = true;
            break;
    }
}
#endif

/**
 * @brief Get PID file path
 */
std::string getPIDFilePath() {
    return utils::PlatformUtils::getConfigDirectory() + "/xpuDaemon.pid";
}

/**
 * @brief Check if daemon is running
 */
bool isDaemonRunning() {
    std::string pid_file = getPIDFilePath();
    std::ifstream pid_stream(pid_file);

    if (!pid_stream.is_open()) {
        return false;
    }

    process_id_t pid;
    pid_stream >> pid;

#ifdef PLATFORM_WINDOWS
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process) {
        CloseHandle(process);
        return true;
    }
    return false;
#else
    // Check if process exists by sending signal 0
    return (kill(pid, 0) == 0);
#endif
}

/**
 * @brief Stop running daemon
 */
ErrorCode stopDaemon() {
    std::string pid_file = getPIDFilePath();
    std::ifstream pid_stream(pid_file);

    if (!pid_stream.is_open()) {
        std::cerr << "Error: Daemon is not running (no PID file)\n";
        return ErrorCode::InvalidState;
    }

    process_id_t pid;
    pid_stream >> pid;

    std::cout << "Stopping daemon (PID: " << pid << ")...\n";

#ifdef PLATFORM_WINDOWS
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process) {
        TerminateProcess(process, 0);
        CloseHandle(process);
        std::remove(pid_file.c_str());
        std::cout << "Daemon stopped\n";
        return ErrorCode::Success;
    }
    return ErrorCode::InvalidOperation;
#else
    if (kill(pid, SIGTERM) == 0) {
        // Wait for process to terminate
        int count = 0;
        while (kill(pid, 0) == 0 && count < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            count++;
        }

        if (kill(pid, 0) == 0) {
            // Force kill if still running
            kill(pid, SIGKILL);
        }

        std::remove(pid_file.c_str());
        std::cout << "Daemon stopped\n";
        return ErrorCode::Success;
    }
    return ErrorCode::InvalidOperation;
#endif
}

/**
 * @brief Reload daemon configuration
 */
ErrorCode reloadDaemonConfig() {
    std::string pid_file = getPIDFilePath();
    std::ifstream pid_stream(pid_file);

    if (!pid_stream.is_open()) {
        std::cerr << "Error: Daemon is not running (no PID file)\n";
        return ErrorCode::InvalidState;
    }

    process_id_t pid;
    pid_stream >> pid;

    std::cout << "Reloading daemon configuration (PID: " << pid << ")...\n";

#ifndef PLATFORM_WINDOWS
    if (kill(pid, SIGHUP) == 0) {
        std::cout << "Configuration reload signal sent\n";
        return ErrorCode::Success;
    }
    std::cerr << "Error: Failed to send reload signal\n";
    return ErrorCode::InvalidOperation;
#else
    std::cerr << "Error: Config reload not supported on Windows\n";
    return ErrorCode::NotImplemented;
#endif
}

/**
 * @brief Show daemon status
 */
ErrorCode showDaemonStatus() {
    std::string pid_file = getPIDFilePath();
    std::ifstream pid_stream(pid_file);

    std::cout << "{\n";
    std::cout << "  \"running\": " << (pid_stream.is_open() ? "true" : "false") << ",\n";

    if (pid_stream.is_open()) {
        process_id_t pid;
        pid_stream >> pid;
        std::cout << "  \"pid\": " << pid << ",\n";

#ifdef PLATFORM_WINDOWS
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (process) {
            std::cout << "  \"status\": \"running\"\n";
            CloseHandle(process);
        } else {
            std::cout << "  \"status\": \"zombie\"\n";
        }
#else
        if (kill(pid, 0) == 0) {
            std::cout << "  \"status\": \"running\"\n";
        } else {
            std::cout << "  \"status\": \"zombie\"\n";
        }
#endif
    } else {
        std::cout << "  \"status\": \"stopped\"\n";
    }

    std::cout << "}\n";
    return ErrorCode::Success;
}

/**
 * @brief Restart daemon
 */
ErrorCode restartDaemon() {
    std::cout << "Restarting daemon...\n";

    // Stop if running
    if (isDaemonRunning()) {
        ErrorCode err = stopDaemon();
        if (err != ErrorCode::Success) {
            return err;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Start daemon
    std::string config_dir = utils::PlatformUtils::getConfigDirectory();
    std::string log_file = utils::PlatformUtils::getLogFilePath();

    // In production, this would execute the daemon binary
    std::cout << "Daemon restart initiated\n";
    std::cout << "Run: xpuDaemon --daemon --config " << config_dir << "/xpuSetting.conf\n";

    return ErrorCode::Success;
}

/**
 * @brief Daemonize process (Unix/Linux)
 */
ErrorCode daemonize() {
#ifdef PLATFORM_WINDOWS
    return ErrorCode::NotImplemented;  // Windows uses services
#else
    process_id_t pid = fork();
    if (pid < 0) {
        return ErrorCode::InvalidOperation;
    }
    if (pid > 0) {
        // Parent exits
        exit(0);
    }

    // Child continues
    setsid();

    // Fork again to prevent acquiring terminal
    pid = fork();
    if (pid < 0) {
        return ErrorCode::InvalidOperation;
    }
    if (pid > 0) {
        exit(0);
    }

    // Set file mode mask
    umask(0);

    // Redirect standard files to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    LOG_INFO("Daemonized successfully");
    return ErrorCode::Success;
#endif
}

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -d, --daemon            Run as daemon (background)\n";
    std::cout << "  -f, --foreground        Run in foreground\n";
    std::cout << "  -c, --config <path>     Configuration file path\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "  --mcp, --mcp-mode       Run as MCP server (stdio mode)\n";
    std::cout << "  --status               Show daemon status\n";
    std::cout << "  --stop                 Stop running daemon\n";
    std::cout << "  --restart              Restart daemon\n";
    std::cout << "  --reload               Reload configuration\n";
    std::cout << "\nProcess management:\n";
    std::cout << "  Orchestrate: xpuLoad -> xpuIn2Wav -> xpuPlay\n";
    std::cout << "  Queue: xpuQueue\n";
    std::cout << "  DSP: xpuProcess\n";
    std::cout << "\nMCP Mode:\n";
    std::cout << "  When run with --mcp, provides MCP server for AI integration\n";
    std::cout << "  Communicates via stdio using JSON-RPC 2.0\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuDaemon version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
}

/**
 * @brief Main daemon loop
 */
ErrorCode runDaemon(bool foreground = false) {
    LOG_INFO("Starting daemon");

    if (!foreground) {
        ErrorCode err = daemonize();
        if (err != ErrorCode::Success) {
            LOG_ERROR("Failed to daemonize");
            return err;
        }
    }

    // Setup signal handlers
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
#ifndef PLATFORM_WINDOWS
    signal(SIGHUP, signalHandler);
#endif

    g_running = true;

    // Create PID file
    std::string pid_file_path = getPIDFilePath();
    std::ofstream pid_file_stream(pid_file_path);
    if (pid_file_stream.is_open()) {
#ifdef PLATFORM_WINDOWS
        pid_file_stream << GetCurrentProcessId();
#else
        pid_file_stream << getpid();
#endif
        pid_file_stream.close();
    } else {
        LOG_ERROR("Failed to create PID file: {}", pid_file_path);
        return ErrorCode::FileWriteError;
    }

    // Initialize orchestrator
    auto orchestrator = std::make_unique<OrchestrationManager>();
    auto proc_mgr = std::make_shared<ProcessManager>();
    ErrorCode err = orchestrator->initialize(proc_mgr);
    if (err != ErrorCode::Success) {
        LOG_ERROR("Failed to initialize orchestrator");
        return err;
    }

    // Start pipeline with a default file path (if any)
    // For now, just initialize without starting a specific pipeline
    // err = orchestrator->startPipeline("/path/to/music.flac");
    // if (err != ErrorCode::Success) {
    //     LOG_ERROR("Failed to start pipeline");
    //     return err;
    // }

    LOG_INFO("Daemon running");

    // Main daemon loop
    while (g_running) {
        // Monitor pipeline health
        err = orchestrator->monitorPipeline();
        if (err != ErrorCode::Success) {
            LOG_ERROR("Pipeline monitoring failed: {}", static_cast<int>(err));
            // Continue running despite errors
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (g_reload_config) {
            LOG_INFO("Reloading configuration");
            // TODO: Implement configuration reload
            // err = orchestrator->reloadConfiguration();
            // if (err != ErrorCode::Success) {
            //     LOG_ERROR("Failed to reload configuration: {}", static_cast<int>(err));
            // } else {
            //     LOG_INFO("Configuration reloaded successfully");
            // }
            g_reload_config = false;
        }
    }

    // Stop pipeline
    orchestrator->stopPipeline();

    // Cleanup
    std::remove(pid_file_path.c_str());
    LOG_INFO("Daemon stopped");

    return ErrorCode::Success;
}

/**
 * @brief Run MCP server mode
 */
int runMCPServer() {
    LOG_INFO("Starting MCP Server mode");

    mcp::MCPServer mcp_server;

    // Set API base URL (can be configured via env var)
    const char* api_url = std::getenv("XPU_API_URL");
    if (api_url) {
        mcp_server.setApiBaseUrl(api_url);
    }

    // Start MCP server (blocking, reads from stdin)
    if (!mcp_server.start()) {
        LOG_ERROR("Failed to start MCP Server");
        return 1;
    }

    return 0;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Initialize logger
    utils::Logger::initialize(utils::PlatformUtils::getLogFilePath(), true);

    LOG_INFO("xpuDaemon starting");

    bool daemon_mode = false;
    bool foreground = false;
    bool mcp_mode = false;
    const char* config_path = nullptr;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = true;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--foreground") == 0) {
            foreground = true;
        } else if (strcmp(argv[i], "--mcp") == 0 || strcmp(argv[i], "--mcp-mode") == 0) {
            mcp_mode = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_path = argv[++i];
            }
        } else if (strcmp(argv[i], "--status") == 0) {
            return static_cast<int>(showDaemonStatus());
        } else if (strcmp(argv[i], "--stop") == 0) {
            return static_cast<int>(stopDaemon());
        } else if (strcmp(argv[i], "--restart") == 0) {
            return static_cast<int>(restartDaemon());
        } else if (strcmp(argv[i], "--reload") == 0) {
            return static_cast<int>(reloadDaemonConfig());
        }
    }

    // Run in appropriate mode
    if (mcp_mode) {
        return runMCPServer();
    } else if (daemon_mode || foreground) {
        return static_cast<int>(runDaemon(foreground));
    } else {
        printUsage(argv[0]);
        return 1;
    }
}
