/**
 * @file MCPServer.cpp
 * @brief MCP Server implementation for XPU
 */

#include "MCPServer.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/select.h>
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <random>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "utils/PlatformUtils.h"

// For HTTP requests
#ifdef _WIN32
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

namespace xpu {
namespace mcp {

// ============================================================================
// Constructor/Destructor
// ============================================================================

MCPServer::MCPServer()
    : running_(false)
    , api_base_url_("http://localhost:8080") {
    spdlog::info("MCPServer created");
}

MCPServer::~MCPServer() {
    stop();
}

// ============================================================================
// Server Control
// ============================================================================

bool MCPServer::start() {
    if (running_) {
        spdlog::warn("MCP Server already running");
        return false;
    }

    spdlog::info("Starting MCP Server (stdio mode)");

    // Set stdin/stdout to binary mode on Windows
    #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    #endif

    running_ = true;

    // Main request/response loop
    std::string line;
    while (running_ && std::cin) {
        // Read JSON-RPC request from stdin
        line.clear();
        if (!std::getline(std::cin, line)) {
            break;
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        try {
            // Parse JSON-RPC request
            json request = json::parse(line);
            json response = handleRequest(request);

            // Write response to stdout
            std::cout << response.dump() << std::endl;
            std::cout.flush();

        } catch (const json::exception& e) {
            json error;
            error["jsonrpc"] = "2.0";
            error["id"] = nullptr;
            error["error"] = {
                {"code", -32700},
                {"message", "Parse error"},
                {"data", e.what()}
            };
            std::cout << error.dump() << std::endl;
            std::cout.flush();
        }
    }

    spdlog::info("MCP Server stopped");
    return true;
}

void MCPServer::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping MCP Server...");
    running_ = false;
}

void MCPServer::setApiBaseUrl(const std::string& url) {
    api_base_url_ = url;
    spdlog::info("MCP Server API URL set to: {}", url);
}

// ============================================================================
// JSON-RPC Request Handlers
// ============================================================================

json MCPServer::handleRequest(const json& request) {
    // Validate JSON-RPC request
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return createErrorResult(-32600, "Invalid JSON-RPC version");
    }

    std::string method = request.value("method", "");
    json id = request.value("id", json());

    spdlog::debug("MCP Request: method={}", method);

    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;

    try {
        if (method == "initialize") {
            response["result"] = handleInitialize(request["params"]);
        }
        else if (method == "tools/list") {
            response["result"] = handleListTools();
        }
        else if (method == "tools/call") {
            response["result"] = handleCallTool(
                request["params"]["name"],
                request["params"]["arguments"]
            );
        }
        else if (method == "resources/list") {
            response["result"] = handleListResources();
        }
        else if (method == "resources/read") {
            response["result"] = handleReadResource(request["params"]["uri"]);
        }
        else if (method == "prompts/list") {
            response["result"] = handleListPrompts();
        }
        else if (method == "prompts/get") {
            response["result"] = handleGetPrompt(request["params"]["name"]);
        }
        else {
            response["error"] = {
                {"code", -32601},
                {"message", "Method not found"},
                {"data", method}
            };
        }
    } catch (const std::exception& e) {
        response["error"] = {
            {"code", -32603},
            {"message", "Internal error"},
            {"data", e.what()}
        };
    }

