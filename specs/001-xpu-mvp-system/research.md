# Technical Research: XPU AI-Ready Music Playback System MVP

**Feature**: 001-xpu-mvp-system
**Date**: 2026-01-07
**Status**: Complete

---

## Overview

This document consolidates technical research and decisions for the XPU MVP implementation. All technology choices are based on the DESIGN.md specifications, industry best practices for audio systems, and the constitution requirements (code quality, TDD, performance, security).

---

## 1. Programming Language & Toolchain

### Decision: C++17 for Core Modules, Python 3.11+ for Scripting

**Rationale**:
- **C++17** provides the performance required for real-time audio processing (< 100ms latency)
- Native C++ integration with FFmpeg, PortAudio, and FFTW3 is mature and well-documented
- Modern C++ features (std::filesystem, std::optional, structured bindings) improve code quality
- **Python 3.11+** for daemon scripts, testing, and tooling (faster development for non-performance-critical code)

**Alternatives Considered**:
- **Rust**: Excellent safety and performance, but FFmpeg/PortAudio bindings less mature than C++
- **Go**: Good concurrency, but GC pauses unacceptable for real-time audio
- **Pure C**: Would work but lacks modern tooling (smart pointers, templates, STL)

**Build System**: CMake 3.20+ (cross-platform, mature, good IDE support)

---

## 2. Audio Processing Libraries

### 2.1 FFmpeg for Audio Decoding

**Decision**: Use FFmpeg libraries (libavcodec, libavformat, libavutil)

**Rationale**:
- Industry standard for audio/video decoding
- Supports FLAC, WAV, and will support future formats (ALAC, DSD)
- Hardware acceleration capabilities for future GPU integration
- Extensive documentation and community support

**Alternatives Considered**:
- **libflac directly**: Simpler but only handles FLAC; would need separate WAV parser
- **dr_flac**: Header-only FLAC decoder, lighter but less feature-rich

### 2.2 PortAudio for Audio Output

**Decision**: PortAudio for cross-platform audio I/O

**Rationale**:
- Single API supports ALSA (Linux), CoreAudio (macOS), WASAPI (Windows)
- Low-latency capabilities (< 10ms achievable)
- Mature and stable (used by Audacity, etc.)

**Cross-Platform Verification**:
- ✅ **Windows**: Supports WASAPI (Windows Audio Session API), DirectSound, MME, ASIO
- ✅ **macOS**: Native CoreAudio framework integration
- ✅ **Linux**: ALSA (Advanced Linux Sound Architecture), OSS, JACK, PulseAudio support
- **Platform Detection**: PortAudio automatically selects appropriate backend at runtime
- **Unified API**: Single code base compiles and runs on all platforms without modification

**Alternatives Considered**:
- **RtAudio**: Similar capabilities, but PortAudio has larger community
- **Platform-specific APIs**: Would require 3 separate code paths (ALSA, CoreAudio, WASAPI)

### 2.3 libsamplerate for Resampling

**Decision**: Secret Rabbit Code (libsamplerate) for high-quality resampling

**Rationale**:
- Best-in-class sample rate conversion quality
- Required for maintaining audio quality when converting between sample rates
- Used by professional audio tools (Audacity, SoX)

**Alternatives Considered**:
- **FFmpeg resampler**: Good quality but libsamplerate is better
- **Linear interpolation**: Fast but unacceptable quality for hi-fi audio

### 2.4 FFTW3 for FFT Computation

**Decision**: FFTW3 (Fastest Fourier Transform in the West)

**Rationale**:
- Fastest FFT implementation available
- Required for FFT caching feature in xpuIn2Wav
- Plan-based optimization for repeated transforms

**Cross-Platform Verification**:
- ✅ **Windows**: Pre-built binaries available (MinGW, MSVC), Conan support
- ✅ **macOS**: Installation packages available, Homebrew: `brew install fftw`
- ✅ **Linux**: Package manager support (Debian, RPM), standard GNU build process
- **Portable Design**: Runs on multiple operating systems and hardware platforms
- **Package Managers**: Available via Conan, vcpkg for unified C/C++ package management

