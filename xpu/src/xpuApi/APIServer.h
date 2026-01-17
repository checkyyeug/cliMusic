/**
 * @file APIServer.h
 * @brief REST API Server for XPU - Phase 2
 *
 * Provides RESTful API endpoints for controlling XPU audio playback
 * Supports SSE (Server-Sent Events) for streaming audio data
 */

#ifndef XPU_API_SERVER_H
#define XPU_API_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

#ifdef _WIN32
#include <process.h>
typedef int pid_t;
#else
#include <sys/types.h>
#endif

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace xpu {
namespace api {

/**
 * @brief Playback session state
 */
struct SessionState {
    std::string session_id;
    std::string file_path;
    bool is_playing;
    bool is_paused;
    double position;
    double duration;
    float volume;
    std::string state;  // "idle", "playing", "paused", "stopped"

    SessionState()
        : is_playing(false)
        , is_paused(false)
        , position(0.0)
        , duration(0.0)
        , volume(1.0f)
        , state("idle") {}
};

/**
 * @brief SSE connection state
 */
struct SSEConnection {
    std::string session_id;
    std::function<void(const std::string& event, const json& data)> send_event;
    std::atomic<bool> active;

    SSEConnection() : active(false) {}
};

/**
 * @brief REST API Server
 */
class APIServer {
public:
    /**
     * @brief Constructor
     * @param host Host address to bind to
     * @param port Port number to listen on
     */
    APIServer(const std::string& host = "localhost", int port = 8080);

    /**
     * @brief Destructor
     */
    ~APIServer();

    /**
     * @brief Start the API server
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the API server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Wait for server to finish (blocking)
     */
    void waitForCompletion();

private:
    // HTTP request handlers
    void handlePlay(const json& request, json& response);
    void handlePause(const json& request, json& response);
    void handleResume(const json& request, json& response);
    void handleStop(const json& request, json& response);
    void handleSeek(const json& request, json& response);
    void handleVolume(const json& request, json& response);
    void handleGetStatus(const json& request, json& response);

    // Queue management handlers
    void handleQueueAdd(const json& request, json& response);
    void handleQueueList(const json& request, json& response);
    void handleQueueClear(const json& request, json& response);
    void handleQueueNext(const json& request, json& response);
    void handleQueuePrevious(const json& request, json& response);

    // Device management handlers
    void handleListDevices(const json& request, json& response);

    // SSE streaming
    void handleSSEStream(const std::string& session_id,
                        std::function<void(const std::string& data)> send);

    // Pipeline orchestration
    bool startPipeline(const std::string& file_path, const std::string& session_id);
    void stopPipeline(const std::string& session_id);
    void pausePipeline(const std::string& session_id);
    void resumePipeline(const std::string& session_id);

    // Session management
    std::string createSession();
    SessionState* getSession(const std::string& session_id);
    void removeSession(const std::string& session_id);

    // Utility functions
    std::string generateUUID();
    json createErrorResponse(int code, const std::string& message);
    json createSuccessResponse(const json& data = json{});

private:
    std::string host_;
    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;

    // Session management
    std::unordered_map<std::string, std::unique_ptr<SessionState>> sessions_;
    std::mutex sessions_mutex_;

    // Pipeline process management
    struct PipelineProcess {
        pid_t pid_load;
        pid_t pid_in2wav;
        pid_t pid_play;
        std::thread read_thread;
    };
    std::unordered_map<std::string, std::unique_ptr<PipelineProcess>> pipelines_;
    std::mutex pipelines_mutex_;
};

} // namespace api
} // namespace xpu

#endif // XPU_API_SERVER_H
