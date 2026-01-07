# Implementation Plan: XPU AI-Ready Music Playback System MVP

**Branch**: `001-xpu-mvp-system` | **Date**: 2026-01-07 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-xpu-mvp-system/spec.md`

---

## Summary

XPU is a modular music playback system designed for the AI era. Each functional module is an independent CLI tool following Unix philosophy, composable via pipes. The MVP implements:

1. **Core CLI Pipeline**: xpuLoad (parse) -> xpuIn2Wav (convert + FFT cache) -> xpuProcess (DSP) -> xpuPlay (output)
2. **State Management**: xpuQueue for playlist management, xpuDaemon for orchestration
3. **AI Integration**: MCP protocol exposing all CLI tools to Claude/GPT-4
4. **API Layer**: REST API for third-party integrations

**Core Innovation**: FFT caching architecture - xpuIn2Wav pre-computes frequency spectrum data once, caches to disk, and all subsequent modules (xpuProcess, future xpuVisualize, xpuFingerprint) can reuse the cached data for 10-100x performance improvement.

The system supports lossless formats (FLAC, WAV) up to 96kHz/24-bit stereo, with FFT caching for performance optimization.

---

## Technical Context

**Language/Version**: C++17 (core CLI modules), Python 3.11+ (daemon scripts, testing)
**Primary Dependencies**: FFmpeg (audio decoding), PortAudio (audio output), libsamplerate (resampling), FFTW3 (FFT computation), nlohmann/json (JSON), spdlog (logging)
**Storage**: File system for audio files, local cache for FFT data (~/.cache/xpu/fft on Linux, ~/Library/Caches/xpu/fft on macOS, %LOCALAPPDATA%\xpu\cache\fft on Windows), JSON for state persistence
**Testing**: GoogleTest (C++ unit tests), pytest (Python integration tests), custom CLI contract tests
**Target Platform**: Linux (ALSA), macOS (CoreAudio), Windows (WASAPI) - **FULLY CROSS-PLATFORM**
**Project Type**: Single project with multiple CLI executables
**Performance Goals**: < 100ms playback latency, < 30s FFT computation for 5-min song, < 50ms queue operations, 10x faster FFT cache replay vs recomputation
**Constraints**: < 500MB memory per stream, FLAC/WAV only (no lossy), 96kHz/24-bit max in MVP, single user local only
**Scale/Scope**: 6 CLI modules, 1 daemon, ~15 REST endpoints, 20+ MCP tools, 80%+ test coverage, 100% coverage for audio pipeline

---

## Cross-Platform Support Verification

All dependencies selected for XPU MVP have been verified for cross-platform compatibility across Windows, macOS, and Linux:

### Core Dependencies Cross-Platform Matrix

| Library | Purpose | Windows | macOS | Linux | Notes |
|---------|---------|---------|-------|-------|-------|
| **FFmpeg** | Audio decoding | ✅ WASAPI/DS | ✅ CoreAudio | ✅ ALSA | Industry standard, supports FLAC/WAV on all platforms |
| **PortAudio** | Audio I/O | ✅ WASAPI/DS/MME | ✅ CoreAudio | ✅ ALSA/OSS/JACK | Unified API, auto-selects native backend |
| **FFTW3** | FFT computation | ✅ MinGW/MSVC | ✅ clang | ✅ gcc | Pre-built binaries available for all platforms |
| **libsamplerate** | Resampling | ✅ Win32 | ✅ macOS | ✅ Linux | BSD license, mature cross-platform codebase |
| **nlohmann/json** | JSON parsing | ✅ header-only | ✅ header-only | ✅ header-only | Single file, zero external dependencies |
| **spdlog** | Logging | ✅ VS2013+/MinGW | ✅ clang | ✅ gcc/clang | Header-only or compiled, multi-target support |
| **cpp-httplib** | HTTP server | ✅ Windows | ✅ macOS | ✅ Linux | Header-only, C++11 compatible |
| **GoogleTest** | Unit testing | ✅ VS/MinGW | ✅ macOS | ✅ Linux | CMake-based, universal support |

### Platform-Specific Audio Backend Selection

**Windows:**
- PortAudio automatically uses **WASAPI** (Windows Audio Session API) for low-latency output
- Fallback support: DirectSound, MME
- FFmpeg uses native Windows decoders

**macOS:**
- PortAudio uses **CoreAudio** framework (native macOS audio API)
- FFmpeg uses CoreAudio codecs
- FFTW3 available via Homebrew: `brew install fftw`

**Linux:**
- PortAudio uses **ALSA** (Advanced Linux Sound Architecture)
- Alternative backends: OSS, JACK, PulseAudio
- Package installation: `sudo apt-get install libportaudio2-dev libfftw3-dev`

### Cross-Platform Build System (CMake)

**CMakeLists.txt Structure:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(xpu VERSION 1.0.0 LANGUAGES CXX)

# Cross-platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    set(PLATFORM_NAME "Windows")
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    set(PLATFORM_NAME "macOS")
else()
    set(PLATFORM_LINUX TRUE)
    set(PLATFORM_NAME "Linux")
endif()

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages (cross-platform)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)
find_package(PortAudio REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil)

# Platform-specific configuration
if(PLATFORM_WINDOWS)
    # Windows-specific settings
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    target_link_libraries(xpuPlay PRIVATE winmm)
elseif(PLATFORM_MACOS)
    # macOS-specific settings
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    target_link_libraries(xpuPlay PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY})
elseif(PLATFORM_LINUX)
    # Linux-specific settings
    target_link_libraries(xpuPlay PRIVATE asound pthread)
endif()
```