**Alternatives Considered**:
- **KissFFT**: Simpler but slower
- **FFmpeg FFT**: Good but FFTW3 is faster for our use case

---

## 3. JSON & Configuration

### Decision: nlohmann/json for JSON, TOML for Configuration

**Rationale**:
- **nlohmann/json**: Header-only, intuitive API, excellent documentation
- **TOML**: Human-readable configuration format, better than JSON for config files
- **cpptoml**: Lightweight TOML parser for C++

**Cross-Platform Verification**:
- ✅ **nlohmann/json**: Single header file (`json.hpp`), zero external dependencies, standard C++ only
- ✅ **cpptoml**: Cross-platform TOML parser, header-only option available
- ✅ **Portability**: Works on any platform with a C++11+ compiler (MSVC, GCC, Clang)
- **Package Managers**: Available via vcpkg, Conan, Homebrew, apt

**Alternatives Considered**:
- **jsoncpp**: Older API, less intuitive
- **YAML**: More complex syntax, potential security issues

---

## 4. Logging

### Decision: spdlog for C++ Logging

**Rationale**:
- Header-only, fast (asynchronous logging)
- Flexible sinks (file, console, syslog)
- {fmt}-lib based formatting (type-safe, Python-like format strings)

**Cross-Platform Verification**:
- ✅ **Windows**: Visual Studio 2013+, MinGW with g++ 4.9.1+, full support
- ✅ **macOS**: OS X with clang 3.5+, Homebrew: `brew install spdlog`
- ✅ **Linux**: gcc 4.8.1+, clang 3.5+, package manager support
- **Multi-target**: Console (with color), rotating files, daily files, syslog, Windows event log, Windows debugger
- **Flexible**: Header-only or compiled library options

**Alternatives Considered**:
- **Boost.Log**: Powerful but heavy dependency
- **glog**: Google's logger, but async support is limited

---

## 5. Testing Frameworks

### 5.1 GoogleTest for C++ Unit Tests

**Decision**: GoogleTest (gtest + gmock)

**Rationale**:
- Industry standard for C++ testing
- Good assertion macros, test discovery, mocking support
- Integrates well with CMake

**Cross-Platform Verification**:
- ✅ **Windows**: Visual Studio, Cygwin, MinGW, Windows CE, Windows Mobile support
- ✅ **macOS**: Mac OS X full support with clang
- ✅ **Linux**: Native support, CMake-based build
- **Platforms**: Also supports Symbian, PlatformIO with plans for Solaris, AIX, z/OS
- **Installation**: Available via package managers (vcpkg, Conan, Homebrew, apt)

### 5.2 pytest for Python Integration Tests

**Decision**: pytest for Python-based integration tests

**Rationale**:
- De facto standard for Python testing
- Excellent fixture support, parametrization, plugins
- Can spawn CLI processes and test stdin/stdout protocol

**Cross-Platform Verification**:
- ✅ **All Platforms**: Pure Python, runs anywhere Python 3.11+ is available
- ✅ **Installation**: `pip install pytest` works on all platforms
- ✅ ** subprocess**: Cross-platform process spawning

### 5.3 Custom CLI Contract Tests

**Decision**: Custom test framework for CLI protocol validation

**Rationale**:
- Need to validate stdin/stdout JSON protocol between modules
- Will build lightweight test runner using pytest + subprocess

---

## 6. REST API Framework

### Decision: cpp-httplib for Lightweight HTTP Server

**Rationale**:
- Header-only, no external dependencies
- Sufficient for single-user local HTTP server
- Supports RESTful routing, JSON handling

**Cross-Platform Verification**:
- ✅ **Windows**: Full support with Winsock2
- ✅ **macOS**: Full support with BSD sockets
- ✅ **Linux**: Full support with POSIX sockets
- **Design**: Pure C++11 implementation, single header file
- **Network Model**: Blocking I/O + thread pool, internally uses select()/poll() for connection handling
- **SSL/TLS**: Optional OpenSSL support (can be disabled for MVP)

