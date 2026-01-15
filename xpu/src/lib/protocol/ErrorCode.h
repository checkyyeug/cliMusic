#ifndef XPU_ERROR_CODE_H
#define XPU_ERROR_CODE_H

#include <string>
#include <cstdint>

namespace xpu {

/**
 * @brief Comprehensive error code enumeration for XPU system
 *
 * Error code ranges:
 * - 0-9: General errors
 * - 10-19: CLI errors
 * - 20-29: API errors
 * - 30-39: MCP errors
 * - 40-49: Agent errors
 * - 50-59: Protocol errors
 * - 60-69: File errors
 * - 70-79: Audio errors
 * - 80-89: Cache errors
 * - 90-99: State errors
 * - 100-109: Resource errors
 * - 110-119: Network errors
 */
enum class ErrorCode : int32_t {
    // Success (0)
    Success = 0,

    // General errors (1-9)
    UnknownError = 1,
    NotImplemented = 2,
    NotSupported = 3,
    InvalidArgument = 4,
    InvalidOperation = 5,
    Timeout = 6,
    Canceled = 7,

    // CLI errors (10-19)
    CLIParseError = 10,
    CLIInvalidOption = 11,
    CLIMissingArgument = 12,

    // API errors (20-29)
    APINotFound = 20,
    APIMethodNotAllowed = 21,
    APIInvalidRequest = 22,

    // MCP errors (30-39)
    MCPToolNotFound = 30,
    MCPInvalidParameter = 31,
    MCPExecutionFailed = 32,

    // Agent errors (40-49)
    AgentNotAvailable = 40,
    AgentExecutionFailed = 41,
    AgentTimeout = 42,

    // Protocol errors (50-59)
    ProtocolViolation = 50,
    ProtocolVersionMismatch = 51,
    InvalidMessageFormat = 52,

    // File errors (60-69)
    FileNotFound = 60,
    FileReadError = 61,
    FileWriteError = 62,
    UnsupportedFormat = 63,
    CorruptedFile = 64,
    FilePermissionDenied = 65,
    FileAlreadyExists = 66,
    FileLocked = 67,
    DirectoryNotFound = 68,
    DirectoryCreationFailed = 69,

    // Audio errors (70-79)
    DeviceUnavailable = 70,
    SampleRateNotSupported = 71,
    ChannelConfigurationError = 72,
    BitDepthNotSupported = 73,
    DeviceOpenFailed = 74,
    DeviceInitFailed = 75,
    AudioDecodeError = 78,
    AudioEncodeError = 79,
    BufferUnderrun = 82,
    BufferOverrun = 83,
    DeviceNotFound = 76,
    AudioBackendError = 77,
    AudioFormatMismatch = 81,

    // Cache errors (80-89)
    CacheMiss = 84,
    CacheCorrupted = 85,
    CacheVersionMismatch = 86,
    CacheWriteError = 87,
    CacheReadError = 88,
    CacheDirNotAccessible = 89,
    CacheFull = 90,
    CacheEntryNotFound = 91,
    CacheExpired = 92,
    CacheValidationError = 93,

    // State errors (90-99)
    InvalidState = 94,
    StateConflict = 95,
    StateVersionMismatch = 96,
    StateCorrupted = 97,
    StateWriteError = 98,
    StateReadError = 99,
    StateTransitionError = 100,
    QueueEmpty = 101,
    QueueFull = 102,
    PlayerAlreadyRunning = 103,
    EndOfQueue = 124,

    // Resource errors (100-109)
    OutOfMemory = 104,
    ResourceLocked = 105,
    QuotaExceeded = 106,
    ResourceExhausted = 107,
    HandleInvalid = 108,
    StreamClosed = 109,
    ThreadPoolExhausted = 110,
    FileDescriptorExhausted = 111,
    SocketLimitReached = 112,
    ThreadCreationFailed = 113,

