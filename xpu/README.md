# XPU - Cross-Platform Professional Audio Playback System

## Overview

XPU is a modular, CLI-based professional audio playback system following the Unix philosophy. It provides high-quality audio playback with advanced features including FFT caching, support for high-resolution audio (up to 768kHz), and extensive format support.

## Features

### Phase 1 (Current)
- **6 Core Modules**: xpuLoad, xpuIn2Wav, xpuPlay, xpuQueue, xpuProcess, xpuDaemon
- **Professional Audio Quality**: Support for 768kHz, 32-bit, DSD formats
- **Ultra-Low Latency**: <50ms playback latency
- **FFT Caching**: 10-100x performance improvement for spectrum analysis
- **Cross-Platform**: Windows (WASAPI), macOS (CoreAudio), Linux (ALSA)

### Architecture

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

## Modules

### Core Modules (Phase 1)
- **xpuLoad**: Audio file parser supporting FLAC, WAV, ALAC, DSD
- **xpuIn2Wav**: Format converter with FFT caching
- **xpuPlay**: Low-latency audio output
- **xpuQueue**: Queue management with persistence
- **xpuProcess**: Basic DSP processing (volume, EQ, fade)
- **xpuDaemon**: Background daemon with orchestration

### Planned Modules (Phase 2-5)
- xpuFingerprint, xpuClassify, xpuVisualize
- xpuOutWave, xpuPlayCtrl, xpuStream
- xpuMeta, xpuPlaylist, xpuCache, xpuDevice

## Building

### Prerequisites
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- FFmpeg 4.0+
- PortAudio 19.7+
- FFTW3 3.3+
- libsamplerate 0.1.9+
- nlohmann/json 3.7+
- spdlog 1.8+
- cpp-httplib 0.9+
- GoogleTest 1.10+

### Build Instructions

```bash
# Install dependencies (Linux)
./scripts/install.sh

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Install
sudo make install
```

## Usage

### Basic Usage

```bash
# Load and play audio file
xpuLoad music.flac | xpuIn2Wav --rate 96000 | xpuPlay

# Create queue
xpuQueue add music/*.flac
xpuQueue play

# Run as daemon
xpuDaemon --daemon
```

### Module Documentation

See `docs/` for detailed module documentation.

## Configuration

Configuration file: `~/.config/xpu/xpuSetting.conf` (Linux)
                   `~/Library/Application Support/xpu/xpuSetting.conf` (macOS)
                   `%APPDATA%\xpu\xpuSetting.conf` (Windows)

## Testing

```bash
# Run all tests
cd build
ctest

# Run specific test suite
./tests/unit/xpuLoad_tests
./tests/integration/pipeline_tests
```

## Performance

- **Latency**: <50ms end-to-end
- **FFT Cache**: 10-100x speedup
- **Startup**: <100ms
- **Memory**: <100MB for typical usage

## License

[Your License Here]

## Contributing

See `CONTRIBUTING.md` for guidelines.
