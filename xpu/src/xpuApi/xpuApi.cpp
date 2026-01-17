/**
 * @file xpuApi.cpp
 * @brief Main entry point for xpuApi - XPU REST API Server
 *
 * Usage:
 *   xpuApi [--port <port>] [--host <host>] [--verbose]
 */

#include "APIServer.h"
#include <iostream>
#include <cstring>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace xpu::api;

// Global server pointer for signal handler
static APIServer* g_server = nullptr;

void signalHandler(int signal) {
    if (g_server && g_server->isRunning()) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  --host <address>        Host address to bind to (default: localhost)\n";
    std::cout << "  --port <port>           Port number (default: 8080)\n";
    std::cout << "  -V, --verbose           Enable verbose output\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << "                          # Start on localhost:8080\n";
    std::cout << "  " << program_name << " --port 9000               # Start on port 9000\n";
    std::cout << "  " << program_name << " --host 0.0.0.0 --port 8080  # Listen on all interfaces\n";
}

void printVersion() {
    std::cout << "xpuApi version 3.0.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
    std::cout << "REST API Server with SSE streaming support\n";
}

int main(int argc, char* argv[]) {
    // Set console to UTF-8 mode on Windows
    #ifdef PLATFORM_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    std::string host = "localhost";
    int port = 8080;
    bool verbose = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        } else if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: --host requires an argument\n";
                return 1;
            }
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    std::cerr << "Error: Invalid port number\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires an argument\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Initialize logger
    try {
        auto console = spdlog::stdout_color_mt("xpuApi");
        if (verbose) {
            spdlog::set_level(spdlog::level::debug);
        } else {
            spdlog::set_level(spdlog::level::info);
        }
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
        return 1;
    }

    spdlog::info("xpuApi v4.1.0 starting");

    // Create and start server
    APIServer server(host, port);
    g_server = &server;

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    #ifndef _WIN32
    std::signal(SIGHUP, signalHandler);
    #endif

    if (!server.start()) {
        spdlog::error("Failed to start server");
        return 1;
    }

    std::cout << "XPU API Server started successfully" << std::endl;
    std::cout << "  URL: http://" << host << ":" << port << "/api/v3" << std::endl;
    std::cout << "  Health: http://" << host << ":" << port << "/api/health" << std::endl;
    std::cout << "\nPress Ctrl+C to stop" << std::endl;

    // Wait for server to finish
    server.waitForCompletion();

    spdlog::info("xpuApi shut down gracefully");
    return 0;
}
