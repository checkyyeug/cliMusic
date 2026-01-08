cmake_minimum_required(VERSION 3.15)

# Set policies early to avoid FetchContent warnings
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(xpu VERSION 0.1.0 LANGUAGES C CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    add_definitions(-DPLATFORM_MACOS)
else()
    set(PLATFORM_LINUX TRUE)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# DEPENDENCIES - All using FetchContent for automatic download
# ============================================================================

include(FetchContent)

# spdlog - Logging library
message(STATUS "Fetching spdlog...")
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)
set(CMAKE_PROJECT_spdlog_INCLUDE_BEFORE "${CMAKE_CURRENT_LIST_DIR}/cmake/SuppressWarnings.cmake")
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json - JSON library (header-only)
message(STATUS "Fetching nlohmann/json...")
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)
set(CMAKE_PROJECT_json_INCLUDE_BEFORE "${CMAKE_CURRENT_LIST_DIR}/cmake/SuppressWarnings.cmake")
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json_interface INTERFACE IMPORTED GLOBAL)
    set_target_properties(nlohmann_json_interface PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${json_SOURCE_DIR}/include"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json_interface)
endif()

# GoogleTest - Testing framework
if(BUILD_TESTING)
    message(STATUS "Fetching GoogleTest...")
    # Prevent GoogleTest from overriding our compiler settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    # Suppress GoogleTest project warnings
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

    # Override GoogleTest's cmake_minimum_required to avoid warnings
    set(CMAKE_PROJECT_googletest_INCLUDE_BEFORE "${CMAKE_CURRENT_LIST_DIR}/cmake/GoogleTestOverrides.cmake")

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    # Save original CMAKE_CXX_STANDARD before changing for GoogleTest
    set(OLD_CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED OFF)
    FetchContent_MakeAvailable(googletest)
    # Restore CMAKE_CXX_STANDARD
    set(CMAKE_CXX_STANDARD ${OLD_CMAKE_CXX_STANDARD})
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# ============================================================================
# OPTIONAL EXTERNAL DEPENDENCIES (with warnings)
# ============================================================================

# FFmpeg - Required for audio decoding
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG libavformat libavcodec libavutil libswresample)
endif()

# Manual search for FFmpeg (especially for Windows)
if(NOT FFMPEG_FOUND)
    # Try common FFmpeg installation paths
    set(FFMPEG_PATHS
        "C:/ffmpeg"
        "$ENV{PROGRAMFILES}/ffmpeg"
        "/usr/local"
        "/opt/local"
    )

    find_path(FFMPEG_INCLUDE_DIRS
        NAMES libavformat/avformat.h
        PATHS ${FFMPEG_PATHS}
        PATH_SUFFIXES include
        DOC "FFmpeg include directory"
    )

    find_library(AVFORMAT_LIB
        NAMES avformat libavformat
        PATHS ${FFMPEG_PATHS}
        PATH_SUFFIXES lib
        DOC "FFmpeg avformat library"
    )

    find_library(AVCODEC_LIB
        NAMES avcodec libavcodec
        PATHS ${FFMPEG_PATHS}
        PATH_SUFFIXES lib
        DOC "FFmpeg avcodec library"
    )

    find_library(AVUTIL_LIB
        NAMES avutil libavutil
        PATHS ${FFMPEG_PATHS}
        PATH_SUFFIXES lib
        DOC "FFmpeg avutil library"
    )

    find_library(SWRESAMPLE_LIB
        NAMES swresample libswresample
        PATHS ${FFMPEG_PATHS}
        PATH_SUFFIXES lib
        DOC "FFmpeg swresample library"
    )

    if(AVFORMAT_LIB AND AVCODEC_LIB AND AVUTIL_LIB AND SWRESAMPLE_LIB)
        set(FFMPEG_FOUND TRUE)
        set(FFMPEG_LIBRARIES ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB})
        # For MSVC, add the library directory to link directories
        if(MSVC)
            get_filename_component(FFMPEG_LIB_DIR "${AVFORMAT_LIB}" DIRECTORY)
            if(FFMPEG_LIB_DIR)
                link_directories("${FFMPEG_LIB_DIR}")
            endif()
        endif()
    endif()
endif()

if(NOT FFMPEG_FOUND)
    message(WARNING "FFmpeg not found. Audio decoding will be limited.")
    message(WARNING "Install FFmpeg for full functionality:")
    message(WARNING "  Linux: sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev")
    message(WARNING "  macOS: brew install ffmpeg")
    message(WARNING "  Windows: Download from https://github.com/BtbN/FFmpeg-Builds/releases or install to C:/ffmpeg")
    set(FFMPEG_INCLUDE_DIRS "")
    set(FFMPEG_LIBRARIES "")
    add_definitions(-DFFMPEG_NOT_AVAILABLE)
