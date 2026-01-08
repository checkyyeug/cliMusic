# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**XPU (Cross-Platform Professional Audio)** is a modular, CLI-based professional audio playback system designed for the AI era. It follows the Unix philosophy - each module does one thing well, communicating via stdin/stdout.

**Key Characteristics:**
- **AI-Native Design**: MCP (Model Context Protocol) integration is core, not an add-on
- **Professional Audio**: Support for 768kHz, 32-bit, DSD formats
- **4-Layer Architecture**: CLI → REST API → MCP → Agent Protocol
- **Ultra-Low Latency**: <50ms playback target
- **FFT Caching**: 10-100x performance improvement for spectrum analysis

## Common Development Commands

### Building the Project

```bash
# Configure build (Release mode)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON

# Build (Linux/macOS)
make -j$(nproc)

# Build (Windows MSVC)
cmake --build . --config Release -j

# Install
sudo make install  # Linux/macOS
cmake --install . --config Release  # Windows
```

### Running Tests

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test category
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
ctest -R contract --output-on-failure

# Run with verbose output
ctest --verbose --output-on-failure

# Run specific test executable
./tests/unit/test_Protocol
./tests/integration/test_PipelineIntegration
```

### Code Quality Checks

```bash
# Format code
clang-format -i src/**/*.cpp src/**/*.h

# Run static analysis
clang-tidy src/**/*.cpp -- -I src/lib
cppcheck --enable=all --std=c++17 src/
```

### Running the Modules

```bash
# Basic playback pipeline
xpuLoad music.flac | xpuIn2Wav | xpuPlay

