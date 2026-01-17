/**
 * @file APIServer.cpp
 * @brief REST API Server implementation for XPU
 */

// Windows headers must be included before httplib.h
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "APIServer.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <random>
#include <iomanip>
#include <sstream>

// Include cpp-httplib for HTTP server functionality
// Note: This is a header-only library
#ifdef CPP_HTTPLIB_ENABLED
    #include "httplib.h"
#else
    #error "cpp-httplib is required. Please install it first."
#endif

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <chrono>
#include <condition_variable>

namespace xpu {
namespace api {

// ============================================================================
// Helper Functions
// ============================================================================

std::string APIServer::generateUUID() {
    // Simple UUID generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << std::hex;
    std::uniform_int_distribution<> dis2(8, 11);
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

json APIServer::createErrorResponse(int code, const std::string& message) {
    json j;
    j["success"] = false;
    j["error"]["code"] = code;
    j["error"]["message"] = message;
    return j;
}

json APIServer::createSuccessResponse(const json& data) {
    json j;
    j["success"] = true;
    if (!data.is_null()) {
        j["data"] = data;
    }
    return j;
}

std::string APIServer::createSession() {
    std::string session_id = generateUUID();
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    sessions_[session_id] = std::make_unique<SessionState>();
    sessions_[session_id]->session_id = session_id;

    spdlog::info("Created session: {}", session_id);
    return session_id;
}

SessionState* APIServer::getSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void APIServer::removeSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        spdlog::info("Removing session: {}", session_id);
        sessions_.erase(it);
    }
}

SessionState* APIServer::getActiveSession() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    // Return first session that is currently playing
    for (auto& pair : sessions_) {
        if (pair.second->is_playing && !pair.second->is_paused) {
            return pair.second.get();
        }
    }
    // If no playing session, return most recent session (last added)
    if (!sessions_.empty()) {
        auto it = sessions_.end();
        --it;
        return it->second.get();
    }
    return nullptr;
}
// ============================================================================
// Constructor/Destructor
// ============================================================================

APIServer::APIServer(const std::string& host, int port)
    : host_(host)
    , port_(port)
    , running_(false) {
    spdlog::info("APIServer created: {}:{}", host, port);
}

APIServer::~APIServer() {
    stop();
}

// ============================================================================
// Server Control
// ============================================================================