    return response;
}

json MCPServer::handleInitialize(const json& params) {
    json result;
    result["protocolVersion"] = server_info_.protocol_version;
    result["serverInfo"] = {
        {"name", server_info_.name},
        {"version", server_info_.version}
    };
    result["capabilities"] = {
        {"tools", {}},
        {"resources", {}}
    };

    spdlog::info("MCP Server initialized: {} v{}",
        server_info_.name, server_info_.version);

    return result;
}

json MCPServer::handleListTools() {
    json result;
    result["tools"] = json::array();

    // Add each tool
    std::vector<MCPTool> tools = {
        {
            "xpu_play",
            "Play a music file. Supports FLAC, WAV, ALAC, DSD formats up to 2.8224 MHz/32-bit.",
            {
                {"type", "object"},
                {"properties", {
                    {"file", {
                        {"type", "string"},
                        {"description", "Path to audio file"}
                    }},
                    {"volume", {
                        {"type", "number"},
                        {"minimum", 0},
                        {"maximum", 1},
                        {"description", "Volume level (0.0-1.0)"}
                    }},
                    {"device", {
                        {"type", "string"},
                        {"description", "Output device name or ID"}
                    }}
                }},
                {"required", {"file"}}
            }
        },
        {
            "xpu_pause",
            "Pause the current playback",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_resume",
            "Resume paused playback",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_stop",
            "Stop playback and clear the buffer",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_seek",
            "Seek to a specific position",
            {
                {"type", "object"},
                {"properties", {
                    {"position", {
                        {"type", "number"},
                        {"description", "Position in seconds"}
                    }}
                }},
                {"required", {"position"}}
            }
        },
        {
            "xpu_volume_set",
            "Set the playback volume",
            {
                {"type", "object"},
                {"properties", {
                    {"volume", {
                        {"type", "number"},
                        {"minimum", 0},
                        {"maximum", 100},
                        {"description", "Volume level (0-100)"}
                    }}
                }},
                {"required", {"volume"}}
            }
        },
        {
            "xpu_queue_add",
            "Add files to the playback queue",
            {
                {"type", "object"},
                {"properties", {
                    {"files", {
                        {"type", "array"},
                        {"items", {{"type", "string"}}},
                        {"description", "List of file paths"}
                    }}
                }},
                {"required", {"files"}}
            }
        },
        {
            "xpu_queue_list",
            "List all items in the playback queue",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_queue_clear",
            "Clear the playback queue",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_queue_next",
            "Skip to the next track",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_get_status",
            "Get current playback status",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        },
        {
            "xpu_list_devices",
            "List available audio devices",
            {
                {"type", "object"},
                {"properties", {}},
                {"required", json::array()}
            }
        }
    };

    for (const auto& tool : tools) {
        json t;
        t["name"] = tool.name;
        t["description"] = tool.description;
        t["inputSchema"] = tool.input_schema;
        result["tools"].push_back(t);
    }

    return result;
}

json MCPServer::handleCallTool(const std::string& name, const json& arguments) {
    spdlog::info("MCP Tool call: {}", name);

    json result;
    result["content"] = json::array();

    try {
        if (name == "xpu_play") {
            result = toolPlay(arguments);
        }
        else if (name == "xpu_pause") {
            result = toolPause(arguments);
        }
        else if (name == "xpu_resume") {
            result = toolResume(arguments);
        }
        else if (name == "xpu_stop") {
            result = toolStop(arguments);
        }
        else if (name == "xpu_seek") {
            result = toolSeek(arguments);
        }
        else if (name == "xpu_volume_set") {
            result = toolVolume(arguments);
        }
        else if (name == "xpu_queue_add") {
            result = toolQueueAdd(arguments);
        }
        else if (name == "xpu_queue_list") {
            result = toolQueueList(arguments);
        }
        else if (name == "xpu_queue_clear") {
            result = toolQueueClear(arguments);
        }
        else if (name == "xpu_queue_next") {
            result = toolQueueNext(arguments);
        }
        else if (name == "xpu_get_status") {
            result = toolGetStatus(arguments);
        }
        else if (name == "xpu_list_devices") {
            result = toolListDevices(arguments);
        }
        else {
            throw std::runtime_error("Unknown tool: " + name);
        }
    } catch (const std::exception& e) {
        result["content"] = json::array({
            {
                {"type", "text"},
                {"text", std::string("Error: ") + e.what()}
            }
        });
        result["isError"] = true;
    }

    return result;
}

json MCPServer::handleListResources() {
    json result;
    result["resources"] = json::array();

    std::vector<MCPResource> resources = {
        {"xpu://queue", "播放队列", "Current playback queue", "application/json"},
        {"xpu://status", "播放状态", "Current playback status", "application/json"},
        {"xpu://devices", "音频设备", "Available audio devices", "application/json"}
    };

    for (const auto& res : resources) {
        json r;
        r["uri"] = res.uri;
        r["name"] = res.name;
        r["description"] = res.description;
        r["mimeType"] = res.mime_type;
        result["resources"].push_back(r);
    }

    return result;
}

json MCPServer::handleReadResource(const std::string& uri) {
    json result;
    result["contents"] = json::array();

    try {
        if (uri == "xpu://queue") {
            result["contents"].push_back(resourceQueue());
        }
        else if (uri == "xpu://status") {
            result["contents"].push_back(resourceStatus());
        }
        else if (uri == "xpu://devices") {
            result["contents"].push_back(resourceDevices());
        }
        else {
            throw std::runtime_error("Unknown resource: " + uri);
        }
    } catch (const std::exception& e) {
        result["contents"].push_back({
            {"type", "text"},
            {"text", std::string("Error: ") + e.what()}
        });
    }

    return result;
}

json MCPServer::handleListPrompts() {
    json result;
    result["prompts"] = json::array();
    return result;
}

json MCPServer::handleGetPrompt(const std::string& name) {
    json result;
    result["messages"] = json::array();
    return result;
}

// ============================================================================
// Tool Implementations
// ============================================================================

json MCPServer::toolPlay(const json& args) {
    std::string file = args["file"];
    json options;
    if (args.contains("volume")) {
        options["volume"] = args["volume"];
    }
    if (args.contains("device")) {
        options["device"] = args["device"];
    }

    json request;
    request["file"] = file;
    if (!options.empty()) {
        request["options"] = options;
    }

    json api_response = callApi("/api/v3/play", request);

    json result;
    result["content"] = json::array();

    if (api_response["success"]) {
        std::string session_id = api_response["data"]["session_id"];
        std::string stream_url = api_response["data"]["stream_url"];

        result["content"].push_back({
            {"type", "text"},
            {"text", "Playback started for: " + file + "\nSession ID: " + session_id}
        });
    } else {
        result["content"].push_back({
            {"type", "text"},
            {"text", "Failed to play: " + api_response["error"]["message"].get<std::string>()}
        });
        result["isError"] = true;
    }

    return result;
}

json MCPServer::toolPause(const json& args) {
    json api_response = callApi("/api/v3/pause", {{"session", "active"}});

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Paused" : "Failed to pause"}}
    });

    return result;
}

