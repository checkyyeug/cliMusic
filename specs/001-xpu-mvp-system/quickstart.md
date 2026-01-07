# XPU Quick Start Guide

**5 Minutes to Your First Music Playback**

---

## Prerequisites

**Supported Platforms:**
- ✅ **Windows 10+** (WASAPI audio backend)
- ✅ **macOS 10.14+** (CoreAudio audio backend)
- ✅ **Linux** (ALSA audio backend, tested on Ubuntu 20.04+)

**Required Tools:**
- CMake 3.20+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Git

---

## Installation (2 minutes)

### Linux (Ubuntu/Debian)

```bash
# Clone repository
git clone https://github.com/your-org/xpu.git
cd xpu

# Install all dependencies
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

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests (optional)
ctest --output-on-failure

# Install (optional)
sudo make install
```

**Note:** On other Linux distributions, use your package manager (dnf, pacman, etc.) to install the equivalent dependencies.

### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Clone repository
git clone https://github.com/your-org/xpu.git
cd xpu

# Install dependencies via Homebrew
brew install cmake ffmpeg portaudio fftw libsamplerate \
             nlohmann-json spdlog googletest

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

# Run tests (optional)
ctest --output-on-failure

# Install (optional)
sudo make install
```

**Note:** XPU uses macOS CoreAudio natively via PortAudio for low-latency audio output.

### Windows

#### Option A: Using vcpkg (Recommended)

```powershell
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install

# Clone repository
git clone https://github.com/your-org/xpu.git
cd xpu

# Install dependencies via vcpkg
C:\vcpkg\vcpkg install fftw3:x64-windows portaudio:x64-windows \
    ffmpeg:x64-windows nlohmann-json:x64-windows \
    spdlog:x64-windows gtest:x64-windows

# Build with Visual Studio 2019/2022
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
        -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Run tests (optional)
ctest -C Release --output-on-failure
```

#### Option B: Using Conan

```powershell
# Install Conan
pip install conan

# Clone repository
git clone https://github.com/your-org/xpu.git
cd xpu

# Install dependencies via Conan
conan install . --build=missing -s build_type=Release

# Build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Run tests (optional)
ctest -C Release --output-on-failure
```

#### Option C: Manual Dependency Installation

1. **Install dependencies manually:**
   - [FFTW3](http://www.fftw.org/install/windows.html) - Extract to `C:\fftw`
   - [PortAudio](http://www.portaudio.com/download.html) - Use MinGW or MSVC binaries
   - [FFmpeg](https://www.gyan.dev/ffmpeg/builds/) - Extract to `C:\ffmpeg`
   - [libsamplerate](https://github.com/libsndfile/libsamplerate/releases) - Build from source
   - [nlohmann/json](https://github.com/nlohmann/json/releases) - Header-only, extract to `include/`
   - [spdlog](https://github.com/gabime/spdlog/releases) - Header-only or compiled
   - [GoogleTest](https://github.com/google/googletest/releases) - Build from source

2. **Set environment variables:**
   ```powershell
   set FFTW3_DIR=C:\fftw
   set PORTAUDIO_DIR=C:\portaudio
   set FFMPEG_DIR=C:\ffmpeg
   ```

3. **Build:**
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 16 2019" -A x64
   cmake --build . --config Release
   ```

**Note:** XPU uses Windows WASAPI audio backend for low-latency audio output on Windows 10+.

---

## Your First Playback (1 minute)

### Option 1: Direct Pipeline

```bash
# Play a single file
xpuLoad ~/Music/song.flac | xpuIn2Wav | xpuPlay
```

### Option 2: Using Queue

```bash
# Add files to queue and play
xpuQueue add ~/Music/*.flac
xpuQueue play
```

---

## Basic Commands (2 minutes)

### Queue Control

```bash
# Add multiple files
xpuQueue add ~/Music/Album/*.flac

# View queue
xpuQueue list

# Next/Previous track
xpuQueue next
xpuQueue previous

# Pause/Resume
xpuQueue pause
xpuQueue resume

# Clear queue
xpuQueue clear
```

### Audio Processing

```bash
# Play with EQ preset
xpuLoad song.flac | xpuIn2Wav | xpuProcess --eq rock | xpuPlay

# Adjust volume
xpuLoad song.flac | xpuIn2Wav | xpuProcess --volume 0.5 | xpuPlay

# Fade in
xpuLoad song.flac | xpuIn2Wav | xpuProcess --fade-in 2000 | xpuPlay
```

### Metadata

```bash
# Get file information
xpuLoad --info song.flac

# Output:
# {
#   "title": "Bohemian Rhapsody",
#   "artist": "Queen",
#   "album": "A Night at the Opera",
#   "duration": 354.5,
#   ...
# }
```

---

## Configuration

### Default Configuration File

Location: `~/.config/xpu/xpuSetting.conf`

```toml
[playback]
target_sample_rate = 96000
target_bit_depth = 32
channels = 2
default_device = "auto"

[fft_cache]
enabled = true
cache_dir = "~/.cache/xpu/fft"
fft_size = 2048
```

### Override Settings

```bash
# Play with specific sample rate
xpuLoad song.flac | xpuIn2Wav --rate 48000 | xpuPlay

# Use specific audio device
xpuPlay --device "hw:1,0"

# Disable FFT cache
xpuLoad song.flac | xpuIn2Wav --no-fft-cache | xpuPlay
```

---

## Testing Your Installation

```bash
# Run tests
cd build
ctest --output-on-failure

# Test audio pipeline
echo "Testing: xpuLoad | xpuIn2Wav | xpuPlay"
xpuLoad tests/data/test.flac | xpuIn2Wav | xpuPlay
```

---

## Troubleshooting

### No Sound Output

1. Check audio device:
   ```bash
   xpuPlay --list-devices
   ```

2. Verify file format:
   ```bash
   xpuLoad --verify song.flac
   ```

3. Check logs:
   ```bash
   tail -f ~/.config/xpu/xpu.log
   ```

### Build Errors

**Missing FFmpeg**:
```bash
# Ubuntu/Debian
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev

# macOS
brew install ffmpeg
```

**Missing PortAudio**:
```bash
# Ubuntu/Debian
sudo apt-get install libportaudio2-dev

# macOS
brew install portaudio
```

### Permission Denied

```bash
# Make binaries executable
chmod +x build/bin/xpu*
```

---

## Next Steps

- **Full Documentation**: See `docs/` directory
- **API Reference**: `contracts/rest-api.yaml`
- **MCP Integration**: `contracts/mcp-tools.yaml`
- **Contributing**: `CONTRIBUTING.md`

---

## Getting Help

- GitHub Issues: https://github.com/your-org/xpu/issues
- Documentation: https://xpu.example.com/docs
- Community: https://discord.gg/xpu

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│                    XPU Quick Reference                       │
├─────────────────────────────────────────────────────────────┤
│ Play single:    xpuLoad file.flac | xpuIn2Wav | xpuPlay    │
│ Add to queue:   xpuQueue add file.flac                      │
│ Play queue:     xpuQueue play                               │
│ Pause:          xpuQueue pause                              │
│ Next track:     xpuQueue next                               │
│ List queue:     xpuQueue list                               │
│ Apply EQ:       xpuProcess --eq rock                        │
│ Set volume:     xpuProcess --volume 0.8                     │
│ Get info:       xpuLoad --info file.flac                    │
└─────────────────────────────────────────────────────────────┘
```
