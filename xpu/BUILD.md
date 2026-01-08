# XPU Build Instructions

## Prerequisites

### Required Tools

- **CMake** 3.15 or higher
- **C++17** compatible compiler:
  - GCC 7.0+ (Linux)
  - Clang 5.0+ (macOS/Linux)
  - MSVC 2017+ (Windows)

### Required Libraries

#### Core Dependencies

1. **FFmpeg** 4.0+
   - `libavcodec`
   - `libavformat`
   - `libavutil`
   - `libswresample`

2. **PortAudio** 19.0+
   - Cross-platform audio I/O

3. **FFTW3** 3.3+
   - FFT computation

4. **libsamplerate** 0.1+
   - Audio resampling

#### Build Dependencies

5. **nlohmann/json** 3.10+
   - JSON parsing (header-only)

6. **spdlog** 1.8+
   - Logging (header-only)

7. **OpenSSL** 1.1+
   - SHA256 hashing

8. **GoogleTest** 1.10+
   - Testing framework

## Platform-Specific Setup

### Linux (Ubuntu/Debian)

```bash
# Install build tools
sudo apt-get update
sudo apt-get install build-essential cmake git

# Install dependencies
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev
sudo apt-get install libportaudio2 libportaudio-dev
sudo apt-get install libfftw3-dev
sudo apt-get install libsamplerate0-dev
sudo apt-get install libssl-dev
sudo apt-get install libgtest-dev
```

### Linux (Fedora/RHEL)

```bash
# Install build tools
sudo dnf install gcc-c++ cmake git

# Install dependencies
sudo dnf install ffmpeg-devel
sudo dnf install portaudio-devel
sudo dnf install fftw-devel
sudo dnf install libsamplerate-devel
sudo dnf install openssl-devel
sudo dnf install gtest-devel
```

### Linux (Arch)

```bash
# Install build tools
sudo pacman -S base-devel cmake git

# Install dependencies
sudo pacman -S ffmpeg
sudo pacman -S portaudio
sudo pacman -S fftw
sudo pacman -S libsamplerate
sudo pacman -S openssl
sudo pacman -S gtest
```

### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake
brew install ffmpeg
brew install portaudio
brew install fftw
brew install libsamplerate
brew install openssl
brew install googletest
```

### Windows

#### Using vcpkg

```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
.\vcpkg\bootstrap-vcpkg.bat

# Add to PATH
set PATH=%PATH%;C:\vcpkg

# Install dependencies
vcpkg install ffmpeg[portable]
vcpkg install portaudio
vcpkg install fftw3
vcpkg install libsamplerate
vcpkg install openssl
vcpkg install gtest
```

#### Using MSYS2

```bash
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-ffmpeg
pacman -S mingw-w64-x86_64-portaudio
pacman -S mingw-w64-x86_64-fftw
pacman -S mingw-w64-x86_64-libsamplerate
```

## Building XPU

### Clone Repository

```bash
git clone https://github.com/yourusername/xpu.git
cd xpu
```

### Configure Build

```bash
mkdir build
cd build
```

#### Linux/macOS

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTS=ON
```

#### Windows (vcpkg)

```powershell
cmake .. `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release `
  -DENABLE_TESTS=ON
```

### Build

```bash
# Linux/macOS
make -j$(nproc)

# Windows (MSVC)
cmake --build . --config Release -j

# Windows (MinGW)
mingw32-make -j
```

### Install

```bash
# Linux/macOS
sudo make install

# Windows (vcpkg)
cmake --install . --config Release
```

## Build Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | Build type (Debug/Release/RelWithDebInfo) |
| `ENABLE_TESTS` | `ON` | Build tests |
| `ENABLE_BENCHMARKS` | `ON` | Build benchmarks |
| `INSTALL_PREFIX` | `/usr/local` | Installation prefix |

### Example: Debug Build

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TESTS=ON