### Installation Commands by Platform

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake g++ git \
    libavcodec-dev libavformat-dev libavutil-dev \
    libportaudio2-dev libportaudio2 \
    libfftw3-dev \
    libsamplerate0-dev \
    nlohmann-json3-dev \
    libspdlog-dev \
    libgtest-dev
```

**macOS (Homebrew):**
```bash
brew install cmake ffmpeg portaudio fftw libsamplerate nlohmann-json spdlog googletest
```

**Windows (vcpkg):**
```powershell
vcpkg install fftw3 portaudio ffmpeg nlohmann-json spdlog googletest
# Or use Conan:
conan install fftw3/3.3.9 portaudio/19.7.0 --build=missing
```

### Configuration File Paths (Platform-Specific)

| Platform | Config Directory | Cache Directory | Log Directory |
|----------|------------------|-----------------|---------------|
| **Linux** | `~/.config/xpu/` | `~/.cache/xpu/fft/` | `~/.local/state/xpu/` |
| **macOS** | `~/Library/Application Support/xpu/` | `~/Library/Caches/xpu/fft/` | `~/Library/Logs/xpu/` |
| **Windows** | `%APPDATA%\xpu\` | `%LOCALAPPDATA%\xpu\cache\fft\` | `%LOCALAPPDATA%\xpu\logs\` |

**Implementation Note:** Use `std::filesystem` and platform detection macros to automatically resolve these paths at runtime:

```cpp
#ifdef _WIN32
    constexpr const char* CACHE_DIR = "%LOCALAPPDATA%\\xpu\\cache\\fft";
#elif __APPLE__
    constexpr const char* CACHE_DIR = "~/Library/Caches/xpu/fft";
#else
    constexpr const char* CACHE_DIR = "~/.cache/xpu/fft";