**Alternatives Considered**:
- **Crow**: More feature-rich but heavier
- **Drogon**: Full-featured but overkill for MVP
- **oat++**: Modern but steeper learning curve

**Note**: Future versions may switch to a more robust framework if multi-user support is added

---

## 7. MCP (Model Context Protocol) Implementation

### Decision: Custom MCP Server Implementation

**Rationale**:
- MCP protocol is JSON-RPC 2.0 over stdio
- Can implement using nlohmann/json + standard C++ streams
- No external MCP library required for stdio transport

**MCP Protocol Details**:
- JSON-RPC 2.0 specification
- Requests via stdin, responses via stdout
- Tools (callable functions), Resources (data access), Prompts (templates)

---

## 8. Data Storage Strategy

### 8.1 FFT Cache Storage

**Decision**: File-based cache in platform-specific cache directory

**Structure**:
```
~/.cache/xpu/fft/           # Linux
~/Library/Caches/xpu/fft/    # macOS
%LOCALAPPDATA%\xpu\cache\fft\ # Windows
```

**Format**:
- `meta.json`: Metadata (cache_id, audio_info, fft_info)
- `magnitude.bin`: Binary float32 array (magnitude spectrum)
- `phase.bin`: Binary float32 array (phase spectrum)
- `config.json`: FFT configuration used

**Rationale**:
- Simple, portable, no database dependency
- Files can be invalidated by cache_id (content hash)
- Efficient random access for frequency-domain operations

### 8.2 Queue State Persistence

**Decision**: JSON file for queue state

**Location**: `~/.config/xpu/queue_state.json`

**Rationale**:
- Human-readable for debugging
- Small data size (typical queue < 1000 tracks)
- Easy to migrate/backup

---

## 9. Cross-Platform Audio APIs

### Platform-Specific Backends

| Platform | Audio API | Notes |
|----------|-----------|-------|
| Linux | ALSA | PortAudio uses ALSA backend |
| macOS | CoreAudio | PortAudio uses CoreAudio backend |
| Windows | WASAPI | PortAudio uses WASAPI (exclusive mode for low latency) |

### Device Enumeration

**Decision**: Use PortAudio's device enumeration APIs

**Rationale**:
- Single API across all platforms
- Can query device capabilities (sample rates, latency)
- User can select device via configuration

---

## 10. Performance Optimization Strategy

### 10.1 FFT Caching

**Decision**: Pre-compute FFT during xpuIn2Wav, cache to disk

**Performance Impact**:
- First run: ~30s for 5-minute song (96kHz, 2048 FFT)
- Cached run: ~2s to load cached FFT (10-15x speedup)

### 10.2 Streaming Pipeline

**Decision**: Process audio in chunks, not load entire file

**Strategy**:
- Use fixed buffer size (e.g., 4096 samples per chunk)
- Pipeline: load -> convert -> process -> play operates on chunks
- Reduces memory footprint (< 500MB for entire stream)

### 10.3 Memory Management

**Decision**: Use smart pointers (std::unique_ptr, std::shared_ptr)

**Rationale**:
- Automatic cleanup, no memory leaks
- Clear ownership semantics
- Enables move semantics for zero-copy where possible

---

## 11. Security Considerations

### 11.1 Input Validation

**File Path Validation**:
- Resolve to absolute path
- Check file exists and is readable
- Whitelist allowed extensions (.flac, .wav)
- Prevent directory traversal attacks

**Parameter Validation**:
- Bounds check all numeric inputs (volume 0.0-1.0, sample rate, etc.)
- Validate enum values (EQ presets)

### 11.2 Buffer Security

**Decision**: Use std::vector and std::string, avoid raw C arrays

**Rationale**:
- Automatic bounds checking
- No manual memory management
- Prevents buffer overflow vulnerabilities