    // Network errors (110-119)
    NetworkUnavailable = 114,
    ConnectionRefused = 115,
    ConnectionTimeout = 116,
    HostNotFound = 117,
    NetworkIOError = 118,
    HTTPError = 119,
    WebSocketError = 120,
    DiscoveryFailed = 121,
    MulticastError = 122,
    UPnPError = 123
};

/**
 * @brief Convert error code to string
 */
inline const char* toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::UnknownError: return "UnknownError";
        case ErrorCode::NotImplemented: return "NotImplemented";
        case ErrorCode::NotSupported: return "NotSupported";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::InvalidOperation: return "InvalidOperation";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::Canceled: return "Canceled";
        case ErrorCode::CLIParseError: return "CLIParseError";
        case ErrorCode::CLIInvalidOption: return "CLIInvalidOption";
        case ErrorCode::CLIMissingArgument: return "CLIMissingArgument";
        case ErrorCode::APINotFound: return "APINotFound";
        case ErrorCode::APIMethodNotAllowed: return "APIMethodNotAllowed";
        case ErrorCode::APIInvalidRequest: return "APIInvalidRequest";
        case ErrorCode::MCPToolNotFound: return "MCPToolNotFound";
        case ErrorCode::MCPInvalidParameter: return "MCPInvalidParameter";
        case ErrorCode::MCPExecutionFailed: return "MCPExecutionFailed";
        case ErrorCode::AgentNotAvailable: return "AgentNotAvailable";
        case ErrorCode::AgentExecutionFailed: return "AgentExecutionFailed";
        case ErrorCode::AgentTimeout: return "AgentTimeout";
        case ErrorCode::ProtocolViolation: return "ProtocolViolation";
        case ErrorCode::ProtocolVersionMismatch: return "ProtocolVersionMismatch";
        case ErrorCode::InvalidMessageFormat: return "InvalidMessageFormat";
        case ErrorCode::FileNotFound: return "FileNotFound";
        case ErrorCode::FileReadError: return "FileReadError";
        case ErrorCode::FileWriteError: return "FileWriteError";
        case ErrorCode::UnsupportedFormat: return "UnsupportedFormat";
        case ErrorCode::CorruptedFile: return "CorruptedFile";
        case ErrorCode::FilePermissionDenied: return "FilePermissionDenied";
        case ErrorCode::FileAlreadyExists: return "FileAlreadyExists";
        case ErrorCode::FileLocked: return "FileLocked";
        case ErrorCode::DirectoryNotFound: return "DirectoryNotFound";
        case ErrorCode::DirectoryCreationFailed: return "DirectoryCreationFailed";
        case ErrorCode::DeviceUnavailable: return "DeviceUnavailable";
        case ErrorCode::SampleRateNotSupported: return "SampleRateNotSupported";
        case ErrorCode::ChannelConfigurationError: return "ChannelConfigurationError";
        case ErrorCode::BitDepthNotSupported: return "BitDepthNotSupported";
        case ErrorCode::DeviceOpenFailed: return "DeviceOpenFailed";
        case ErrorCode::DeviceInitFailed: return "DeviceInitFailed";
        case ErrorCode::AudioDecodeError: return "AudioDecodeError";
        case ErrorCode::AudioEncodeError: return "AudioEncodeError";
        case ErrorCode::BufferUnderrun: return "BufferUnderrun";
        case ErrorCode::BufferOverrun: return "BufferOverrun";
        case ErrorCode::DeviceNotFound: return "DeviceNotFound";
        case ErrorCode::AudioBackendError: return "AudioBackendError";
        case ErrorCode::CacheMiss: return "CacheMiss";
        case ErrorCode::CacheCorrupted: return "CacheCorrupted";
        case ErrorCode::CacheVersionMismatch: return "CacheVersionMismatch";
        case ErrorCode::CacheWriteError: return "CacheWriteError";
        case ErrorCode::CacheReadError: return "CacheReadError";
        case ErrorCode::CacheDirNotAccessible: return "CacheDirNotAccessible";
        case ErrorCode::CacheFull: return "CacheFull";
        case ErrorCode::CacheEntryNotFound: return "CacheEntryNotFound";
        case ErrorCode::CacheExpired: return "CacheExpired";
        case ErrorCode::CacheValidationError: return "CacheValidationError";
        case ErrorCode::InvalidState: return "InvalidState";
        case ErrorCode::StateConflict: return "StateConflict";
        case ErrorCode::StateVersionMismatch: return "StateVersionMismatch";
        case ErrorCode::StateCorrupted: return "StateCorrupted";
        case ErrorCode::StateWriteError: return "StateWriteError";
        case ErrorCode::StateReadError: return "StateReadError";
        case ErrorCode::StateTransitionError: return "StateTransitionError";
        case ErrorCode::QueueEmpty: return "QueueEmpty";
        case ErrorCode::QueueFull: return "QueueFull";
        case ErrorCode::PlayerAlreadyRunning: return "PlayerAlreadyRunning";
        case ErrorCode::EndOfQueue: return "EndOfQueue";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::ResourceLocked: return "ResourceLocked";
        case ErrorCode::QuotaExceeded: return "QuotaExceeded";
        case ErrorCode::ResourceExhausted: return "ResourceExhausted";
        case ErrorCode::HandleInvalid: return "HandleInvalid";
        case ErrorCode::StreamClosed: return "StreamClosed";
        case ErrorCode::ThreadPoolExhausted: return "ThreadPoolExhausted";
        case ErrorCode::FileDescriptorExhausted: return "FileDescriptorExhausted";
        case ErrorCode::SocketLimitReached: return "SocketLimitReached";
        case ErrorCode::ThreadCreationFailed: return "ThreadCreationFailed";
        case ErrorCode::NetworkUnavailable: return "NetworkUnavailable";
        case ErrorCode::ConnectionRefused: return "ConnectionRefused";
        case ErrorCode::ConnectionTimeout: return "ConnectionTimeout";
        case ErrorCode::HostNotFound: return "HostNotFound";
        case ErrorCode::NetworkIOError: return "NetworkIOError";
        case ErrorCode::HTTPError: return "HTTPError";
        case ErrorCode::WebSocketError: return "WebSocketError";
        case ErrorCode::DiscoveryFailed: return "DiscoveryFailed";
        case ErrorCode::MulticastError: return "MulticastError";
        case ErrorCode::UPnPError: return "UPnPError";
        default: return "UnknownErrorCode";
    }
}