#endif
```

### Cross-Platform Compatibility Confirmation

✅ **All dependencies verified for cross-platform support**
✅ **CMake build system ensures consistent compilation across platforms**
✅ **Platform-specific audio backend selection handled automatically**
✅ **Configuration paths follow OS conventions**
✅ **No platform-specific code required in core business logic**

**Status**: MVP is designed to run on Windows 10+, macOS 10.14+, and Ubuntu 20.04+ without code modifications.

---

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**验证项** (基于 `.specify/memory/constitution.md`):

- [x] **代码质量**: 模块设计是否符合单一职责原则? Yes - each CLI module does one thing (parse, convert, play, etc.). 圈复杂度是否可控? Yes - modular design keeps complexity < 15 per module.
- [x] **TDD**: 是否已定义测试策略? Yes - GoogleTest for C++, pytest for Python, contract tests for CLI protocols. 关键路径是否有100%覆盖计划? Yes - audio pipeline (load -> convert -> play) must have 100% coverage.
- [x] **性能**: 是否满足音频性能要求 (< 100ms延迟, < 500MB内存)? Yes - PortAudio for low-latency output, stream processing keeps memory < 500MB.
- [x] **安全**: 外部输入验证是否已规划? Yes - file path validation, codec whitelist (FLAC/WAV only), parameter bounds checking. 网络传输是否使用加密? MVP uses local HTTP (no TLS needed for single-user local), TLS will be added for remote access in v1.1+.

**No violations - all constitution requirements satisfied**

---

## Project Structure

### Documentation (this feature)

```text
specs/001-xpu-mvp-system/
├── spec.md              # Feature specification
├── plan.md              # This file (implementation plan)
├── research.md          # Phase 0 output (technical research)
├── data-model.md        # Phase 1 output (data structures)
├── quickstart.md        # Phase 1 output (5-minute guide)
├── contracts/           # Phase 1 output (API specifications)
│   ├── rest-api.yaml    # OpenAPI spec for REST API
│   ├── mcp-tools.yaml   # MCP tool definitions
│   └── cli-protocol.md  # CLI stdin/stdout protocol
└── tasks.md             # Phase 2 output (task breakdown via /speckit.tasks)
```

### Source Code (repository root)

```text
xpu/
├── src/
│   ├── xpuLoad/         # Audio file parser (FLAC/WAV)
│   │   ├── main.cpp
│   │   ├── audio_parser.cpp
│   │   ├── metadata_extractor.cpp
│   │   └── tests/
│   ├── xpuIn2Wav/       # Format converter + FFT cache (CORE PERFORMANCE MODULE)
│   │   ├── main.cpp
│   │   ├── decoder.cpp           # FFmpeg decoder wrapper
│   │   ├── resampler.cpp         # libsamplerate wrapper
│   │   ├── fft_computer.cpp      # FFTW3 wrapper
│   │   ├── cache_manager.cpp     # FFT cache file I/O
│   │   └── tests/
│   ├── xpuProcess/      # DSP processing (EQ, volume, fade)
│   │   ├── main.cpp
│   │   ├── equalizer.cpp         # FFT-based EQ using cache
│   │   ├── volume_control.cpp
│   │   ├── fade.cpp              # Fade envelope generator
│   │   └── tests/
│   ├── xpuPlay/         # Audio output
│   │   ├── main.cpp
│   │   ├── audio_output.cpp
│   │   ├── device_manager.cpp
│   │   └── tests/
│   ├── xpuQueue/        # Queue management with state persistence
│   │   ├── main.cpp
│   │   ├── queue_manager.cpp
│   │   ├── navigation.cpp        # next/previous/index
│   │   ├── playback_control.cpp  # pause/resume/stop
│   │   ├── state_persistence.cpp # JSON file save/load
│   │   └── tests/
│   ├── xpuDaemon/       # Orchestrator daemon
│   │   ├── main.cpp
│   │   ├── orchestrator.cpp       # Module lifecycle management
│   │   ├── process_manager.cpp    # Child process monitoring
│   │   ├── rest_server.cpp        # cpp-httplib HTTP server
│   │   ├── mcp_server.cpp         # JSON-RPC 2.0 stdio server
│   │   ├── mcp_tool_registry.cpp  # MCP tools dispatcher
│   │   └── tests/
│   └── lib/             # Shared libraries
│       ├── audio/
│       │   ├── fftw_wrapper.cpp
│       │   ├── ffmpeg_decoder.cpp
│       │   ├── portaudio_output.cpp
│       │   └── libsamplerate_wrapper.cpp
│       ├── protocol/
│       │   ├── json_serializer.cpp
│       │   └── cli_protocol.cpp
│       └── utils/
│           ├── config.cpp
│           └── logger.cpp
├── tests/
│   ├── contract/        # CLI protocol contract tests
│   ├── integration/     # End-to-end pipeline tests
│   └── benchmark/       # Performance tests (FFT cache speed, latency, memory)
├── scripts/             # Build and install scripts
├── configs/             # Example configurations
│   └── xpuSetting.conf  # TOML configuration template
└── docs/                # Module documentation

