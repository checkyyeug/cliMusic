@echo off
REM XPU Windows Build Script
REM This script helps build the XPU project on Windows

echo ====================================
echo XPU Project Windows Build Script
echo ====================================
echo.

REM Check if vcpkg toolchain file exists
set VCPKG_TOOLCHAIN=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

if not exist "%VCPKG_TOOLCHAIN%" (
    echo ERROR: vcpkg toolchain not found at %VCPKG_TOOLCHAIN%
    echo.
    echo Please install vcpkg and required dependencies:
    echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    echo   cd C:\vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo   .\vcpkg integrate install
    echo   .\vcpkg install ffmpeg:x64-windows fftw3:x64-windows libsamplerate:x64-windows portaudio:x64-windows
    echo.
    pause
    exit /b 1
)

echo Found vcpkg toolchain at: %VCPKG_TOOLCHAIN%
echo.

REM Create build directory if it doesn't exist
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

echo.
echo ====================================
echo Configuring project with CMake...
echo ====================================
echo.

cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Please ensure all dependencies are installed via vcpkg:
    echo   vcpkg install ffmpeg:x64-windows
    echo   vcpkg install fftw3:x64-windows
    echo   vcpkg install libsamplerate:x64-windows
    echo   vcpkg install portaudio:x64-windows
    echo   vcpkg install spdlog:x64-windows
    pause
    exit /b 1
)

echo.
echo ====================================
echo Building project...
echo ====================================
echo.

cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ====================================
echo Build completed successfully!
echo ====================================
echo.
echo Output binaries are in: build\bin\Release\
echo.
echo Available executables:
echo   - xpuLoad.exe
echo   - xpuIn2Wav.exe
echo   - xpuPlay.exe
echo   - xpuQueue.exe
echo   - xpuProcess.exe
echo   - xpuDaemon.exe
echo.
echo To run tests:
echo   cd build
echo   ctest --output-on-failure -C Release
echo.

pause