json MCPServer::toolResume(const json& args) {
    json api_response = callApi("/api/v3/resume", {{"session", "active"}});

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Resumed" : "Failed to resume"}}
    });

    return result;
}

json MCPServer::toolStop(const json& args) {
    json api_response = callApi("/api/v3/stop", {{"session", "active"}});

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Stopped" : "Failed to stop"}}
    });

    return result;
}

json MCPServer::toolSeek(const json& args) {
    double position = args["position"];

    json api_response = callApi("/api/v3/seek", {
        {"session", "active"},
        {"position", position}
    });

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Seeked to " + std::to_string(position) + "s" : "Failed to seek"}}
    });

    return result;
}

json MCPServer::toolVolume(const json& args) {
    double volume = args["volume"];

    json api_response = callApi("/api/v3/volume", {
        {"session", "active"},
        {"volume", volume / 100.0}  // Convert 0-100 to 0.0-1.0
    });

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Volume set to " + std::to_string(volume) + "%" : "Failed to set volume"}}
    });

    return result;
}

json MCPServer::toolQueueAdd(const json& args) {
    std::vector<std::string> files = args["files"];

    json api_response = callApi("/api/v3/queue/add", {
        {"files", files},
        {"position", -1}
    });

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", "Added " + std::to_string(files.size()) + " files to queue"}}
    });

    return result;
}

json MCPServer::toolQueueList(const json& args) {
    json api_response = callApi("/api/v3/queue", json{});

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response.dump(2)}}
    });

    return result;
}

json MCPServer::toolQueueClear(const json& args) {
    json api_response = callApi("/api/v3/queue", json{}, "DELETE");

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Queue cleared" : "Failed to clear queue"}}
    });

    return result;
}

json MCPServer::toolQueueNext(const json& args) {
    json api_response = callApi("/api/v3/queue/next", {{"session", "active"}});

    json result;
    result["content"] = json::array({
        {{"type", "text"}, {"text", api_response["success"] ? "Skipped to next track" : "Failed to skip"}}
    });

    return result;
}

json MCPServer::toolGetStatus(const json& args) {
    json api_response = callApi("/api/v3/status", json{});

    json result;
    std::string status_text = "Status: " + api_response.dump(2);
    result["content"] = json::array({
        {{"type", "text"}, {"text", status_text}}
    });

    return result;
}

json MCPServer::toolListDevices(const json& args) {
    json api_response = callApi("/api/v3/devices", json{});

    json result;
    std::string devices_text = "Devices: " + api_response.dump(2);
    result["content"] = json::array({
        {{"type", "text"}, {"text", devices_text}}
    });

    return result;
}

// ============================================================================
// Resource Implementations
// ============================================================================

json MCPServer::resourceQueue() {
    json api_response = callApi("/api/v3/queue", json{});

    json content;
    content["type"] = "text";
    content["text"] = "Queue:\n" + api_response.dump(2);
    return content;
}

json MCPServer::resourceStatus() {
    json api_response = callApi("/api/v3/status", json{});

    json content;
    content["type"] = "text";
    content["text"] = "Status:\n" + api_response.dump(2);
    return content;
}

json MCPServer::resourceDevices() {
    json api_response = callApi("/api/v3/devices", json{});

    json content;
    content["type"] = "text";
    content["text"] = "Devices:\n" + api_response.dump(2);
    return content;
}

// ============================================================================
// HTTP Client (simplified implementation)
// ============================================================================

json MCPServer::callApi(const std::string& endpoint, const json& data) {
    return callApi(endpoint, data, "POST");
}

json MCPServer::callApi(const std::string& endpoint, const json& data, const std::string& method) {
    // For now, use a simple implementation with curl or WinHTTP
    // This is a stub that returns mock responses
    // TODO: Implement actual HTTP client

    json response;
    response["success"] = false;
    response["error"] = {
        {"code", 501},
        {"message", "HTTP client not implemented yet"}
    };

    spdlog::warn("HTTP call to {} not implemented (stub mode)", endpoint);
    return response;
}

// ============================================================================
// Helper Functions
// ============================================================================

json MCPServer::createSuccessResult(const json& data) {
    json result;
    if (!data.is_null()) {
        result["data"] = data;
    }
    return result;
}

json MCPServer::createErrorResult(int code, const std::string& message) {
    json error;
    error["code"] = code;
    error["message"] = message;
    return error;
}

std::string MCPServer::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

} // namespace mcp
} // namespace xpu