### 11.3 Network Security (Future)

**MVP Scope**: Local HTTP only (localhost binding)

**Future v1.1+**:
- TLS/SSL for remote access
- API key authentication
- Rate limiting

---

## 12. Code Quality Standards

### 12.1 Coding Style

**Decision**: Google C++ Style Guide

**Rationale**:
- Clear, consistent conventions
- Tooling support (clang-format, cpplint)
- Widely adopted in industry

### 12.2 Static Analysis

**Tools**:
- **clang-tidy**: Linting and modern C++ suggestions
- **cppcheck**: Static analysis for bugs
- **AddressSanitizer**: Memory error detection (debug builds)

### 12.3 Code Coverage

**Target**: 80%+ overall, 100% for audio pipeline

**Tools**:
- **gcov/lcov**: Code coverage reporting
- **CI integration**: Automated coverage checks

---

## 13. Module Dependencies

### Dependency Graph

```
xpuPlay      --> lib/audio (portaudio_output)
xpuLoad      --> lib/audio (ffmpeg_decoder)
xpuIn2Wav    --> lib/audio (ffmpeg_decoder, fftw_wrapper, libsamplerate)
xpuProcess   --> lib/audio (fftw_wrapper for EQ)
xpuQueue     --> lib/protocol (json_serializer)
xpuDaemon    --> lib/protocol (json_serializer), lib/utils (config, logger)
             --> rest_server, mcp_server
```

### Shared Library (lib/) Structure

- **lib/audio**: Audio processing primitives (decoding, FFT, resampling, output)
- **lib/protocol**: JSON serialization, CLI protocol handling
- **lib/utils**: Configuration, logging, path utilities

---

## 14. Build & Packaging

### Build System: CMake

**Project Structure**:
```cmake
xpu/
├── CMakeLists.txt           # Root project
├── src/
│   ├── CMakeLists.txt       # Source subdirs
│   ├── xpuLoad/CMakeLists.txt
│   ├── xpuIn2Wav/CMakeLists.txt
│   ...
├── lib/CMakeLists.txt       # Shared library
└── tests/CMakeLists.txt     # Test suite
```

**Targets**:
- `xpuLoad`, `xpuIn2Wav`, `xpuPlay`, `xpuQueue`, `xpuProcess`, `xpuDaemon`
- `xpu-lib` (shared library)
- `xpu-tests` (test suite)

### Installation

