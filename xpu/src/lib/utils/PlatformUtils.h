#ifndef XPU_PLATFORM_UTILS_H
#define XPU_PLATFORM_UTILS_H

#include <string>
#include <vector>
#include <fstream>
#include "protocol/ErrorCode.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <shlobj.h>
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <pwd.h>
    #include <limits.h>
    #include <pthread.h>
#endif

namespace xpu {
namespace utils {

/**
 * @brief Platform detection utilities
 */
class PlatformUtils {
public:
    /**
     * @brief Get platform name
     */
    static std::string getPlatformName() {
#ifdef PLATFORM_WINDOWS
        return "Windows";
#elif defined(PLATFORM_MACOS)
        return "macOS";
#elif defined(PLATFORM_LINUX)
        return "Linux";
#else
        return "Unknown";
#endif
    }

    /**
     * @brief Get home directory
     */
    static std::string getHomeDirectory() {
#ifdef PLATFORM_WINDOWS
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
            return std::string(path);
        }
        return "C:\\Users\\Public";
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        const char* home = getenv("HOME");
        if (home) return std::string(home);

        struct passwd *pw = getpwuid(getuid());
        if (pw) return std::string(pw->pw_dir);

        return "/tmp";
#else
        return ".";
#endif
    }

    /**
     * @brief Get XPU configuration directory
     */
    static std::string getConfigDirectory() {
        std::string base;

#ifdef PLATFORM_WINDOWS
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            base = std::string(path);
        } else {
            base = getHomeDirectory();
        }
        return base + "\\xpu";
#elif defined(PLATFORM_MACOS)
        return getHomeDirectory() + "/Library/Application Support/xpu";
#elif defined(PLATFORM_LINUX)
        const char* xdg_config = getenv("XDG_CONFIG_HOME");
        if (xdg_config) {
            return std::string(xdg_config) + "/xpu";
        }
        return getHomeDirectory() + "/.config/xpu";
#else
        return getHomeDirectory() + "/.xpu";
#endif
    }

    /**
     * @brief Get XPU cache directory
     */
    static std::string getCacheDirectory() {
#ifdef PLATFORM_WINDOWS
        return getConfigDirectory() + "\\cache";
#elif defined(PLATFORM_MACOS)
        return getHomeDirectory() + "/Library/Caches/xpu";
#elif defined(PLATFORM_LINUX)
        const char* xdg_cache = getenv("XDG_CACHE_HOME");
        if (xdg_cache) {
            return std::string(xdg_cache) + "/xpu";
        }
        return getHomeDirectory() + "/.cache/xpu";
#else
        return getConfigDirectory() + "/cache";
#endif
    }

    /**
     * @brief Get XPU state directory
     */
    static std::string getStateDirectory() {
#ifdef PLATFORM_WINDOWS
        return getConfigDirectory() + "\\state";
#elif defined(PLATFORM_MACOS)
        return getConfigDirectory() + "/state";
#elif defined(PLATFORM_LINUX)
        const char* xdg_state = getenv("XDG_STATE_HOME");
        if (xdg_state) {
            return std::string(xdg_state) + "/xpu";
        }
        return getHomeDirectory() + "/.local/state/xpu";
#else
        return getConfigDirectory() + "/state";
#endif
    }

    /**
     * @brief Get queue file path
     */
    static std::string getQueueFilePath() {
#ifdef PLATFORM_WINDOWS
        return getConfigDirectory() + "\\queue.json";
#else
        return getConfigDirectory() + "/queue.json";
#endif
    }

    /**
     * @brief Get state file path
     */
    static std::string getStateFilePath() {
#ifdef PLATFORM_WINDOWS
        return getStateDirectory() + "\\state.json";
#else
        return getStateDirectory() + "/state.json";
#endif
    }

    /**
     * @brief Get configuration file path
     */
    static std::string getConfigFilePath() {
#ifdef PLATFORM_WINDOWS
        return getConfigDirectory() + "\\xpuSetting.conf";
#else
        return getConfigDirectory() + "/xpuSetting.conf";
#endif
    }

    /**
     * @brief Get log file path
     */
    static std::string getLogFilePath() {
#ifdef PLATFORM_WINDOWS
        return getConfigDirectory() + "\\xpu.log";
#else
        return getConfigDirectory() + "/xpu.log";
#endif
    }

