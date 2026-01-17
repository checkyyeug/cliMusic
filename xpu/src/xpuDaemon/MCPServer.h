/**
 * @file MCPServer.h
 * @brief MCP (Model Context Protocol) Server for XPU
 *
 * Implements stdio-based MCP server following the 2025 specification
 * Allows Claude AI to control XPU via MCP tools and resources
 */

#ifndef XPU_MCP_SERVER_H
#define XPU_MCP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace xpu {
namespace mcp {

/**
 * @brief MCP Tool definition
 */
struct MCPTool {
    std::string name;
    std::string description;
    json input_schema;
};

/**
 * @brief MCP Resource definition
 */
struct MCPResource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
};

/**
 * @brief MCP Prompt definition
 */
struct MCPPrompt {
    std::string name;
    std::string description;
    json arguments;
};

/**
 * @brief MCP Server - stdio JSON-RPC 2.0 implementation
 */
class MCPServer {
public:
    /**
     * @brief Constructor
     */
    MCPServer();

    /**
     * @brief Destructor
     */
    ~MCPServer();

    /**
     * @brief Start the MCP server (stdio mode)
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the MCP server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Set API base URL for calling xpuApi
     */
    void setApiBaseUrl(const std::string& url);

private:
    // JSON-RPC handlers
    json handleRequest(const json& request);
    json handleInitialize(const json& params);
    json handleListTools();
    json handleCallTool(const std::string& name, const json& arguments);
    json handleListResources();
    json handleReadResource(const std::string& uri);
    json handleListPrompts();
    json handleGetPrompt(const std::string& name);

    // Tool implementations
    json toolPlay(const json& args);
    json toolPause(const json& args);
    json toolResume(const json& args);
    json toolStop(const json& args);
    json toolSeek(const json& args);
    json toolVolume(const json& args);
    json toolQueueAdd(const json& args);
    json toolQueueList(const json& args);
    json toolQueueClear(const json& args);
    json toolQueueNext(const json& args);
    json toolGetStatus(const json& args);
    json toolListDevices(const json& args);

    // Resource implementations
    json resourceQueue();
    json resourceStatus();
    json resourceDevices();

    // HTTP client for calling xpuApi
    json callApi(const std::string& endpoint, const json& data);
    json callApi(const std::string& endpoint, const json& data, const std::string& method);

    // Helper functions
    json createSuccessResult(const json& data = json{});
    json createErrorResult(int code, const std::string& message);
    std::string generateRequestId();

private:
    std::atomic<bool> running_;
    std::string api_base_url_;
    std::mutex mutex_;

    // Server info
    struct ServerInfo {
        std::string name = "xpu";
        std::string version = "3.0.0";
        std::string protocol_version = "2025-03-26";
    } server_info_;
};

} // namespace mcp
} // namespace xpu

#endif // XPU_MCP_SERVER_H