bool APIServer::start() {
    if (running_) {
        spdlog::warn("Server already running");
        return false;
    }

    spdlog::info("Starting API server on {}:{}", host_, port_);

    // Create HTTP server
    http_server_ = std::make_unique<httplib::Server>();

    // ====================================================================
    // CORS preflight
    // ====================================================================
    http_server_->Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Session-ID");
        res.set_header("Access-Control-Max-Age", "86400");
        return;
    });

    // ====================================================================
    // Play Control Endpoints
    // ====================================================================

    // POST /api/v3/play
    http_server_->Post("/api/v3/play", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handlePlay(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON: " + std::string(e.what()));
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/pause
    http_server_->Post("/api/v3/pause", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handlePause(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/resume
    http_server_->Post("/api/v3/resume", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleResume(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/stop
    http_server_->Post("/api/v3/stop", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleStop(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/seek
    http_server_->Post("/api/v3/seek", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleSeek(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/volume
    http_server_->Post("/api/v3/volume", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleVolume(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // GET /api/v3/status
    http_server_->Get("/api/v3/status", [this](const httplib::Request& req, httplib::Response& res) {
        json request_data;
        json response_data;

        handleGetStatus(request_data, response_data);

        res.set_content(response_data.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // ====================================================================
    // Queue Management Endpoints
    // ====================================================================

    // POST /api/v3/queue/add
    http_server_->Post("/api/v3/queue/add", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleQueueAdd(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // GET /api/v3/queue
    http_server_->Get("/api/v3/queue", [this](const httplib::Request& req, httplib::Response& res) {
        json request_data;
        json response_data;

        // Parse session from query parameter
        if (req.has_param("session")) {
            request_data["session"] = req.get_param_value("session");
        }

        handleQueueList(request_data, response_data);

        res.set_content(response_data.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // DELETE /api/v3/queue
    http_server_->Delete("/api/v3/queue", [this](const httplib::Request& req, httplib::Response& res) {
        json request_data;
        json response_data;

        // Parse session from query parameter
        if (req.has_param("session")) {
            request_data["session"] = req.get_param_value("session");
        }

        handleQueueClear(request_data, response_data);

        res.set_content(response_data.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // POST /api/v3/queue/next
    http_server_->Post("/api/v3/queue/next", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleQueueNext(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // POST /api/v3/queue/previous
    http_server_->Post("/api/v3/queue/previous", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response_data;

            handleQueuePrevious(request_data, response_data);

            res.set_content(response_data.dump(), "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        } catch (const json::exception& e) {
            json error = createErrorResponse(400, "Invalid JSON");
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        }
    });

    // ====================================================================
    // Device Management Endpoints
    // ====================================================================

    // GET /api/v3/devices
    http_server_->Get("/api/v3/devices", [this](const httplib::Request& req, httplib::Response& res) {
        json request_data;
        json response_data;

        handleListDevices(request_data, response_data);

        res.set_content(response_data.dump(), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // ====================================================================
    // SSE Streaming Endpoint
    // ====================================================================

    // GET /api/v3/stream
    http_server_->Get("/api/v3/stream", [this](const httplib::Request& req, httplib::Response& res) {
        // Set SSE headers
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");
        res.set_header("Access-Control-Allow-Origin", "*");

        // Get session ID
        std::string session_id;
        if (req.has_param("session")) {
            session_id = req.get_param_value("session");
        } else {
            // Send error and close
            std::string error = "event: error\ndata: {\"code\":400,\"message\":\"Missing session parameter\"}\n\n";
            res.set_content(error, "text/event-stream");
            return;
        }

        SessionState* session = getSession(session_id);
        if (!session) {
            std::string error = "event: error\ndata: {\"code\":404,\"message\":\"Session not found\"}\n\n";
            res.set_content(error, "text/event-stream");
            return;
        }

        spdlog::info("SSE stream started for session: {}", session_id);

        // Set content provider for streaming
        auto content_provider = [this, session_id](size_t offset, httplib::DataSink& sink) -> bool {
            SessionState* session = getSession(session_id);
            if (!session) {
                return false;
            }

            // Send status update
            json status;
            status["state"] = session->state;
            status["position"] = session->position;
            status["duration"] = session->duration;
            status["volume"] = session->volume;

            std::string status_event = "event: status\ndata: " + status.dump() + "\n\n";
            return sink.write(status_event.c_str(), status_event.size());
        };

        // Use chunked transfer encoding with content provider
        res.set_chunked_content_provider(
            "text/event-stream",
            content_provider
        );
    });

    // ====================================================================
    // Health check endpoint
    // ====================================================================
    http_server_->Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
        json health;
        health["status"] = "ok";
        health["version"] = "3.0.0";
        health["service"] = "xpu-api";
        res.set_content(health.dump(), "application/json");
    });

    // ====================================================================
    // Start server in a separate thread
    // ====================================================================
    server_thread_ = std::thread([this]() {
        spdlog::info("Server thread started, listening on {}:{}", host_, port_);
        running_ = true;

        if (!http_server_->listen(host_.c_str(), port_)) {
            spdlog::error("Failed to start server on {}:{}", host_, port_);
            running_ = false;
        }
    });

    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return running_;
}

void APIServer::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping API server...");
    running_ = false;

    // Stop all pipelines first
    std::vector<std::string> sessions_to_stop;
    {
        std::lock_guard<std::mutex> lock(pipelines_mutex_);
        if (!pipelines_.empty()) {
            spdlog::info("Stopping {} active playback session(s)...", pipelines_.size());
        }
        for (auto& pair : pipelines_) {
            sessions_to_stop.push_back(pair.first);
        }
    }

    // Stop pipelines without holding the lock (avoid deadlock in stopPipeline)
    for (const auto& session_id : sessions_to_stop) {
        stopPipeline(session_id);
    }

    // Clear all sessions
    {
        std::lock_guard<std::mutex> lock2(sessions_mutex_);
        sessions_.clear();
    }

    // Stop HTTP server - this will unblock listen()
    if (http_server_) {
        spdlog::debug("Stopping HTTP server");
        http_server_->stop();
    }

    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        spdlog::debug("Waiting for server thread to finish");
        server_thread_.join();
    }

    spdlog::info("API server stopped");
}
void APIServer::waitForCompletion() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

// ============================================================================
// Request Handlers
// ============================================================================

void APIServer::handlePlay(const json& request, json& response) {
    if (!request.contains("file")) {
        response = createErrorResponse(400, "Missing 'file' parameter");
        return;
    }

    std::string file_path = request["file"];
    spdlog::info("Play request for file: {}", file_path);

    // Create new session
    std::string session_id = createSession();
    SessionState* session = getSession(session_id);

    // Parse options
    float volume = 1.0f;
    if (request.contains("options") && request["options"].contains("volume")) {
        volume = request["options"]["volume"];
        session->volume = volume;
    }

    // Start pipeline
    if (!startPipeline(file_path, session_id)) {
        response = createErrorResponse(500, "Failed to start playback pipeline");
        removeSession(session_id);
        return;
    }

    session->file_path = file_path;
    session->state = "playing";
    session->is_playing = true;

    // Build response
    json data;
    data["session_id"] = session_id;
    data["stream_url"] = "/api/stream/audio?session=" + session_id;
    data["metadata"]["file"] = file_path;

    response = createSuccessResponse(data);
    spdlog::info("Playback started, session: {}", session_id);
}

void APIServer::handlePause(const json& request, json& response) {
    if (!request.contains("session")) {
        response = createErrorResponse(400, "Missing 'session' parameter");
        return;
    }

    std::string session_id = request["session"];
    SessionState* session = getSession(session_id);

    if (!session) {
        response = createErrorResponse(404, "Session not found");
        return;
    }

    if (!session->is_playing) {
        response = createErrorResponse(400, "Not currently playing");
        return;
    }

    pausePipeline(session_id);
    session->state = "paused";
    session->is_paused = true;

    response = createSuccessResponse();
    spdlog::info("Paused session: {}", session_id);
}

void APIServer::handleResume(const json& request, json& response) {
    if (!request.contains("session")) {
        response = createErrorResponse(400, "Missing 'session' parameter");
        return;
    }

    std::string session_id = request["session"];
    SessionState* session = getSession(session_id);

    if (!session) {
        response = createErrorResponse(404, "Session not found");
        return;
    }

    if (!session->is_paused) {
        response = createErrorResponse(400, "Not currently paused");
        return;
    }

    resumePipeline(session_id);
    session->state = "playing";
    session->is_paused = false;

    response = createSuccessResponse();
    spdlog::info("Resumed session: {}", session_id);
}

void APIServer::handleStop(const json& request, json& response) {
    if (!request.contains("session")) {
        response = createErrorResponse(400, "Missing 'session' parameter");
        return;
    }

    std::string session_id = request["session"];
    SessionState* session = nullptr;

    // Support "active" as an alias for the currently playing session
    if (session_id == "active") {
        session = getActiveSession();
        if (session) {
            session_id = session->session_id;
        }
    } else {
        session = getSession(session_id);
    }

    if (!session) {
        response = createErrorResponse(404, "Session not found");
        return;
    }

    stopPipeline(session_id);
    session->state = "stopped";
    session->is_playing = false;
    session->is_paused = false;
    session->position = 0.0;

    response = createSuccessResponse();
    spdlog::info("Stopped session: {}", session_id);
}

void APIServer::handleSeek(const json& request, json& response) {
    if (!request.contains("session")) {
        response = createErrorResponse(400, "Missing 'session' parameter");
        return;
    }

    if (!request.contains("position")) {
        response = createErrorResponse(400, "Missing 'position' parameter");
        return;
    }

    std::string session_id = request["session"];
    double position = request["position"];

    SessionState* session = getSession(session_id);
    if (!session) {
        response = createErrorResponse(404, "Session not found");
        return;
    }

    // TODO: Implement seek functionality
    // This requires restarting the pipeline with seek parameter
    session->position = position;

    response = createSuccessResponse();
    spdlog::info("Seek session {} to {} seconds", session_id, position);
}

void APIServer::handleVolume(const json& request, json& response) {
    if (!request.contains("session")) {
        response = createErrorResponse(400, "Missing 'session' parameter");
        return;
    }

    if (!request.contains("volume")) {
        response = createErrorResponse(400, "Missing 'volume' parameter");
        return;
    }

    std::string session_id = request["session"];
    float volume = request["volume"];

    SessionState* session = getSession(session_id);
    if (!session) {
        response = createErrorResponse(404, "Session not found");
        return;
    }

    session->volume = volume;

    // TODO: Send volume change to running pipeline

    response = createSuccessResponse();
    spdlog::info("Set volume for session {} to {}", session_id, volume);
}

void APIServer::handleGetStatus(const json& request, json& response) {
    json data;
    data["sessions"] = json::array();

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& pair : sessions_) {
        const SessionState* session = pair.second.get();
        json session_info;
        session_info["session_id"] = session->session_id;
        session_info["state"] = session->state;
        session_info["position"] = session->position;
        session_info["duration"] = session->duration;
        session_info["volume"] = session->volume;
        session_info["file"] = session->file_path;
        data["sessions"].push_back(session_info);
    }

    response = createSuccessResponse(data);
}

void APIServer::handleQueueAdd(const json& request, json& response) {
    // TODO: Implement queue management
    response = createErrorResponse(501, "Queue management not yet implemented");
}

void APIServer::handleQueueList(const json& request, json& response) {
    // TODO: Implement queue management
    response = createErrorResponse(501, "Queue management not yet implemented");
}

void APIServer::handleQueueClear(const json& request, json& response) {
    // TODO: Implement queue management
    response = createErrorResponse(501, "Queue management not yet implemented");
}

void APIServer::handleQueueNext(const json& request, json& response) {
    // TODO: Implement queue management
    response = createErrorResponse(501, "Queue management not yet implemented");
}

void APIServer::handleQueuePrevious(const json& request, json& response) {
    // TODO: Implement queue management
    response = createErrorResponse(501, "Queue management not yet implemented");
}

void APIServer::handleListDevices(const json& request, json& response) {
    // TODO: Implement device listing by calling xpuPlay -l
    response = createErrorResponse(501, "Device listing not yet implemented");
}

// ============================================================================
// Pipeline Orchestration
// ============================================================================

bool APIServer::startPipeline(const std::string& file_path, const std::string& session_id) {
    spdlog::info("Starting pipeline for session: {}, file: {}", session_id, file_path);

    // Get session for volume/DSP parameters
    SessionState* session = getSession(session_id);
    if (!session) {
        spdlog::error("Session not found: {}", session_id);
        return false;
    }

#ifdef _WIN32
    // Windows implementation using CreateProcess with anonymous pipes
    // Pipeline: xpuLoad → xpuIn2Wav → xpuProcess → xpuPlay

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes for communication
    HANDLE pipe_load_in2wav_read, pipe_load_in2wav_write;
    HANDLE pipe_in2wav_proc_read, pipe_in2wav_proc_write;
    HANDLE pipe_proc_play_read, pipe_proc_play_write;

    // Create pipe: xpuLoad → xpuIn2Wav
    if (!CreatePipe(&pipe_load_in2wav_read, &pipe_load_in2wav_write, &sa, 0)) {
        spdlog::error("Failed to create pipe_load_in2wav: {}", GetLastError());
        return false;
    }
    // Set write handle to be inherited by xpuLoad, read handle not inherited by children
    SetHandleInformation(pipe_load_in2wav_read, HANDLE_FLAG_INHERIT, 0);

    // Create pipe: xpuIn2Wav → xpuProcess
    if (!CreatePipe(&pipe_in2wav_proc_read, &pipe_in2wav_proc_write, &sa, 0)) {
        spdlog::error("Failed to create pipe_in2wav_proc: {}", GetLastError());
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        return false;
    }
    SetHandleInformation(pipe_in2wav_proc_read, HANDLE_FLAG_INHERIT, 0);

    // Create pipe: xpuProcess → xpuPlay
    if (!CreatePipe(&pipe_proc_play_read, &pipe_proc_play_write, &sa, 0)) {
        spdlog::error("Failed to create pipe_proc_play: {}", GetLastError());
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        CloseHandle(pipe_in2wav_proc_read);
        CloseHandle(pipe_in2wav_proc_write);
        return false;
    }
    SetHandleInformation(pipe_proc_play_read, HANDLE_FLAG_INHERIT, 0);

    // Prepare command lines
    char module_path[MAX_PATH];
    GetModuleFileNameA(NULL, module_path, MAX_PATH);
    std::string bin_dir = std::string(module_path);
    size_t pos = bin_dir.find_last_of("\\/");
    if (pos != std::string::npos) {
        bin_dir = bin_dir.substr(0, pos);
    }

    std::string xpuLoad_path = bin_dir + "\\xpuLoad.exe";
    std::string xpuIn2Wav_path = bin_dir + "\\xpuIn2Wav.exe";
    std::string xpuProcess_path = bin_dir + "\\xpuProcess.exe";
    std::string xpuPlay_path = bin_dir + "\\xpuPlay.exe";

    // Build command lines
    std::string cmd_load = "\"" + xpuLoad_path + "\" \"" + file_path + "\"";
    std::string cmd_in2wav = "\"" + xpuIn2Wav_path + "\"";
    // xpuProcess expects volume in percent (0-200), not decimal (0.0-2.0)
    std::string volume_arg = "--volume " + std::to_string((int)(session->volume * 100));
    std::string cmd_process = "\"" + xpuProcess_path + "\" " + volume_arg;
    std::string cmd_play = "\"" + xpuPlay_path + "\" -a";  /* Enable auto resampling */

    // IMPORTANT: Create processes in REVERSE order so the pipes are properly connected
    // When parent creates child, only explicitly marked handles are inherited
    // We need to mark the handles each process needs as inheritable before creating it

    // Step 1: Create xpuPlay (last in chain)
    STARTUPINFOA si_play = {};
    PROCESS_INFORMATION pi_play = {};
    si_play.cb = sizeof(STARTUPINFOA);
    si_play.hStdInput = pipe_proc_play_read;
    si_play.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si_play.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si_play.dwFlags |= STARTF_USESTDHANDLES;

    // Mark the read handle as inheritable for xpuPlay
    SetHandleInformation(pipe_proc_play_read, HANDLE_FLAG_INHERIT, 1);

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_play.c_str()),
                        NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si_play, &pi_play)) {
        spdlog::error("Failed to create xpuPlay process: {}", GetLastError());
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        CloseHandle(pipe_in2wav_proc_read);
        CloseHandle(pipe_in2wav_proc_write);
        CloseHandle(pipe_proc_play_read);
        CloseHandle(pipe_proc_play_write);
        return false;
    }

    // Step 2: Create xpuProcess
    STARTUPINFOA si_proc = {};
    PROCESS_INFORMATION pi_proc = {};
    si_proc.cb = sizeof(STARTUPINFOA);
    si_proc.hStdInput = pipe_in2wav_proc_read;
    si_proc.hStdOutput = pipe_proc_play_write;
    si_proc.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si_proc.dwFlags |= STARTF_USESTDHANDLES;

    // Mark handles as inheritable for xpuProcess
    SetHandleInformation(pipe_in2wav_proc_read, HANDLE_FLAG_INHERIT, 1);
    SetHandleInformation(pipe_proc_play_write, HANDLE_FLAG_INHERIT, 1);

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_process.c_str()),
                        NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si_proc, &pi_proc)) {
        spdlog::error("Failed to create xpuProcess process: {}", GetLastError());
        TerminateProcess(pi_play.hProcess, 1);
        CloseHandle(pi_play.hProcess);
        CloseHandle(pi_play.hThread);
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        CloseHandle(pipe_in2wav_proc_read);
        CloseHandle(pipe_in2wav_proc_write);
        CloseHandle(pipe_proc_play_read);
        CloseHandle(pipe_proc_play_write);
        return false;
    }

    // Step 3: Create xpuIn2Wav
    STARTUPINFOA si_in2wav = {};
    PROCESS_INFORMATION pi_in2wav = {};
    si_in2wav.cb = sizeof(STARTUPINFOA);
    si_in2wav.hStdInput = pipe_load_in2wav_read;
    si_in2wav.hStdOutput = pipe_in2wav_proc_write;
    si_in2wav.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si_in2wav.dwFlags |= STARTF_USESTDHANDLES;

    // Mark handles as inheritable for xpuIn2Wav
    SetHandleInformation(pipe_load_in2wav_read, HANDLE_FLAG_INHERIT, 1);
    SetHandleInformation(pipe_in2wav_proc_write, HANDLE_FLAG_INHERIT, 1);

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_in2wav.c_str()),
                        NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si_in2wav, &pi_in2wav)) {
        spdlog::error("Failed to create xpuIn2Wav process: {}", GetLastError());
        TerminateProcess(pi_play.hProcess, 1);
        TerminateProcess(pi_proc.hProcess, 1);
        CloseHandle(pi_play.hProcess);
        CloseHandle(pi_play.hThread);
        CloseHandle(pi_proc.hProcess);
        CloseHandle(pi_proc.hThread);
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        CloseHandle(pipe_in2wav_proc_read);
        CloseHandle(pipe_in2wav_proc_write);
        CloseHandle(pipe_proc_play_read);
        CloseHandle(pipe_proc_play_write);
        return false;
    }

    // Step 4: Create xpuLoad (first in chain)
    STARTUPINFOA si_load = {};
    PROCESS_INFORMATION pi_load = {};
    si_load.cb = sizeof(STARTUPINFOA);
    si_load.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si_load.hStdOutput = pipe_load_in2wav_write;
    si_load.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si_load.dwFlags |= STARTF_USESTDHANDLES;

    // Mark write handle as inheritable for xpuLoad
    SetHandleInformation(pipe_load_in2wav_write, HANDLE_FLAG_INHERIT, 1);

    if (!CreateProcessA(NULL, const_cast<char*>(cmd_load.c_str()),
                        NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si_load, &pi_load)) {
        spdlog::error("Failed to create xpuLoad process: {}", GetLastError());
        TerminateProcess(pi_in2wav.hProcess, 1);
        TerminateProcess(pi_proc.hProcess, 1);
        TerminateProcess(pi_play.hProcess, 1);
        CloseHandle(pi_in2wav.hProcess);
        CloseHandle(pi_in2wav.hThread);
        CloseHandle(pi_proc.hProcess);
        CloseHandle(pi_proc.hThread);
        CloseHandle(pi_play.hProcess);
        CloseHandle(pi_play.hThread);
        CloseHandle(pipe_load_in2wav_read);
        CloseHandle(pipe_load_in2wav_write);
        CloseHandle(pipe_in2wav_proc_read);
        CloseHandle(pipe_in2wav_proc_write);
        CloseHandle(pipe_proc_play_read);
        CloseHandle(pipe_proc_play_write);
        return false;
    }

    // Close our handles to the pipes (children have their own copies)
    CloseHandle(pipe_load_in2wav_write);
    CloseHandle(pipe_load_in2wav_read);
    CloseHandle(pipe_in2wav_proc_write);
    CloseHandle(pipe_in2wav_proc_read);
    CloseHandle(pipe_proc_play_write);
    CloseHandle(pipe_proc_play_read);

    // Close thread handles (we don't need them)
    CloseHandle(pi_load.hThread);
    CloseHandle(pi_in2wav.hThread);
    CloseHandle(pi_proc.hThread);
    CloseHandle(pi_play.hThread);

    // Store pipeline info (use process IDs as pid_t)
    std::lock_guard<std::mutex> lock(pipelines_mutex_);
    pipelines_[session_id] = std::make_unique<PipelineProcess>();
    pipelines_[session_id]->pid_load = pi_load.dwProcessId;
    pipelines_[session_id]->pid_in2wav = pi_in2wav.dwProcessId;
    pipelines_[session_id]->pid_process = pi_proc.dwProcessId;
    pipelines_[session_id]->pid_play = pi_play.dwProcessId;
    pipelines_[session_id]->volume = session->volume;
    pipelines_[session_id]->eq_enabled = false;
    pipelines_[session_id]->eq_bass = 0.0f;
    pipelines_[session_id]->eq_mid = 0.0f;
    pipelines_[session_id]->eq_treble = 0.0f;

    // Keep process handles for later termination (TODO: store in PipelineProcess)
    // For now we rely on PID lookup with OpenProcess

    spdlog::info("Pipeline started: load={}, in2wav={}, process={}, play={}",
                 pi_load.dwProcessId, pi_in2wav.dwProcessId, pi_proc.dwProcessId, pi_play.dwProcessId);
    return true;

#else
    // Unix/Linux implementation using fork + exec
    // Pipeline: xpuLoad → xpuIn2Wav → xpuProcess → xpuPlay
    int pipe_load_in2wav[2];    // xpuLoad → xpuIn2Wav
    int pipe_in2wav_proc[2];    // xpuIn2Wav → xpuProcess
    int pipe_proc_play[2];      // xpuProcess → xpuPlay

    // Create pipes
    if (pipe(pipe_load_in2wav) == -1) {
        spdlog::error("Failed to create pipe_load_in2wav");
        return false;
    }

    if (pipe(pipe_in2wav_proc) == -1) {
        spdlog::error("Failed to create pipe_in2wav_proc");
        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        return false;
    }

    if (pipe(pipe_proc_play) == -1) {
        spdlog::error("Failed to create pipe_proc_play");
        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        close(pipe_in2wav_proc[0]);
        close(pipe_in2wav_proc[1]);
        return false;
    }

    // Fork xpuIn2Wav
    pid_t pid_in2wav = fork();
    if (pid_in2wav == 0) {
        // Child process (xpuIn2Wav)
        dup2(pipe_load_in2wav[0], STDIN_FILENO);
        dup2(pipe_in2wav_proc[1], STDOUT_FILENO);

        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        close(pipe_in2wav_proc[0]);
        close(pipe_in2wav_proc[1]);
        close(pipe_proc_play[0]);
        close(pipe_proc_play[1]);

        execlp("xpuIn2Wav", "xpuIn2Wav", nullptr);
        exit(1);
    }

    // Fork xpuProcess
    pid_t pid_process = fork();
    if (pid_process == 0) {
        // Child process (xpuProcess)
        dup2(pipe_in2wav_proc[0], STDIN_FILENO);
        dup2(pipe_proc_play[1], STDOUT_FILENO);

        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        close(pipe_in2wav_proc[0]);
        close(pipe_in2wav_proc[1]);
        close(pipe_proc_play[0]);
        close(pipe_proc_play[1]);

        // Build xpuProcess arguments with volume
        std::string volume_arg = "--volume=" + std::to_string(session->volume);
        execlp("xpuProcess", "xpuProcess", volume_arg.c_str(), nullptr);
        exit(1);
    }

    // Fork xpuPlay
    pid_t pid_play = fork();
    if (pid_play == 0) {
        // Child process (xpuPlay)
        dup2(pipe_proc_play[0], STDIN_FILENO);

        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        close(pipe_in2wav_proc[0]);
        close(pipe_in2wav_proc[1]);
        close(pipe_proc_play[0]);
        close(pipe_proc_play[1]);

        execlp("xpuPlay", "xpuPlay", nullptr);
        exit(1);
    }

    // Fork xpuLoad
    pid_t pid_load = fork();
    if (pid_load == 0) {
        // Child process (xpuLoad)
        dup2(pipe_load_in2wav[1], STDOUT_FILENO);

        close(pipe_load_in2wav[0]);
        close(pipe_load_in2wav[1]);
        close(pipe_in2wav_proc[0]);
        close(pipe_in2wav_proc[1]);
        close(pipe_proc_play[0]);
        close(pipe_proc_play[1]);

        execlp("xpuLoad", "xpuLoad", file_path.c_str(), nullptr);
        exit(1);
    }

    // Parent process - close unused pipe ends
    close(pipe_load_in2wav[0]);
    close(pipe_load_in2wav[1]);
    close(pipe_in2wav_proc[0]);
    close(pipe_in2wav_proc[1]);
    close(pipe_proc_play[0]);
    close(pipe_proc_play[1]);

    // Store pipeline info
    std::lock_guard<std::mutex> lock(pipelines_mutex_);
    pipelines_[session_id] = std::make_unique<PipelineProcess>();
    pipelines_[session_id]->pid_load = pid_load;
    pipelines_[session_id]->pid_in2wav = pid_in2wav;
    pipelines_[session_id]->pid_process = pid_process;
    pipelines_[session_id]->pid_play = pid_play;
    pipelines_[session_id]->volume = session->volume;
    pipelines_[session_id]->eq_enabled = false;
    pipelines_[session_id]->eq_bass = 0.0f;
    pipelines_[session_id]->eq_mid = 0.0f;
    pipelines_[session_id]->eq_treble = 0.0f;

    spdlog::info("Pipeline started: load={}, in2wav={}, process={}, play={}",
                 pid_load, pid_in2wav, pid_process, pid_play);
    return true;
#endif
}

void APIServer::stopPipeline(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(pipelines_mutex_);
    auto it = pipelines_.find(session_id);
    if (it == pipelines_.end()) {
        return;
    }

    PipelineProcess* pipeline = it->second.get();
    spdlog::info("Stopping pipeline for session: {}", session_id);

#ifdef _WIN32
    // Windows: TerminateProcess
    HANDLE hProcess;
    if (pipeline->pid_load > 0) {
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pipeline->pid_load);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
    if (pipeline->pid_in2wav > 0) {
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pipeline->pid_in2wav);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
    if (pipeline->pid_process > 0) {
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pipeline->pid_process);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
    if (pipeline->pid_play > 0) {
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pipeline->pid_play);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
#else
    // Unix/Linux: kill processes
    if (pipeline->pid_load > 0) kill(pipeline->pid_load, SIGTERM);
    if (pipeline->pid_in2wav > 0) kill(pipeline->pid_in2wav, SIGTERM);
    if (pipeline->pid_process > 0) kill(pipeline->pid_process, SIGTERM);
    if (pipeline->pid_play > 0) kill(pipeline->pid_play, SIGTERM);
#endif

    pipelines_.erase(it);
}

void APIServer::pausePipeline(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(pipelines_mutex_);
    auto it = pipelines_.find(session_id);
    if (it == pipelines_.end()) {
        return;
    }

    PipelineProcess* pipeline = it->second.get();
    spdlog::info("Pausing pipeline for session: {}", session_id);

#ifdef _WIN32
    // TODO: Implement Windows pause
#else
    if (pipeline->pid_play > 0) {
        kill(pipeline->pid_play, SIGSTOP);
    }
#endif
}

void APIServer::resumePipeline(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(pipelines_mutex_);
    auto it = pipelines_.find(session_id);
    if (it == pipelines_.end()) {
        return;
    }

    PipelineProcess* pipeline = it->second.get();
    spdlog::info("Resuming pipeline for session: {}", session_id);

#ifdef _WIN32
    // TODO: Implement Windows resume
#else
    if (pipeline->pid_play > 0) {
        kill(pipeline->pid_play, SIGCONT);
    }
#endif
}

} // namespace api
} // namespace xpu