    /**
     * @brief Create directory recursively
     */
    static bool createDirectory(const std::string& path) {
#ifdef PLATFORM_WINDOWS
        return CreateDirectoryA(path.c_str(), NULL) ||
               GetLastError() == ERROR_ALREADY_EXISTS;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    /**
     * @brief Ensure all XPU directories exist
     */
    static void ensureDirectories() {
        createDirectory(getConfigDirectory());
        createDirectory(getCacheDirectory());
        createDirectory(getStateDirectory());
    }

    /**
     * @brief Get path separator
     */
    static std::string getPathSeparator() {
#ifdef PLATFORM_WINDOWS
        return "\\";
#else
        return "/";
#endif
    }

    /**
     * @brief Join path components
     */
    static std::string joinPath(const std::vector<std::string>& components) {
        if (components.empty()) return "";

        std::string result = components[0];
        std::string sep = getPathSeparator();

        for (size_t i = 1; i < components.size(); ++i) {
            // Avoid double separators
            if (!result.empty() && result.back() != sep[0]) {
                result += sep;
            }
            result += components[i];
        }

        return result;
    }

    /**
     * @brief Get CPU count
     */
    static int getCPUCount() {
#ifdef PLATFORM_WINDOWS
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        return sysconf(_SC_NPROCESSORS_ONLN);
#else
        return 1;
#endif
    }

    /**
     * @brief Get page size
     */
    static size_t getPageSize() {
#ifdef PLATFORM_WINDOWS
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwPageSize;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        return sysconf(_SC_PAGESIZE);
#else
        return 4096;
#endif
    }

    /**
     * @brief Get total memory in bytes
     */
    static uint64_t getTotalMemory() {
#ifdef PLATFORM_WINDOWS
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullTotalPhys;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        return sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
#else
        return 2ULL * 1024 * 1024 * 1024; // 2GB default
#endif
    }

    /**
     * @brief Get available memory in bytes
     */
    static uint64_t getAvailableMemory() {
#ifdef PLATFORM_WINDOWS
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullAvailPhys;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        return sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
#else
        return 1ULL * 1024 * 1024 * 1024; // 1GB default
#endif
    }

    /**
     * @brief Get current thread ID
     */
    static uint64_t getCurrentThreadId() {
#ifdef PLATFORM_WINDOWS
        return GetCurrentThreadId();
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        return (uint64_t)pthread_self();
#else
        return 0;
#endif
    }

    /**
     * @brief Create a temporary file
     */
    static std::string createTempFile(const std::string& prefix = "xpu_") {
#ifdef PLATFORM_WINDOWS
        char temp_path[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_path);
        char temp_file[MAX_PATH];
        GetTempFileNameA(temp_path, prefix.c_str(), 0, temp_file);
        return std::string(temp_file);
#else
        std::string temp_dir = getCacheDirectory();
        return temp_dir + "/" + prefix + "XXXXXX";
#endif
    }

    /**
     * @brief Ensure directory exists (alias for createDirectory)
     */
    static bool ensureDirectoryExists(const std::string& path) {
        return createDirectory(path);
    }

    /**
     * @brief Get OS name
     */
    static std::string getOSName() {
        return getPlatformName();
    }

    /**
     * @brief Get architecture
     */
    static std::string getArchitecture() {
#ifdef PLATFORM_WINDOWS
#ifdef _WIN64
        return "x86_64";
#else
        return "x86";
#endif
#elif defined(__x86_64__)
        return "x86_64";
#elif defined(__i386__)
        return "x86";
#elif defined(__aarch64__) || defined(__arm64__)
        return "arm64";
#elif defined(__arm__)
        return "arm";
#else
        return "unknown";
#endif
    }

    /**
     * @brief Get OS version
     */
    static std::string getOSVersion() {
#ifdef PLATFORM_WINDOWS
        OSVERSIONINFOA osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        #pragma warning(push)
        #pragma warning(disable: 4996)
        GetVersionExA(&osvi);
        #pragma warning(pop)
        return std::to_string(osvi.dwMajorVersion) + "." +
               std::to_string(osvi.dwMinorVersion) + "." +
               std::to_string(osvi.dwBuildNumber);
#else
        return "unknown";
#endif
    }

    /**
     * @brief Get temporary directory
     */
    static std::string getTempDirectory() {
#ifdef PLATFORM_WINDOWS
        char temp_path[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_path);
        return std::string(temp_path);
#else
        return "/tmp";
#endif
    }

    /**
     * @brief Check if little endian
     */
    static bool isLittleEndian() {
        int n = 1;
        return *(char*)&n == 1;
    }

    enum ThreadPriority {
        Low = 0,
        Normal = 1,
        High = 2
    };

    /**
     * @brief Set thread priority
     */
    static ErrorCode setThreadPriority(ThreadPriority priority) {
        (void)priority;  // Suppress unused parameter warning
#ifdef PLATFORM_WINDOWS
        // Not implemented for now
        return ErrorCode::NotImplemented;
#else
        return ErrorCode::NotImplemented;
#endif
    }

    /**
     * @brief Set thread name
     */
    static ErrorCode setThreadName(const std::string& name) {
        (void)name;  // Suppress unused parameter warning
        
#ifdef PLATFORM_WINDOWS
        // Not implemented for now
        return ErrorCode::NotImplemented;
#else
        return ErrorCode::NotImplemented;
#endif
    }

    /**
     * @brief Get file size
     */
    static uint64_t getFileSize(const std::string& filepath) {
#ifdef PLATFORM_WINDOWS
        WIN32_FILE_ATTRIBUTE_DATA file_info;
        if (GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &file_info)) {
            return ((uint64_t)file_info.nFileSizeHigh << 32) | file_info.nFileSizeLow;
        }
        return 0;
#else
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0) {
            return st.st_size;
        }
        return 0;
#endif
    }

    /**
     * @brief Check if file exists
     */
    static bool fileExists(const std::string& filepath) {
#ifdef PLATFORM_WINDOWS
        DWORD attrs = GetFileAttributesA(filepath.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES);
#else
        struct stat st;
        return (stat(filepath.c_str(), &st) == 0);
#endif
    }

    /**
     * @brief Atomic write to file
     */
    static ErrorCode atomicWrite(const std::string& filepath, const std::string& content) {
        std::string temp_file = filepath + ".tmp";
        std::ofstream file(temp_file, std::ios::binary);
        if (!file.is_open()) {
            return ErrorCode::FileWriteError;
        }
        file.write(content.data(), content.size());
        file.close();

#ifdef PLATFORM_WINDOWS
        if (!MoveFileExA(temp_file.c_str(), filepath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return ErrorCode::FileWriteError;
        }
#else
        if (rename(temp_file.c_str(), filepath.c_str()) != 0) {
            return ErrorCode::FileWriteError;
        }
#endif

        return ErrorCode::Success;
    }
};

} // namespace utils
} // namespace xpu

#endif // XPU_PLATFORM_UTILS_H