# Queue management
xpuQueue add ~/Music/*.flac
xpuQueue play
xpuQueue next

# DSP processing
xpuLoad song.flac | xpuIn2Wav | xpuProcess --eq rock --volume 0.8 | xpuPlay

# Daemon mode
xpuDaemon --daemon
xpuDaemon --foreground
```

## High-Level Architecture

### 4-Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Layer 4: Agent Protocol                    │
│              (智能代理协议 - Phase 2)                        │
├─────────────────────────────────────────────────────────────┤
│                  Layer 3: MCP Protocol                       │
│           (MCP服务器 - Phase 2, 核心)                        │
├─────────────────────────────────────────────────────────────┤
│                  Layer 2: REST API                           │
│         (REST API服务器 - Phase 2, 15+端点)                  │
├─────────────────────────────────────────────────────────────┤
│                  Layer 1: CLI Modules                        │
│    (CLI工具 - Phase 1, 6个核心模块)                          │
└─────────────────────────────────────────────────────────────┘
```

### Core Modules (Phase 1)

**Location**: `xpu/src/`

1. **xpuLoad** (`src/xpuLoad/`) - Audio file parser
   - Supports: FLAC, WAV, ALAC, DSD (DSF/DSD)
   - Outputs: JSON metadata + PCM audio data to stdout
   - Key classes: `AudioFileLoader`, `DSDDecoder`, `MetadataExtractor`

2. **xpuIn2Wav** (`src/xpuIn2Wav/`) - Format converter + FFT cache
   - Converts to standard format (48kHz stereo float)
   - Computes FFT with caching (10-100x speedup)
   - Key classes: `FFTEngine`, `FormatConverter`, `CacheManager`
   - Cache location: `~/.cache/xpu/fft/`

3. **xpuPlay** (`src/xpuPlay/`) - Low-latency audio output
   - Target: <50ms latency
   - Platform backends: WASAPI (Windows), CoreAudio (macOS), ALSA (Linux)
   - Key classes: `AudioBackend`, `AudioBackendWASAPI`, `AudioBackendCoreAudio`, `AudioBackendALSA`

4. **xpuQueue** (`src/xpuQueue/`) - Queue management
   - Persistent queue (JSON format)
   - Playback modes: Sequential, Random, LoopSingle, LoopAll
   - Key classes: `QueueManager`, `QueueStorage`, `PlaybackController`

5. **xpuProcess** (`src/xpuProcess/`) - DSP effects
   - Volume control (0-200%)
   - Fade effects (fade-in/fade-out)
   - 3-band EQ (Bass/Mid/Treble)
   - Key classes: `VolumeControl`, `FadeEffects`, `Equalizer`, `DSPPipeline`

6. **xpuDaemon** (`src/xpuDaemon/`) - Background daemon
   - Orchestrates all modules
   - Configuration hot-reload
   - State persistence
   - Key classes: `DaemonController`, `ProcessManager`, `OrchestrationManager`, `ConfigWatcher`, `StatePersistence`

### Shared Library (libxpu)

**Location**: `xpu/src/lib/`

**Protocol Layer** (`lib/protocol/`):
- `ErrorCode` - 60+ error codes with HTTP status mapping
- `ErrorResponse` - JSON error format
- `Protocol` - Data structures for inter-module communication

**Utils Layer** (`lib/utils/`):
- `Logger` - spdlog integration
- `PlatformUtils` - Cross-platform paths (Linux/macOS/Windows)
- `ConfigLoader` - TOML configuration with atomic writes
- `ConfigValidator` - Configuration validation

**Audio Layer** (`lib/audio/`):
- `AudioFormat` - Format detection (FLAC, WAV, ALAC, DSD, MP3, AAC, OGG, OPUS)
- `AudioMetadata` - Metadata parsing via FFmpeg
- `AudioProperties` - Bitrate calculation, quality detection

**Extension Interfaces** (`lib/interfaces/`):
- All Phase 2-5 interfaces defined in Phase 1 using stub pattern
- Each interface has `isAvailable()` returning false in Phase 1
- All methods return `ErrorCode::NotImplemented` in Phase 1
- This ensures zero breaking changes for future phases

### Configuration System

**File Locations**:
- Linux: `~/.config/xpu/xpuSetting.conf`
- macOS: `~/Library/Application Support/xpu/xpuSetting.conf`
- Windows: `%APPDATA%\xpu\xpuSetting.conf`

**Configuration Structure** (TOML):
```toml
[playback]
device = default
sample_rate = 48000
channels = 2
buffer_size = 2048
latency_ms = 45

[fft_cache]
enabled = true
cache_dir = ~/.cache/xpu/
max_size_mb = 5120
fft_size = 2048

[queue]
persistent = true
queue_file = ~/.config/xpu/queue.json

[logging]
level = info
file = /var/log/xpu/xpu.log
```

### Error Handling

**Error Code Categories**:
- File errors (60-69)
- Audio errors (70-79)
- Network errors (72-79)
- Cache errors (80-89)
- State errors (90-99)
- Resource errors (100-109)

**Error Response Format**:
```json
{
  "error_code": 60,
  "message": "File not found",
  "timestamp": "2026-01-08T12:34:56Z",
  "http_status": 404
}
```

### Key Design Patterns

1. **Unix Philosophy**: Each module is a separate executable communicating via stdin/stdout
2. **Stub Pattern**: Phase 2-5 interfaces are stub implementations returning `NotImplemented`
3. **Platform Abstraction**: Audio backend factory pattern for cross-platform support
4. **Atomic Operations**: Configuration and state files use atomic writes (temp → rename)
5. **Feature Status Markers**: `FeatureStatus` enum tracks phase ownership (CORE_V1, API_V1, etc.)

## Cross-Platform Considerations

**Platform Detection**:
```cpp
#ifdef PLATFORM_WINDOWS
    // Windows-specific code (WASAPI)
#elif defined(PLATFORM_MACOS)
    // macOS-specific code (CoreAudio)
#elif defined(PLATFORM_LINUX)
    // Linux-specific code (ALSA)
#endif
```

**Audio Backend Factory**:
```cpp
std::unique_ptr<AudioBackend> AudioBackend::create() {
#ifdef PLATFORM_WINDOWS
    return std::make_unique<AudioBackendWASAPI>();
#elif defined(PLATFORM_MACOS)
    return std::make_unique<AudioBackendCoreAudio>();
#elif defined(PLATFORM_LINUX)
    return std::make_unique<AudioBackendALSA>();
#endif
}
```

## Data Flow

### Typical Playback Flow

```
1. User: xpuQueue add music.flac
   └─> QueueManager.addTrack()

2. User: xpuQueue play
   └─> OrchestrationManager.startPipeline()

3. Pipeline: xpuLoad music.flac
   └─> AudioFileLoader.load()
       ├─> FFmpeg: Decode audio
       ├─> Extract metadata
       └─> Output: JSON metadata + PCM data

4. Pipeline: xpuIn2Wav
   └─> FFTEngine.computeFFTWithCache()
       ├─> Check cache (SHA256 hash)
       ├─> Compute FFT if needed
       ├─> Save to cache
       └─> Output: FFT data

5. Pipeline: xpuProcess
   └─> DSPPipeline.process()
       ├─> VolumeControl.process()
       ├─> FadeEffects.process()
       └─> Equalizer.process()

6. Pipeline: xpuPlay
   └─> AudioBackend.write()
       ├─> WASAPI/CoreAudio/ALSA
       └─> Output: Audio

7. Status: 10Hz push to stdout
   └─> PlaybackStatus (JSON)
```

## Performance Targets

- **Playback Latency**: <50ms (2048 sample buffer)
- **FFT Cache**: 10-100x speedup (first run ~30s, cached run <3s)
- **Memory Usage**: <100MB typical
- **CPU Usage**: <5% for audio playback

## Testing Strategy

**Test Categories**:
- Unit tests (`tests/unit/`) - Test individual components
- Integration tests (`tests/integration/`) - Test module interactions
- Contract tests (`tests/contract/`) - Test interface compliance
- Performance tests (`tests/performance/`) - Benchmarks
- Error tests (`tests/error/`) - Error handling
- Cross-platform tests (`tests/crossplatform/`) - Platform-specific tests

## Important Notes

1. **Never skip extension interfaces**: All Phase 2-5 interfaces must be defined with stub implementations in Phase 1
2. **Atomic writes required**: Always use atomic writes for configuration and state files
3. **Cross-platform paths**: Use `PlatformUtils` for all file paths
4. **Error propagation**: Always return proper `ErrorCode` from all functions
5. **Real-time status**: xpuPlay pushes status at 10Hz in JSON format to stdout
6. **FFT caching**: Always check cache before computing FFT (critical for performance)
7. **Signal handling**: xpuDaemon handles SIGTERM, SIGINT, SIGHUP
8. **Hot-reload**: Configuration changes are applied via SIGHUP without restart

## Module Dependencies

```
xpuLoad
  ├─ FFmpeg (libavcodec, libavformat, libavutil, libswresample)

xpuIn2Wav
  ├─ FFTW3
  ├─ OpenSSL (SHA256)
  ├─ libsamplerate

xpuPlay
  ├─ PortAudio (or platform-specific: WASAPI, CoreAudio, ALSA)

xpuQueue
  ├─ nlohmann/json

xpuProcess
  └─ (no external dependencies beyond libxpu)

xpuDaemon
  ├─ spdlog
  └─ nlohmann/json
```

## Future Phases

- **Phase 2**: REST API + MCP Server + Agent Protocol
- **Phase 3**: Extended modules (xpuFingerprint, xpuClassify, xpuVisualize, etc.)
- **Phase 4**: Network streaming + DLNA/AirPlay + Distributed architecture
- **Phase 5**: GPU acceleration + AI features + Enterprise features

All Phase 2-5 interfaces are already defined in `xpu/src/lib/interfaces/` with stub implementations.
