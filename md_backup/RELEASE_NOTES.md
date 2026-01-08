# XPU v0.1.0 Release Notes

## Overview

XPU (Cross-Platform Professional Audio) is a high-performance, cross-platform audio playback system designed for audiophiles and professionals.

**Version**: 0.1.0 (Phase 1 - Foundation)
**Release Date**: January 2025
**Status**: Stable Release

## What's New in v0.1.0

This is the initial release of XPU, implementing a complete audio playback pipeline with support for high-resolution audio, professional DSP effects, queue management, and daemon orchestration.

### Core Features

#### 🎵 Audio Loading (xpuLoad)
- Support for 10+ audio formats (FLAC, WAV, ALAC, MP3, AAC, OGG, OPUS, DSF, DSDIFF)
- Automatic high-resolution audio detection (≥ 96kHz)
- Complete metadata extraction (title, artist, album, year, genre, track number)
- JSON output format for easy integration
- Lossless/lossless format detection
- Original sample rate preservation

#### 📊 FFT Visualization (xpuIn2Wav)
- Real-time FFT computation using FFTW3
- SHA256-based cache system for fast repeated analysis
- Multi-threaded FFT computation with hardware concurrency
- SIMD optimization (AVX, AVX2, AVX-512, ARM NEON)
- Hann windowing for accurate frequency analysis
- Configurable FFT size (2048 default)

#### 🔊 Audio Playback (xpuPlay)
- Cross-platform audio output:
  - **Windows**: WASAPI exclusive mode (< 50ms latency)
  - **macOS**: CoreAudio HAL
  - **Linux**: ALSA with dmix plugin
- Support for sample rates from 8kHz to 768kHz
- Bit depth support: 8, 16, 24, 32, 64-bit
- Multi-channel audio (mono, stereo, surround)

#### 🎚️ DSP Processing (xpuProcess)
- **Volume Control**: 0-200% range
- **Fade Effects**: Configurable fade-in/out duration
- **3-Band Equalizer**: Bass, mid, treble control (-20dB to +20dB)
- **EQ Presets**: Rock, pop, classical, jazz, flat
- Real-time processing (faster-than-real-time)

#### 📋 Queue Management (xpuQueue)
- Add/remove/clear tracks
- Shuffle mode
- Repeat modes: No repeat, repeat one, repeat all
- Queue persistence (save/load to disk)
- JSON queue status output
- Track navigation (next, previous, jump to)

#### 🎛️ Daemon Orchestration (xpuDaemon)
- Background daemon with lifecycle management
- Pipeline orchestration and monitoring
- Configuration reload (SIGHUP on Linux/macOS)
- JSON status output with PID and state
- Signal handling (SIGTERM, SIGINT, SIGHUP)
- PID file management

## Technical Highlights

### Performance

- **Load Latency**: 10 seconds of audio in < 1 second
- **DSP Processing**: < 50% CPU usage
- **FFT Computation**: Faster than real-time
- **Memory Usage**: < 500MB for 1 minute of 96kHz audio
- **Playback Latency**: < 50ms on all platforms

### Cross-Platform Support

- **Windows**: Full Windows 10/11 support
- **macOS**: Full macOS 10.15+ support
- **Linux**: Full Ubuntu 20.04+ support
- **Architecture**: x86_64 and ARM64

### Code Quality

- **Language**: C++17
- **Style**: Google C++ Style Guide
- **Testing**: 235+ comprehensive tests
- **Test Coverage**: Unit, integration, performance, error handling, cross-platform

## Installation

### Prerequisites

- CMake 3.15+
- C++17 compiler (GCC 8+, Clang 10+, MSVC 2019+)
- FFmpeg 4.0+
- FFTW3 3.3+
- spdlog 1.8+
- GTest 1.10+ (for testing)

### Build from Source

```bash
git clone https://github.com/xpu/audio-system.git
cd audio-system
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## Usage Examples

### Loading Audio

```bash
# Load audio and get metadata as JSON
xpuLoad /path/to/audio.flac