/**
 * @brief Check if error code indicates success
 */
inline bool isSuccess(ErrorCode code) {
    return code == ErrorCode::Success;
}

/**
 * @brief Check if error code indicates failure
 */
inline bool isFailure(ErrorCode code) {
    return code != ErrorCode::Success;
}

/**
 * @brief Get error code category for HTTP mapping
 */
inline int getHTTPStatusCode(ErrorCode code) {
    if (isSuccess(code)) return 200;

    switch (code) {
        case ErrorCode::InvalidArgument:
        case ErrorCode::CLIParseError:
        case ErrorCode::CLIInvalidOption:
        case ErrorCode::APIInvalidRequest:
        case ErrorCode::MCPInvalidParameter:
        case ErrorCode::InvalidMessageFormat:
            return 400;

        case ErrorCode::NotImplemented:
        case ErrorCode::NotSupported:
            return 501;

        case ErrorCode::FileNotFound:
        case ErrorCode::APINotFound:
        case ErrorCode::MCPToolNotFound:
        case ErrorCode::CacheEntryNotFound:
        case ErrorCode::DirectoryNotFound:
            return 404;

        case ErrorCode::Timeout:
        case ErrorCode::AgentTimeout:
        case ErrorCode::ConnectionTimeout:
            return 408;

        case ErrorCode::DeviceUnavailable:
        case ErrorCode::AgentNotAvailable:
        case ErrorCode::NetworkUnavailable:
            return 503;

        default:
            return 500;
    }
}

} // namespace xpu

#endif // XPU_ERROR_CODE_H