build/                   # Build output
└── bin/                 # Compiled executables
```

**Structure Decision**: Single project with multiple executables. Each CLI module is an independent binary that can be used standalone or chained via pipes. Shared library (lib/) contains common code (audio processing, protocol handling, utilities). This structure maximizes modularity and testability while allowing code reuse.

**FFT Cache Structure** (shared across modules):
```
~/.cache/xpu/fft/<cache_id>/
├── meta.json             # Metadata (cache_id, audio_info, fft_info)
├── magnitude.bin         # Amplitude spectrum [frames × bins × channels]
├── phase.bin             # Phase spectrum [frames × bins × channels]
└── config.json           # FFT configuration (size, hop, window)
```

---

## Implementation Phases

### Phase 1: Pipeline Modules (Priority: P1-P2)

**Deliverables**: Working CLI tools for audio playback pipeline with FFT caching

1. **xpuLoad** - Parse FLAC/WAV, extract metadata, output binary + JSON
2. **xpuIn2Wav** - Convert to WAV, compute FFT cache, output cache_id to stderr
3. **xpuPlay** - Output to audio device with < 100ms latency
4. **xpuQueue** - Manage playback queue with state persistence (JSON file)
5. **xpuProcess** - EQ (using FFT cache), volume, fade effects

**Success Criteria**: Can play local FLAC via `xpuLoad song.flac | xpuIn2Wav | xpuPlay` and FFT cache provides 10x speedup on replay

### Phase 2: API & MCP Layer (Priority: P3)

**Deliverables**: REST API and MCP protocol implementation

1. **REST API Server** - HTTP endpoints for playback control (cpp-httplib)
2. **MCP Server** - JSON-RPC 2.0 over stdio with 20+ tools
3. **API Contracts** - OpenAPI and MCP tool definitions

**Success Criteria**: Can control playback via HTTP and MCP protocol with < 200ms response time

### Phase 3: Orchestration & Skills (Priority: P3)

**Deliverables**: Daemon process and Claude Skills integration

1. **xpuDaemon** - Process orchestrator with state management
2. **Module Lifecycle** - Spawn, monitor, terminate CLI modules (SIGCHLD, SIGPIPE handling)
3. **Resource Management** - Device locks, state versioning, conflict detection
4. **Claude Skills** - Integration scripts for Claude Code

**Success Criteria**: Claude can control playback via natural language with proper error recovery

---

## Design Principles from DESIGN.md

### 1. Architecture: Input Diversity → Unified Processing

```
Input Format Diversity        Unified WAV + FFT Cache      Standardized Pipeline
┌──────────────┐              ┌──────────────┐              ┌──────────────┐
│   FLAC      │              │              │              │              │
│   WAV       │    ┌─────┐    │              │     ┌──────→   │ xpuProcess   │
│  (ALAC, DSD  │───→│ WAV │───→│  FFT Cache   │─────→│  (EQ using  │
│   future)   │    └─────┘    │   (Pre-computed)    │     │   cache)     │
└──────────────┘        ↓       └──────────────┘     └──────┴──────────┘
                  │                                   │
                  └───────────────────────────────────┘
                           Reusable across modules
