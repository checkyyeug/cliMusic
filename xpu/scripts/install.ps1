# XPU Installation Script for Windows
# Supports: vcpkg, Conan

param(
    [string]$PackageManager = "vcpkg",
    [switch]$SkipTests = $false
)

$ErrorActionPreference = "Stop"

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "XPU Installation Script for Windows" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Warning: Not running as Administrator. Some operations may require elevated privileges." -ForegroundColor Yellow
}

# Detect package manager
Write-Host "Package manager: $PackageManager" -ForegroundColor Green

# Install dependencies
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Installing dependencies..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

switch ($PackageManager) {
    "vcpkg" {
        # Check if vcpkg is installed
        if (-not (Get-Command vcpkg -ErrorAction SilentlyContinue)) {
            Write-Host "vcpkg not found. Installing..." -ForegroundColor Yellow

            # Clone vcpkg
            $vcpkgDir = "$env:USERPROFILE\vcpkg"
            if (Test-Path $vcpkgDir) {
                Write-Host "vcpkg directory already exists at $vcpkgDir" -ForegroundColor Yellow
            } else {
                git clone https://github.com/Microsoft/vcpkg.git $vcpkgDir
                & "$vcpkgDir\bootstrap-vcpkg.bat"
            }

            # Add vcpkg to PATH
            $env:PATH += ";$vcpkgDir"
        }

        # Install dependencies via vcpkg
        Write-Host "Installing dependencies via vcpkg..." -ForegroundColor Green
        vcpkg install ffmpeg:x64-windows
        vcpkg install portaudio:x64-windows
        vcpkg install fftw3:x64-windows
        vcpkg install libsamplerate:x64-windows
        vcpkg install nlohmann-json:x64-windows
        vcpkg install spdlog:x64-windows
        vcpkg install gtest:x64-windows

        # Integrate with CMake
        vcpkg integrate install
    }

    "conan" {
        # Check if conan is installed
        if (-not (Get-Command conan -ErrorAction SilentlyContinue)) {
            Write-Host "Conan not found. Installing..." -ForegroundColor Yellow
            pip install conan
        }

        # Create conan profile
        conan profile new --detect default

        # Install dependencies via conan
        Write-Host "Installing dependencies via conan..." -ForegroundColor Green
        conan install ffmpeg/4.4@
        conan install portaudio/19.7@
        conan install fftw3/3.3.8@
        conan install libsamplerate/0.1.9@
        conan install nlohmann-json/3.9.1@
        conan install spdlog/1.9.2@
        conan install gtest/1.11.0@
    }

    default {
        Write-Host "Unsupported package manager: $PackageManager" -ForegroundColor Red
        exit 1
    }
}

# Install build tools
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Installing build tools..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

# Install Visual Studio Build Tools if not present
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Host "Visual Studio Build Tools not found. Please install from:" -ForegroundColor Yellow
    Write-Host "https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
    Write-Host "Select 'Desktop development with C++' workload" -ForegroundColor Yellow
    exit 1
}

# Install CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake not found. Installing..." -ForegroundColor Yellow

    # Download CMake installer
    $cmakeInstaller = "$env:TEMP\cmake-installer.exe"
    Invoke-WebRequest -Uri "https://github.com/Kitware/CMake/releases/download/v3.25.1/cmake-3.25.1-windows-x86_64.exe" -OutFile $cmakeInstaller

    # Run installer
    Start-Process -FilePath $cmakeInstaller -ArgumentList "/S" -Wait

    # Add CMake to PATH
    $env:PATH += ";C:\Program Files\CMake\bin"
}

# Build XPU
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Building XPU..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$xpuDir = Split-Path -Parent $scriptDir

Set-Location $xpuDir

# Create build directory
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
}
New-Item -ItemType Directory -Path "build" | Out-Null
Set-Location "build"

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Green
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Build
Write-Host "Building..." -ForegroundColor Green
cmake --build . --config Release -j

# Run tests
if (-not $SkipTests) {
    Write-Host "======================================" -ForegroundColor Cyan
    Write-Host "Running tests..." -ForegroundColor Cyan
    Write-Host "======================================" -ForegroundColor Cyan

    $testResult = ctest --output-on-failure --build-config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Tests failed. Installation aborted." -ForegroundColor Red
        exit 1
    }
}

# Install
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Installing XPU..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

cmake --install . --config Release

# Verify installation
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "Verifying installation..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

$modules = @("xpuLoad", "xpuIn2Wav", "xpuPlay", "xpuQueue", "xpuProcess", "xpuDaemon")

foreach ($module in $modules) {
    if (Get-Command $module -ErrorAction SilentlyContinue) {
        Write-Host "$module installed successfully" -ForegroundColor Green
    } else {
        Write-Host "$module not found in PATH" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Green
Write-Host "XPU installation completed successfully!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Configuration file: %APPDATA%\xpu\xpuSetting.conf"
Write-Host "Cache directory: %APPDATA%\xpu\cache"
Write-Host ""
Write-Host "Usage:"
Write-Host "  xpuLoad <file> | xpuIn2Wav | xpuPlay"
Write-Host "  xpuQueue add <file>"
Write-Host "  xpuDaemon --daemon"