endif()

# FFTW3 - Required for FFT computation
find_package(FFTW3 QUIET CONFIG)
if(FFTW3_FOUND)
    # Use the imported target from vcpkg
    # vcpkg provides target 'FFTW3::fftw3'
    set(FFTW3_FOUND TRUE)
    if(TARGET FFTW3::fftw3)
        set(FFTW3_LIBRARIES FFTW3::fftw3)
        get_target_property(FFTW3_INCLUDE_DIRS FFTW3::fftw3 INTERFACE_INCLUDE_DIRECTORIES)
    endif()
else()
    # Fallback to pkg-config for non-vcpkg systems
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3 fftw3)
    endif()
endif()

if(NOT FFTW3_FOUND)
    message(WARNING "FFTW3 not found. FFT computation will be disabled.")
    message(WARNING "Install FFTW3 for FFT functionality:")
    message(WARNING "  Linux: sudo apt-get install libfftw3-dev")
    message(WARNING "  macOS: brew install fftw")
    message(WARNING "  Windows: Use vcpkg: vcpkg install fftw3:x64-windows")
    set(FFTW3_INCLUDE_DIRS "")
    set(FFTW3_LIBRARIES "")
    add_definitions(-DFFTW3_NOT_AVAILABLE)
endif()

# PortAudio - Required for audio output
find_package(PortAudio QUIET CONFIG)
if(PortAudio_FOUND)
    # Use the imported target from vcpkg
    # vcpkg provides target 'portaudio' (lowercase)
    set(PORTAUDIO_FOUND TRUE)
    if(TARGET portaudio)
        set(PORTAUDIO_LIBRARIES portaudio)
        get_target_property(PORTAUDIO_INCLUDE_DIRS portaudio INTERFACE_INCLUDE_DIRECTORIES)
    else()
        set(PORTAUDIO_LIBRARIES PortAudio::PortAudio)
    endif()
else()
    # Fallback to pkg-config for non-vcpkg systems
    if(PkgConfig_FOUND)
        pkg_check_modules(PORTAUDIO portaudio-2.0)
    endif()
endif()

if(NOT PORTAUDIO_FOUND)
    message(WARNING "PortAudio not found. Audio output will be limited.")
    message(WARNING "Install PortAudio for audio playback:")
    message(WARNING "  Linux: sudo apt-get install portaudio19-dev")
    message(WARNING "  macOS: brew install portaudio")
    message(WARNING "  Windows: Use vcpkg: vcpkg install portaudio:x64-windows")
    if(NOT PORTAUDIO_INCLUDE_DIRS)
        set(PORTAUDIO_INCLUDE_DIRS "")
    endif()
    if(NOT PORTAUDIO_LIBRARIES)
        set(PORTAUDIO_LIBRARIES "")
    endif()
    add_definitions(-DPORTAUDIO_NOT_AVAILABLE)
endif()

# libsamplerate - Required for audio resampling
# Note: vcpkg's libsamplerate doesn't provide a CMake config, so we use pkg-config or manual find
if(PkgConfig_FOUND)
    pkg_check_modules(SAMPLERATE samplerate)
    # For MSVC, pkg-config might not set the library path correctly
    if(MSVC AND SAMPLERATE_FOUND)
        # Try to find the library in vcpkg directories
        find_library(SAMPLERATE_LIB
            NAMES samplerate libsamplerate
            PATHS
                "${CMAKE_PREFIX_PATH}/lib"
                "$ENV{PROGRAMFILES}/libsamplerate/lib"
                "/c/vcpkg/installed/x64-windows/lib"
        )
        if(SAMPLERATE_LIB)
            set(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIB})
            # Also get the include directory
            find_path(SAMPLERATE_INCLUDE_DIRS
                NAMES samplerate.h
                PATHS
                    "${CMAKE_PREFIX_PATH}/include"
                    "/c/vcpkg/installed/x64-windows/include"
            )
        endif()
    endif()
endif()

# Manual search for vcpkg-installed libsamplerate (if not found via pkg-config)
if(NOT SAMPLERATE_FOUND)
    find_path(SAMPLERATE_INCLUDE_DIRS
        NAMES samplerate.h
        PATHS
            "${CMAKE_PREFIX_PATH}/include"
            "$ENV{PROGRAMFILES}/libsamplerate/include"
            "/usr/include"
            "/usr/local/include"
            "/c/vcpkg/installed/x64-windows/include"
    )

    find_library(SAMPLERATE_LIBRARIES
        NAMES samplerate libsamplerate
        PATHS
            "${CMAKE_PREFIX_PATH}/lib"
            "$ENV{PROGRAMFILES}/libsamplerate/lib"
            "/usr/lib"
            "/usr/local/lib"
            "/c/vcpkg/installed/x64-windows/lib"
    )

    if(SAMPLERATE_INCLUDE_DIRS AND SAMPLERATE_LIBRARIES)
        set(SAMPLERATE_FOUND TRUE)
    endif()
