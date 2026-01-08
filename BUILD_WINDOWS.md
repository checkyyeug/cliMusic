# Windows 构建指南

## 前置要求

### 1. 安装 Visual Studio
- Visual Studio 2022 或更高版本
- 安装 "使用 C++ 的桌面开发" 工作负载

### 2. 安装 CMake
- 从 https://cmake.org/download/ 下载并安装
- 版本要求: 3.15 或更高

### 3. 安装依赖库

#### 方法 A: 使用 vcpkg (推荐)

1. 安装 vcpkg:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

2. 安装依赖:
```bash
.\vcpkg install ffmpeg:x64-windows
.\vcpkg install fftw3:x64-windows
.\vcpkg install libsamplerate:x64-windows
.\vcpkg install portaudio:x64-windows
.\vcpkg install spdlog:x64-windows
.\vcpkg install nlohmann-json:x64-windows
```

#### 方法 B: 手动安装

1. **FFmpeg**:
   - 下载: https://github.com/BtbN/FFmpeg-Builds/releases
   - 解压到 `C:\ffmpeg`
   - 设置环境变量: `FFMPEG_DIR=C:\ffmpeg`

2. **FFTW3**:
   - 下载: http://www.fftw.org/install/windows.html
   - 解压到 `C:\fftw`
   - 设置环境变量: `FFTW3_DIR=C:\fftw`

3. **libsamplerate**:
   - 下载: http://www.mega-nerd.com/libsamplerate/
   - 使用 vcpkg 或自行编译

4. **PortAudio**:
   - 下载: http://www.portaudio.com/download.html
   - 使用 vcpkg 或自行编译

## 构建步骤

### 使用 vcpkg:

```bash
# 1. 克隆项目
git clone <repository-url>
cd cliMusic/xpu

# 2. 创建构建目录
mkdir build
cd build

# 3. 配置项目 (指定 vcpkg 工具链)
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DBUILD_TESTING=ON

# 4. 构建
cmake --build . --config Release

# 5. 运行测试
ctest --output-on-failure

# 6. 安装 (可选)
cmake --install . --config Release
```

### 不使用 vcpkg (需要手动设置路径):

```bash
# 1. 配置项目 (手动指定依赖路径)
cmake .. ^
    -DFFMPEG_INCLUDE_DIRS=C:\ffmpeg\include ^
    -DFFMPEG_LIBRARIES=C:\ffmpeg\lib\avcodec.lib;C:\ffmpeg\lib\avformat.lib;C:\ffmpeg\lib\avutil.lib;C:\ffmpeg\lib\swresample.lib ^
    -DFFTW3_INCLUDE_DIRS=C:\fftw\include ^
    -DFFTW3_LIBRARIES=C:\fftw\lib\fftw3.lib ^
    -DBUILD_TESTING=ON

# 2. 构建
cmake --build . --config Release

# 3. 运行测试
ctest --output-on-failure -C Release
```

## 运行程序

构建完成后，可执行文件位于:
```
build\bin\Release\
```

可用模块:
- `xpuLoad.exe` - 音频文件加载器
- `xpuIn2Wav.exe` - 格式转换器
- `xpuPlay.exe` - 音频播放器
- `xpuQueue.exe` - 队列管理
- `xpuProcess.exe` - DSP 处理
- `xpuDaemon.exe` - 守护进程

## 常见问题

### 问题 1: 找不到 PortAudio
**解决方案**: 使用 vcpkg 安装: `vcpkg install portaudio:x64-windows`

### 问题 2: 找不到 FFmpeg
**解决方案**:
- 使用 vcpkg: `vcpkg install ffmpeg:x64-windows`
- 或手动下载并设置 `FFMPEG_INCLUDE_DIRS` 和 `FFMPEG_LIBRARIES`

### 问题 3: 链接错误
**解决方案**:
- 确保所有依赖库都是相同架构 (x64)
- 检查库路径是否正确
- 确保 CMAKE_BUILD_TYPE 设置正确 (Debug/Release)

### 问题 4: CMake 版本过低
**解决方案**: 升级 CMake 到 3.15 或更高版本

## 开发环境设置

### Visual Studio Code
1. 安装 "C/C++" 扩展
2. 安装 "CMake Tools" 扩展
3. 打开 xpu 文件夹
4. 选择 CMake Kit (Visual Studio Community 2022 Release - x64)
5. 按 F5 开始调试

### Visual Studio 2022
1. 打开 `build\xpu.sln`
2. 设置启动项目
3. 按 F5 开始调试

## 性能优化

Release 模式已启用以下优化:
- `/O2` - 优化速度
- `/GL` - 全局优化
- `/LTCG` - 链接时代码生成

## 调试

Debug 模式构建:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

启用详细输出:
```bash
cmake .. --trace-expand
```

## 清理构建

```bash
# 清理构建文件
cmake --build . --target clean

# 或完全删除构建目录
cd ..
rm -rf build
mkdir build
cd build
```

## 下一步

构建成功后，请参考:
- [QUICKSTART.md](QUICKSTART.md) - 快速入门指南
- [ARCHITECTURE.md](ARCHITECTURE.md) - 架构文档
- [TASK.md](TASK.md) - 任务清单
