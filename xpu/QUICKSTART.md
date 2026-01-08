# XPU Quick Start Guide

## Installation

### Prerequisites

- **CMake** 3.15 or higher
- **C++17** compatible compiler
- **FFmpeg** libraries
- **PortAudio** (for audio playback)
- **FFTW3** (for FFT computation)
- **libsamplerate** (for audio resampling)
- **nlohmann/json** (for configuration)
- **spdlog** (for logging)

### Linux (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev
sudo apt-get install libportaudio2 libportaudio-dev
sudo apt-get install libfftw3-dev
sudo apt-get install libsamplerate0-dev
```

Run the installation script:
```bash
cd xpu
./scripts/install.sh
```

### macOS

```bash
brew install cmake
brew install ffmpeg
brew install portaudio
brew install fftw
brew install libsamplerate
```

Run the installation script:
```bash
cd xpu
./scripts/install.sh
```

### Windows

Install dependencies via vcpkg:
```bash
vcpkg install ffmpeg portaudio fftw3 libsamplerate
```

Run the installation script:
```powershell
cd xpu
.\scripts\install.ps1
```

### Build from Source

```bash
cd xpu
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Basic Usage

### 1. Load Audio File

```bash
xpuLoad music.flac
```

This will:
- Parse the audio file
- Extract metadata (title, artist, album, etc.)
- Output metadata in JSON format to stdout

### 2. Convert and Compute FFT

```bash
xpuIn2Wav music.flac
```

This will:
- Convert audio to standard format (48kHz, stereo, 32-bit float)
- Compute FFT with caching
- Cache results for faster subsequent access

### 3. Play Audio

```bash
xpuPlay music.flac
```

Options:
- `--device <name>` - Specify output device
- `--buffer-size <frames>` - Set buffer size (default: 2048)
- `--list-devices` - List available devices

### 4. Manage Queue

```bash
# Add tracks to queue
xpuQueue add music.flac
xpuQueue add *.flac

# List queue
xpuQueue list

# Play queue
xpuQueue play

# Next/Previous track
xpuQueue next
xpuQueue previous

# Clear queue
xpuQueue clear
```

### 5. Apply DSP Effects

```bash
# Adjust volume (0-200%)
xpuProcess music.flac --volume 150

# Fade in/out
xpuProcess music.flac --fade-in 1000
xpuProcess music.flac --fade-out 2000

# Apply EQ preset
xpuProcess music.flac --eq rock

# Custom EQ
xpuProcess music.flac --eq-bass 5 --eq-mid -2 --eq-treble 3
```

### 6. Run as Daemon

```bash
# Start daemon
xpuDaemon --daemon

# Run in foreground
xpuDaemon --foreground

# Get status
xpuDaemon --status

# Stop daemon
xpuDaemon --stop
```

## Configuration

Configuration file: `~/.config/xpu/xpuSetting.conf` (Linux/macOS) or `%APPDATA%\xpu\xpuSetting.conf` (Windows)

### Example Configuration

```ini
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
overlap_ratio = 0.5
window_function = hann

[queue]
persistent = true
queue_file = ~/.config/xpu/queue.json
max_items = 10000

[logging]
level = info
file = /var/log/xpu/xpu.log
rotation = true
max_size_mb = 10
max_files = 3

[audio_processing]
resample_quality = sinc_best
dithering = true

[dsp]
volume = 1.0
fade_in_ms = 0
fade_out_ms = 0
eq_preset = flat
eq_low = 0
eq_mid = 0
eq_high = 0
```

## Troubleshooting

### Audio Not Playing

1. Check if device is available:
   ```bash
   xpuPlay --list-devices
   ```

2. Try default device:
   ```bash
   xpuPlay --device default music.flac
   ```

3. Increase buffer size:
   ```bash
   xpuPlay --buffer-size 4096 music.flac
   ```

### High Latency

1. Reduce buffer size:
   ```bash
   xpuPlay --buffer-size 1024 music.flac
   ```

2. Use exclusive mode (Windows):
   - Configure in `xpuSetting.conf`: `latency_ms = 20`

### FFT Cache Issues

1. Clear cache:
   ```bash
   rm -rf ~/.cache/xpu/fft/
   ```

2. Disable cache:
   ```ini
   [fft_cache]
   enabled = false
   ```

### Permission Errors (Linux/macOS)

```bash
# Fix permissions
sudo chmod +x /usr/local/bin/xpu*
```

## Getting Help

- View help: `xpuLoad --help`
- Check logs: `tail -f /var/log/xpu/xpu.log`
- Report issues: GitHub Issues

## Next Steps

- Read [USER_GUIDE.md](USER_GUIDE.md) for detailed documentation
- Read [ARCHITECTURE.md](ARCHITECTURE.md) for system architecture
- Read [API.md](API.md) for API reference