make -j$(nproc)
```

### Example: Custom Installation Prefix

```bash
cmake .. \
  -DCMAKE_INSTALL_PREFIX=/opt/xpu \
  -DENABLE_TESTS=ON

make -j$(nproc)
sudo make install
```

## Running Tests

### Build Tests

```bash
cd build
cmake .. -DENABLE_TESTS=ON
make -j$(nproc)
```

### Run All Tests

```bash
ctest --output-on-failure
```

### Run Specific Tests

```bash
# Unit tests only
ctest -R unit --output-on-failure

# Integration tests only
ctest -R integration --output-on-failure

# Specific test suite
ctest -R test_FFTEngine --output-on-failure
```

### Run with Verbose Output

```bash
ctest --verbose --output-on-failure
```

## Verification

### Check Installation

```bash
# Check if binaries are installed
which xpuLoad
which xpuIn2Wav
which xpuPlay
which xpuQueue
which xpuProcess
which xpuDaemon

# Check version
xpuLoad --version
```

### Test Basic Functionality

```bash
# List devices
xpuPlay --list-devices

# Load audio file
xpuLoad test_audio.flac

# Play audio
xpuPlay test_audio.flac
```

## Troubleshooting

### FFmpeg Not Found

**Error**: `Could not find FFmpeg`

**Solution**:
```bash
# Set FFmpeg path explicitly
cmake .. \
  -DFFMPEG_ROOT=/path/to/ffmpeg \
  -DFFMPEG_INCLUDE_DIRS=/path/to/ffmpeg/include \
  -DFFMPEG_LIBRARIES=/path/to/ffmpeg/lib/libavcodec.so
```

### PortAudio Not Found

**Error**: `Could not find PortAudio`

**Solution**:
```bash
# Set PortAudio path explicitly
cmake .. \
  -DPORTAUDIO_ROOT=/path/to/portaudio \
  -DPORTAUDIO_INCLUDE_DIRS=/path/to/portaudio/include \
  -DPORTAUDIO_LIBRARIES=/path/to/portaudio/lib/libportaudio.so
```

### C++17 Not Supported

**Error**: `C++17 is required`

**Solution**:
```bash
# Specify C++ standard
cmake .. \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON
```

### Windows Build Errors

**Error**: `RC1015: cannot open include file 'afxres.h'`

**Solution**:
- Install Windows SDK
- Or add to CMake: `-DCMAKE_WIN32_EXECUTABLE=OFF`

### macOS Build Errors

**Error**: `'atomic' file not found`

**Solution**:
```bash
# Link with atomic library
cmake .. \
  -DCMAKE_CXX_FLAGS="-latomic"
```

## Advanced Build Options

### Static Linking

```bash
cmake .. \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_EXE_LINKER_FLAGS="-static"
```

### Sanitizer Builds

```bash
# Address sanitizer
cmake .. \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"

# Thread sanitizer
cmake .. \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

### Cross-Compilation

#### Linux to Windows (MinGW)

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=mingw-w64.cmake \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
```

#### Linux to macOS

```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
```

## Performance Optimization

### Compiler-Specific Optimizations

```bash
# GCC/Clang
cmake .. \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"

# MSVC
cmake .. \
  -DCMAKE_CXX_FLAGS="/O2 /arch:AVX2"
```

### Link-Time Optimization (LTO)

```bash
cmake .. \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

## Packaging

### Create tarball

```bash
make package
```

### Create DEB package (Debian/Ubuntu)

```bash
cpack -G DEB
```

### Create RPM package (Fedora/RHEL)

```bash
cpack -G RPM
```

### Create DMG (macOS)

```bash
cpack -G DragNDrop
```

## Clean Build

```bash
# From build directory
make clean

# Or remove entire build directory
cd ..
rm -rf build
mkdir build
cd build
cmake ..
```

## Next Steps

- Read [QUICKSTART.md](QUICKSTART.md) for usage instructions
- Read [ARCHITECTURE.md](ARCHITECTURE.md) for system architecture
- Read [API.md](API.md) for API reference