endif()

# For MSVC, add the library directory to the link directories
if(MSVC AND SAMPLERATE_FOUND AND SAMPLERATE_LIBRARIES AND NOT SAMPLERATE_LIBRARIES MATCHES "^-l")
    get_filename_component(SAMPLERATE_LIB_DIR "${SAMPLERATE_LIBRARIES}" DIRECTORY)
    if(SAMPLERATE_LIB_DIR)
        link_directories("${SAMPLERATE_LIB_DIR}")
    endif()
endif()

if(NOT SAMPLERATE_FOUND AND NOT Samplerate_FOUND)
    message(WARNING "libsamplerate not found. Audio resampling will use basic implementation.")
    message(WARNING "Install libsamplerate for high-quality resampling:")
    message(WARNING "  Linux: sudo apt-get install libsamplerate0-dev")
    message(WARNING "  macOS: brew install libsamplerate")
    message(WARNING "  Windows: Use vcpkg: vcpkg install libsamplerate:x64-windows")
    if(NOT SAMPLERATE_INCLUDE_DIRS)
        set(SAMPLERATE_INCLUDE_DIRS "")
    endif()
    if(NOT SAMPLERATE_LIBRARIES)
        set(SAMPLERATE_LIBRARIES "")
    endif()
    add_definitions(-DSAMPLERATE_NOT_AVAILABLE)
endif()

# Threads
find_package(Threads REQUIRED)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/lib/protocol
    ${CMAKE_SOURCE_DIR}/src/lib/utils
    ${CMAKE_SOURCE_DIR}/src/lib/audio
    ${CMAKE_SOURCE_DIR}/src/lib/interfaces
)

if(FFMPEG_INCLUDE_DIRS)
    include_directories(${FFMPEG_INCLUDE_DIRS})
endif()
if(PORTAUDIO_INCLUDE_DIRS)
    include_directories(${PORTAUDIO_INCLUDE_DIRS})
endif()
if(FFTW3_INCLUDE_DIRS)
    include_directories(${FFTW3_INCLUDE_DIRS})
endif()
if(SAMPLERATE_INCLUDE_DIRS)
    include_directories(${SAMPLERATE_INCLUDE_DIRS})
endif()

# ============================================================================
# COMPILER FLAGS
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        add_compile_options(-O3 -march=native)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        add_compile_options(/O2)
    endif()
endif()

# ============================================================================
# SUBDIRECTORIES
# ============================================================================

# Always build the shared library stub
add_subdirectory(src/lib)

# Build core modules (even without dependencies, they'll have stub implementations)
add_subdirectory(src/xpuLoad)
add_subdirectory(src/xpuIn2Wav)
add_subdirectory(src/xpuPlay)
add_subdirectory(src/xpuQueue)
add_subdirectory(src/xpuProcess)
add_subdirectory(src/xpuDaemon)

# Tests
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

install(DIRECTORY configs/
    DESTINATION etc/xpu
    FILES_MATCHING PATTERN "*.conf"
)

install(DIRECTORY scripts/
    DESTINATION share/xpu/scripts
    FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# ============================================================================
# CONFIGURATION SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "XPU Configuration Summary:")
message(STATUS "  Version: ${xpu_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
if(PLATFORM_WINDOWS)
    message(STATUS "  Audio backend: WASAPI")
elseif(PLATFORM_MACOS)
    message(STATUS "  Audio backend: CoreAudio")
else()
    message(STATUS "  Audio backend: ALSA")
endif()
message(STATUS "  Build tests: ${BUILD_TESTING}")
message(STATUS "")
message(STATUS "Dependency Status:")
message(STATUS "  FFmpeg: ${FFMPEG_FOUND}")
message(STATUS "  FFTW3: ${FFTW3_FOUND}")
message(STATUS "  PortAudio: ${PORTAUDIO_FOUND}")
message(STATUS "  libsamplerate: ${SAMPLERATE_FOUND}")
if(NOT FFMPEG_FOUND OR NOT FFTW3_FOUND OR NOT PORTAUDIO_FOUND OR NOT SAMPLERATE_FOUND)
    message(STATUS "")
    message(WARNING "Some dependencies are missing. Features will be disabled.")
    message(WARNING "See BUILD.md for installation instructions.")
endif()
message(STATUS "")