```

### 2. Module Communication Protocol

**CLI Protocol Version 1.0**:

Request (stdin → module):
```json
{
  "version": "1.0",
  "command": "string",
  "params": {},
  "request_id": "uuid"
}
```

Response (module → stdout):
```json
{
  "version": "1.0",
  "success": true/false,
  "data": null,
  "error": {"code": 1000, "message": "..."},
  "request_id": "uuid"
}
```

Log (module → stderr):
```json
{
  "timestamp": "2026-01-07T12:34:56.789Z",
  "level": "INFO",
  "module": "xpuLoad",
  "message": "..."
}
```

### 3. State Management Architecture

**Stateless CLI Modules + Stateful Daemon**:
- CLI modules: No persistent state, single execution
- xpuDaemon: Maintains all state (queue, playback, configuration)
- State synchronization via REST API and MCP protocol

**State Versioning**:
- Each state change increments version number
- Conflict detection using expected_version in PUT requests
- Returns 409 Conflict if state modified by another client

### 4. Error Codes Specification

| Code | Category | Name | Description |
|------|----------|------|-------------|
| 1000-1099 | Format | FILE_* | File format errors |
| 2000-2099 | Audio | AUDIO_* | Audio device errors |
| 3000-3099 | Queue | QUEUE_* | Queue management errors |
| 4000-4099 | Cache | CACHE_* | FFT cache errors |
| 5000-5099 | Network | NETWORK_* | Network/API errors |
| 6000-6099 | System | SYSTEM_* | Daemon/process errors |

### 5. FFT Cache Design (Core Performance Feature)

**Why FFT Caching**:
1. Pre-compute once during xpuIn2Wav conversion
2. Cache magnitude.bin and phase.bin (binary float32 arrays)
3. Subsequent modules (xpuProcess, xpuVisualize, xpuFingerprint) reuse cache
4. 10-100x faster for frequency-domain operations

**Cache Invalidation**:
- cache_id = SHA-256 hash of audio content
- Recompute automatically if audio file changes
- Manual invalidation via `--no-fft-cache` flag

### 6. Concurrency Model

**Single-User Local (MVP)**:
- No multi-user synchronization needed
- State versioning prevents self-conflict
- Device lock: single audio stream at a time

**Future (v1.1+)**: Multi-user with proper locking

---

## Complexity Tracking

> No violations - all constitution requirements satisfied without complexity tradeoffs

**Architecture Complexity Managed by**:
- Single responsibility per module (Unix philosophy)
- Clear protocol boundaries (JSON stdin/stdout)
- Stateless modules + stateful daemon separation
- FFT cache provides performance without complex optimizations

---

## Module-Specific Design Details

### xpuLoad

**Input**: File path (command line) or stdin
**Output**: Binary format + JSON metadata to stdout
**Key Features**:
- FLAC/WAV parsing only (MVP constraint)
- Metadata extraction (Vorbis comments, ID3 tags)
- File validation (corruption detection)
- Base64 audio_stream option for JSON mode

### xpuIn2Wav (Core Module)

**Input**: Binary format from xpuLoad
**Output**: WAV format (44-byte header + PCM) to stdout, cache_id to stderr
**Key Features**:
- Target sample rate: 44.1/48/88.2/96 kHz (configurable)
- Target bit depth: 16/24/32-bit (configurable)
- FFT size: 1024/2048/4096/8192 (default 2048)
- Hop size: fft_size/2 (50% overlap)
- Window function: hann (default), hamming, blackman
- Quality modes: --high-quality (sinc best), --fast (linear)

**FFT Cache Output** (stderr):
```json
{
  "cache_id": "a1b2c3d4e5f6",
  "cache_path": "/home/user/.cache/xpu/fft/a1b2c3d4e5f6",
  "audio_info": {
    "duration": 245.8,
    "sample_rate": 96000,
    "channels": 2,
    "bit_depth": 32
  },
  "fft_info": {
    "fft_size": 2048,
    "hop_size": 1024,
    "window_function": "hann",
    "num_frames": 22988,
    "freq_bins": 1025,
    "freq_resolution": 46.875
  },
  "files": {
    "magnitude": "magnitude.bin",
    "phase": "phase.bin",
    "meta": "meta.json",
    "config": "config.json"
  }
}
```

### xpuPlay

**Input**: WAV PCM from stdin
**Output**: Audio to device, real-time status to stdout
**Key Features**:
- Device enumeration (--list-devices)
- Buffer size: 512/1024 (default)/2048 samples
- Device auto-selection (default) or manual selection
- < 100ms latency requirement
- Graceful device disconnection handling
- Status events: playing, finished, error

### xpuQueue

**Commands**: add, list, remove, clear, next, previous, pause, resume
**State Persistence**: JSON file at ~/.config/xpu/queue_state.json
**Key Features**:
- Max 1000 tracks (MVP)
- < 50ms operation time requirement
- Shuffle mode (boolean)
- Repeat mode: off/all/one
- Auto-save on modification
- State restored on daemon restart

### xpuProcess

**Input**: WAV PCM from stdin
**Output**: Processed WAV PCM to stdout
**Key Features**:
- EQ presets: flat, rock, pop, classical, jazz
- Frequency-domain EQ using FFT cache
- Volume control: 0.0-1.0 linear scaling
- Fade-in/fade-out: configurable duration (ms)
- Bypass mode if FFT cache unavailable

**EQ Preset Definitions**:
| Preset | 60Hz | 250Hz | 1kHz | 4kHz | 16kHz |
|--------|------|-------|------|------|-------|
| flat   | 0    | 0     | 0    | 0    | 0     |
| rock   | +3   | +1    | 0    | +2   | +3    |
| pop    | +1   | +3    | +2   | +1   | +2    |
| classical | +2  | +1    | 0    | +1   | +2    |
| jazz   | +2   | +1    | +2   | +3   | +1    |

### xpuDaemon

**Responsibilities**:
1. Module orchestration (spawn, monitor, terminate CLI modules)
2. State management (queue, playback, configuration)
3. REST API server (HTTP endpoints)
4. MCP server (JSON-RPC 2.0 stdio)
5. Resource management (device locks, memory limits)
6. Error recovery (child process cleanup, graceful shutdown)

**Process Lifecycle**:
1. Initialize: Parse pipe command, validate modules, allocate pipe_id
2. Start: fork/exec modules, create pipes, setup signal handlers
3. Run: Monitor child processes, collect output, detect errors
4. Cleanup: SIGTERM → wait 5s → SIGKILL if needed, close fds, clean temp files

**Subprocess Monitoring**:
```cpp
class PipeManager {
public:
    PipeID create(const std::vector<Module>& modules);
    void monitor(PipeID id);
    void onChildExit(pid_t pid, int status);
    void cleanup(PipeID id);
private:
    static void sigchldHandler(int sig);
    void checkTimeouts();
};
```

---

## Configuration (xpuSetting.conf - TOML)

```toml
[playback]
target_sample_rate = 96000     # xpuIn2Wav default
target_bit_depth = 32          # xpuIn2Wav default
channels = 2                   # Stereo only in MVP
default_device = "auto"

