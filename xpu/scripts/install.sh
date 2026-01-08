#!/bin/bash

# XPU Installation Script for Linux/macOS
# Supports: Ubuntu/Debian (apt), Fedora (dnf), Arch (pacman), macOS (Homebrew)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "XPU Installation Script"
echo "======================================"

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    echo -e "${RED}Unsupported OS: $OSTYPE${NC}"
    exit 1
fi

echo "Detected OS: $OS"

# Detect package manager
if [ "$OS" == "linux" ]; then
    if command -v apt &> /dev/null; then
        PKG_MANAGER="apt"
    elif command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
    elif command -v pacman &> /dev/null; then
        PKG_MANAGER="pacman"
    else
        echo -e "${RED}No supported package manager found${NC}"
        exit 1
    fi
elif [ "$OS" == "macos" ]; then
    if command -v brew &> /dev/null; then
        PKG_MANAGER="brew"
    else
        echo -e "${YELLOW}Homebrew not found. Installing...${NC}"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        PKG_MANAGER="brew"
    fi
fi

echo "Package manager: $PKG_MANAGER"

# Install dependencies
echo "======================================"
echo "Installing dependencies..."
echo "======================================"

install_dependencies() {
    case $PKG_MANAGER in
        apt)
            sudo apt update
            sudo apt install -y \
                build-essential \
                cmake \
                pkg-config \
                libavcodec-dev \
                libavformat-dev \
                libavutil-dev \
                libswresample-dev \
                libportaudio2 \
                libportaudio-dev \
                libfftw3-dev \
                libsamplerate0-dev \
                nlohmann-json3-dev \
                libspdlog-dev \
                clang \
                clang-tidy \
                clang-format \
                cppcheck \
                git
            ;;
        dnf)
            sudo dnf install -y \
                gcc-c++ \
                cmake \
                pkg-config \
                ffmpeg-devel \
                portaudio-devel \
                fftw-devel \
                libsamplerate-devel \
                json-devel \
                spdlog-devel \
                clang \
                clang-tools-extra \
                cppcheck \
                git
            ;;
        pacman)
            sudo pacman -S --noconfirm \
                gcc \
                cmake \
                pkg-config \
                ffmpeg \
                portaudio \
                fftw \
                libsamplerate \
                nlohmann-json \
                spdlog \
                clang \
                clang-tools-extra \
                cppcheck \
                git
            ;;
        brew)
            brew install \
                cmake \
                pkg-config \
                ffmpeg \
                portaudio \
                fftw \
                libsamplerate \
                nlohmann-json \
                spdlog \
                clang-format \
                cppcheck \
                git
            ;;
    esac
}

install_dependencies

# Install GoogleTest
echo "======================================"
echo "Installing GoogleTest..."
echo "======================================"

if [ ! -d "/usr/local/include/gtest" ] && [ ! -d "/usr/include/gtest" ]; then
    git clone https://github.com/google/googletest.git /tmp/googletest
    cd /tmp/googletest
    mkdir build && cd build
    cmake ..
    sudo make install
    cd /tmp
    rm -rf /tmp/googletest
fi

# Build XPU
echo "======================================"
echo "Building XPU..."
echo "======================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XPU_DIR="$(dirname "$SCRIPT_DIR")"

cd "$XPU_DIR"
mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Run tests
echo "======================================"
echo "Running tests..."
echo "======================================"

ctest --output-on-failure || {
    echo -e "${RED}Tests failed. Installation aborted.${NC}"
    exit 1
}

# Install
echo "======================================"
echo "Installing XPU..."
echo "======================================"

sudo make install

# Verify installation
echo "======================================"
echo "Verifying installation..."
echo "======================================"

if command -v xpuLoad &> /dev/null; then
    echo -e "${GREEN}xpuLoad installed successfully${NC}"
    xpuLoad --version
else
    echo -e "${YELLOW}xpuLoad not found in PATH${NC}"
fi

if command -v xpuIn2Wav &> /dev/null; then
    echo -e "${GREEN}xpuIn2Wav installed successfully${NC}"
else
    echo -e "${YELLOW}xpuIn2Wav not found in PATH${NC}"
fi

if command -v xpuPlay &> /dev/null; then
    echo -e "${GREEN}xpuPlay installed successfully${NC}"
else
    echo -e "${YELLOW}xpuPlay not found in PATH${NC}"
fi

if command -v xpuQueue &> /dev/null; then
    echo -e "${GREEN}xpuQueue installed successfully${NC}"
else
    echo -e "${YELLOW}xpuQueue not found in PATH${NC}"
fi

if command -v xpuProcess &> /dev/null; then
    echo -e "${GREEN}xpuProcess installed successfully${NC}"
else
    echo -e "${YELLOW}xpuProcess not found in PATH${NC}"
fi

if command -v xpuDaemon &> /dev/null; then
    echo -e "${GREEN}xpuDaemon installed successfully${NC}"
else
    echo -e "${YELLOW}xpuDaemon not found in PATH${NC}"
fi

echo ""
echo -e "${GREEN}======================================"
echo "XPU installation completed successfully!"
echo "======================================${NC}"
echo ""
echo "Configuration file: ~/.config/xpu/xpuSetting.conf"
echo "Cache directory: $(xpu::utils::PlatformUtils::getCacheDirectory)"
echo ""
echo "Usage:"
echo "  xpuLoad <file> | xpuIn2Wav | xpuPlay"
echo "  xpuQueue add <file>"
echo "  xpuDaemon --daemon"