# Output format:
# {
#   "success": true,
#   "metadata": {
#     "file_path": "/path/to/audio.flac",
#     "format": "FLAC",
#     "title": "Song Title",
#     "artist": "Artist Name",
#     "sample_rate": 96000,
#     "original_sample_rate": 96000,
#     "channels": 2,
#     "bit_depth": 24,
#     "is_lossless": true,
#     "is_high_res": true
#   }
# }
```

### Playing Audio

```bash
# Play audio with default settings
xpuPlay /path/to/audio.flac

# Play with custom volume and EQ
xpuProcess --volume 80 --eq rock | xpuPlay -d default
```

### Queue Management

```bash
# Add tracks to queue
xpuQueue add /path/to/track1.flac
xpuQueue add /path/to/track2.flac

# Start playback
xpuQueue play

# Enable shuffle
xpuQueue shuffle on

# Show queue status
xpuQueue status
```

### Daemon Control

```bash
# Start daemon in foreground
xpuDaemon --foreground

# Start daemon in background
xpuDaemon --daemon

# Check daemon status
xpuDaemon --status

# Stop daemon
xpuDaemon --stop

# Reload configuration
xpuDaemon --reload
```

## Configuration

Configuration files are stored in platform-specific locations:

- **Windows**: `%APPDATA%\xpu\xpuSetting.conf`
- **macOS**: `~/Library/Application Support/xpu/xpuSetting.conf`
- **Linux**: `~/.config/xpu/xpuSetting.conf`

Cache directory:

- **Windows**: `%LOCALAPPDATA%\xpu\cache`
- **macOS**: `~/Library/Caches/xpu`
- **Linux**: `~/.cache/xpu`

## Supported Formats

### Lossless Formats
- FLAC (Free Lossless Audio Codec)
- WAV (Waveform Audio File Format)
- ALAC (Apple Lossless Audio Codec)
- DSF (DSD Stream File)
- DSDIFF (DSD Interchange File Format)

### Lossy Formats
- MP3 (MPEG Audio Layer III)
- AAC (Advanced Audio Coding)
- OGG (Ogg Vorbis)
- OPUS (Opus Audio Codec)

## Known Limitations

This is Phase 1 of XPU development. The following features are planned for future releases:

### Not Implemented in Phase 1
- Extension plugin system
- Network streaming
- Real-time scheduling
- DSD-to-PCM conversion
- Audio device hot-plug detection
- Visualizer output
- Advanced DSP effects (reverb, delay, etc.)

### Platform-Specific Notes
- **Windows**: Windows Service integration not fully implemented
- **macOS**: launchd integration not fully implemented
- **Linux**: Systemd service configuration not included

## Documentation

- **README.md**: Project overview and quick start
- **TASK.md**: Complete task list (169 tasks)
- **PLAN.md**: Implementation plan
- **TEST_SUMMARY.md**: Test suite documentation
- **RELEASE_VALIDATION.md**: Validation checklist

## Testing

Comprehensive test suite with 235+ tests covering:

- **Unit Tests**: Protocol, platform utilities, audio wrappers, daemon, interfaces
- **Integration Tests**: Complete pipeline, queue, daemon orchestration
- **Performance Tests**: Latency, memory, CPU, throughput benchmarks
- **Error Handling Tests**: All error scenarios and edge cases
- **Cross-Platform Tests**: Windows, macOS, Linux compatibility

Run tests:
```bash
cd build
ctest --output-on-failure
```

## Contributing

We welcome contributions! Please see our contributing guidelines for more information.

## License

[License information to be added]

## Acknowledgments

XPU is built on excellent open-source projects:
- FFmpeg for audio decoding
- FFTW3 for FFT computation
- spdlog for logging
- GoogleTest for testing framework

## Support

- **Issues**: https://github.com/xpu/audio-system/issues
- **Discussions**: https://github.com/xpu/audio-system/discussions
- **Documentation**: https://github.com/xpu/audio-system/wiki

## Roadmap

### Phase 2 (Planned)
- Extension plugin system
- Network streaming support
- Real-time scheduling
- DSD-to-PCM conversion
- Advanced DSP effects
- Visualizer support

### Phase 3 (Future)
- Audio device hot-plug
- Multi-room audio
- DLNA/UPnP support
- Web interface
- Mobile apps

---

**Thank you for using XPU!**

The XPU Development Team