[fft_cache]
enabled = true                 # Default enabled
cache_dir = "~/.cache/xpu/fft"  # Platform-specific in code
fft_size = 2048                # FFT window size
hop_size = 1024                 # Default fft_size/2
window = "hann"                 # Window function

[queue]
max_tracks = 1000              # MVP limit
auto_save = true
save_interval = 60             # Seconds

[rest_api]
host = "127.0.0.1"
port = 8080
enabled = true

[mcp]
enabled = true
transport = "stdio"

[logging]
level = "info"                  # trace/debug/info/warn/error/fatal
file = "~/.config/xpu/xpu.log"
max_size_mb = 100
```

---

## Quality Metrics & Validation

### Performance Benchmarks (MVP Requirements)

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Playback latency | < 100ms | Time from command to audio output |
| FFT computation (5-min song) | < 30s | Time for xpuIn2Wav first run |
| FFT cache replay | 10x faster | Compare cache hit vs miss |
| Queue operations | < 50ms | Time for add/next/previous |
| Memory per stream | < 500MB | Peak memory during playback |
| MCP response time | < 200ms | Excluding audio playback |
| REST concurrency | 10 clients | No degradation |

### Code Quality Gates

- **Before commit**: All tests pass, coverage ≥80%, static analysis clean
- **Before release**: Integration tests pass, benchmarks pass, security review complete, quickstart.md validated

### Test Coverage Requirements

- Overall: 80%+
- Audio pipeline (xpuLoad → xpuIn2Wav → xpuPlay): 100%
- Queue state management: 90%+
- Error handling paths: 70%+