**Paths**:
- Binaries: `/usr/local/bin/` (Linux), `/usr/local/bin/` (macOS), `C:\Program Files\XPU\bin\` (Windows)
- Config: `~/.config/xpu/` (user-specific)
- Cache: Platform-specific cache directory

---

## 15. Documentation Strategy

### Code Documentation

**Decision**: Doxygen for C++ API documentation

**Rationale**:
- Industry standard for C++
- Generates HTML, PDF, and man pages
- Integrates with CMake

### User Documentation

**Files**:
- `README.md`: Project overview, quick start
- `docs/quickstart.md`: 5-minute getting started guide
- `docs/api.md`: REST API reference
- `docs/mcp-tools.md`: MCP tool reference
- `man/`: Man pages for each CLI tool

---

## Summary of Key Decisions

| Area | Decision | Rationale |
|------|----------|-----------|
| Language | C++17 + Python 3.11+ | Performance + productivity |
| Audio Decode | FFmpeg | Industry standard, format support |
| Audio Output | PortAudio | Cross-platform, low-latency |
| Resampling | libsamplerate | Best quality |
| FFT | FFTW3 | Fastest |
| JSON | nlohmann/json | Easy to use, header-only |
| Logging | spdlog | Fast, async |
| HTTP Server | cpp-httplib | Lightweight, header-only |
| Testing | GoogleTest + pytest | Industry standards |
| Build | CMake | Cross-platform, mature |
| Style | Google C++ Style | Clear, tooling support |

---

## Cross-Platform Compatibility Summary

### Verification Status: ✅ ALL DEPENDENCIES VERIFIED

All libraries and dependencies selected for XPU MVP have been verified for cross-platform support across **Windows 10+**, **macOS 10.14+**, and **Linux (Ubuntu 20.04+)**.

### Cross-Platform Matrix

| Dependency | Windows | macOS | Linux | Package Managers |
|------------|---------|-------|-------|------------------|
| **FFmpeg** | ✅ | ✅ | ✅ | apt, brew, vcpkg |
| **PortAudio** | ✅ WASAPI | ✅ CoreAudio | ✅ ALSA | apt, brew, conan |
| **FFTW3** | ✅ MinGW/MSVC | ✅ clang | ✅ gcc | apt, brew, vcpkg |
| **libsamplerate** | ✅ Win32 | ✅ macOS | ✅ Linux | apt, brew, conan |
| **nlohmann/json** | ✅ header-only | ✅ header-only | ✅ header-only | apt, brew, vcpkg |
| **spdlog** | ✅ VS2013+/MinGW | ✅ clang | ✅ gcc/clang | apt, brew, vcpkg |
| **cpp-httplib** | ✅ Winsock2 | ✅ BSD sockets | ✅ POSIX | header-only |
| **GoogleTest** | ✅ VS/MinGW | ✅ macOS | ✅ Linux | apt, brew, vcpkg |

### Platform-Specific Audio Backend Selection

**Windows:**
- PortAudio automatically uses **WASAPI** (Windows Audio Session API)
- Fallback: DirectSound, MME
- FFmpeg uses native Windows codecs

**macOS:**
- PortAudio uses **CoreAudio** framework
- FFmpeg uses CoreAudio codecs
- Native low-latency audio support

**Linux:**
- PortAudio uses **ALSA** (Advanced Linux Sound Architecture)
- Alternative backends: OSS, JACK, PulseAudio
- Package installation via apt/dnf/pacman

### Build System Verification

**CMake** ensures consistent compilation across all platforms:
- Cross-platform detection (WIN32, APPLE, UNIX)
- Automatic library discovery via `find_package()` and `pkg-config`
- Platform-specific linking (winmm on Windows, CoreAudio on macOS, asound on Linux)
- Unified build commands: `cmake .. && make` (Linux/macOS), `cmake --build .` (Windows)

### Configuration Paths (OS Conventions)

| Platform | Config | Cache | Logs |
|----------|--------|-------|------|
| **Linux** | `~/.config/xpu/` | `~/.cache/xpu/fft/` | `~/.local/state/xpu/` |
| **macOS** | `~/Library/Application Support/xpu/` | `~/Library/Caches/xpu/fft/` | `~/Library/Logs/xpu/` |
| **Windows** | `%APPDATA%\xpu\` | `%LOCALAPPDATA%\xpu\cache\fft\` | `%LOCALAPPDATA%\xpu\logs\` |

### Code Portability Guarantees

✅ **Zero platform-specific code in core business logic**
✅ **Single code base compiles on all three platforms without modification**
✅ **Platform detection handled by CMake preprocessor macros**
✅ **Audio backend selection automatic via PortAudio**
✅ **File system paths abstracted via std::filesystem**

### Testing Across Platforms

**MVP Development Workflow:**
1. Primary development on Linux (Ubuntu 20.04+)
2. Continuous testing on macOS (via GitHub Actions or local testing)
3. Windows testing (via GitHub Actions Windows runners or local testing)

**CI/CD Requirements:**
- GitHub Actions matrix builds (ubuntu-latest, macos-latest, windows-latest)
- Automated tests run on all three platforms
- Platform-specific regression tests

---

## Open Questions Resolved

All technical questions have been answered through this research. No NEEDS CLARIFICATION items remain. The implementation can proceed with confidence in the technology choices.

**Cross-Platform Status**: ✅ **VERIFIED** - All dependencies confirmed for Windows, macOS, and Linux support.

**Status**: Ready for Phase 1 (Design & Contracts)
