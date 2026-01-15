# xpuLoad 使用说明书

## 目录
- [概述](#概述)
- [设计理念](#设计理念)
- [功能特性](#功能特性)
- [安装与构建](#安装与构建)
- [命令行语法](#命令行语法)
- [详细使用方法](#详细使用方法)
- [输出格式详解](#输出格式详解)
- [管道通信](#管道通信)
- [支持格式](#支持格式)
- [高级选项](#高级选项)
- [性能特性](#性能特性)
- [错误处理](#错误处理)
- [示例用法](#示例用法)
- [最佳实践](#最佳实践)
- [故障排除](#故障排除)
- [技术细节](#技术细节)

## 概述

xpuLoad是XPU（Cross-Platform Professional Audio Playback System）音频处理系统的核心模块之一，遵循Unix哲学的模块化设计理念。作为专业的音频文件解析器，它专注于高效、忠实地读取各种音频格式并提取完整的元数据信息。

### 核心职责
- **音频文件解析**：支持多种无损和有损音频格式
- **元数据提取**：提取完整的音频文件信息
- **标准化输出**：输出格式化的JSON元数据和标准化PCM数据
- **管道通信**：为后续处理模块提供标准化的数据接口

## 设计理念

### Unix哲学遵循
xpuLoad严格遵循Unix哲学的核心原则：

1. **单一职责**：专注于文件解析和元数据提取，不进行格式转换
2. **标准接口**：通过标准输出提供JSON格式的元数据和PCM数据
3. **模块化设计**：可与其他xpu模块通过管道组合使用
4. **专业化**：专注于高质量的音频解码和元数据处理

### 架构设计原则
- **质量优先**：保持原始音频质量，不进行不必要的处理
- **性能优化**：高效的解码流程，支持高分辨率音频
- **跨平台兼容**：支持Windows、macOS、Linux三大平台
- **扩展性强**：为未来功能扩展预留接口

### 设计哲学
```
"xpuLoad应该做一件事，并且要做好"
- 专注音频文件解析
- 输出标准化数据
- 为管道通信优化
- 保持原始质量
```

## 功能特性

### 核心功能
- ✅ **多格式支持**：FLAC、WAV、ALAC、DSD、MP3、AAC、OGG、OPUS
- ✅ **高分辨率支持**：最高支持768kHz采样率，32位深度
- ✅ **DSD格式支持**：原生支持DSF和DSDIFF格式
- ✅ **元数据提取**：完整的ID3、FLAC、Vorbis标签支持
- ✅ **标准化输出**：统一的JSON + PCM数据格式
- ✅ **管道友好**：专为Unix管道设计的输出格式

### 高级特性
- ✅ **智能格式检测**：自动识别音频文件格式
- ✅ **错误恢复**：健壮的错误处理和恢复机制
- ✅ **详细日志**：结构化日志记录，便于调试
- ✅ **内存优化**：高效的内存使用，适合大文件处理
- ✅ **多线程解码**：利用多核CPU提升解码性能

### 输出特性
- ✅ **JSON元数据**：结构化的音频文件信息
- ✅ **PCM数据**：32位浮点立体声交错格式
- ✅ **大小头**：8字节数据大小头，便于流处理
- ✅ **原始采样率保持**：默认保持原始采样率，避免质量损失

## 安装与构建

### 系统要求
- **操作系统**：Windows 10+、macOS 10.14+、Linux（主流发行版）
- **编译器**：C++17兼容编译器
  - GCC 7+
  - Clang 5+
  - MSVC 2017+
- **CMake**：3.15或更高版本

### 依赖库
- **FFmpeg 4.0+**：音频解码核心
- **libsamplerate**：DSD解码支持
- **spdlog**：日志记录
- **nlohmann/json**：JSON处理

### 构建步骤

#### Linux (Ubuntu/Debian)
```bash
# 安装依赖
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev
sudo apt-get install libsamplerate0-dev

# 构建项目
cd xpu
mkdir build && cd build
cmake ..
make -j$(nproc)

# 安装
sudo make install
```

#### macOS
```bash
# 使用Homebrew安装依赖
brew install cmake ffmpeg libsamplerate

# 构建项目
cd xpu
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

# 安装
sudo make install
```

#### Windows
```powershell
# 使用vcpkg安装依赖
vcpkg install ffmpeg libsamplerate

# 构建项目
cd xpu
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# 安装
cmake --build . --target install --config Release
```

## 命令行语法

### 基本语法
```bash
xpuLoad [选项] <输入文件>
```

### 选项说明

| 选项 | 长选项 | 说明 | 默认值 |
|------|--------|------|--------|
| `-h` | `--help` | 显示帮助信息 | - |
| `-v` | `--version` | 显示版本信息 | - |
| `-V` | `--verbose` | 启用详细输出 | false |
| `-m` | `--metadata` | 仅输出元数据（JSON格式） | false |
| `-d` | `--data` | 仅输出PCM数据（无元数据） | false |
| `-r <rate>` | `--sample-rate <rate>` | 目标采样率（Hz） | 0（保持原样） |

### 采样率选项
支持的采样率值：
- `44100` - CD质量
- `48000` - 专业音频标准
- `96000` - 高分辨率音频
- `192000` - 专业录音棚标准
- `384000` - 极高分辨率
- `768000` - 实验室级别（极限支持）

**注意**：设置为0或省略表示保持原始采样率，这是推荐用法。

## 详细使用方法

### 1. 基本用法
```bash
# 解析音频文件并输出完整信息
xpuLoad music.flac

# 解析MP3文件
xpuLoad song.mp3

# 解析DSD文件
xpuLoad track.dsf
```

### 2. 仅获取元数据
```bash
# 仅输出JSON格式的元数据
xpuLoad --metadata music.flac

# 或使用短选项
xpuLoad -m music.flac
```

### 3. 仅获取PCM数据
```bash
# 仅输出PCM音频数据
xpuLoad --data music.flac

# 或使用短选项
xpuLoad -d music.flac
```

### 4. 指定输出采样率
```bash
# 转换为48kHz采样率
xpuLoad --sample-rate 48000 music.flac

# 或使用短选项
xpuLoad -r 48000 music.flac

# 转换为96kHz高分辨率
xpuLoad -r 96000 music.flac
```

### 5. 详细输出模式
```bash
# 启用详细日志输出
xpuLoad --verbose music.flac

# 组合使用
xpuLoad -V -r 48000 music.flac
```

### 6. 批处理模式
```bash
# 处理多个文件
for file in *.flac; do
    echo "Processing: $file"
    xpuLoad --metadata "$file"
done

# 使用find命令
find . -name "*.flac" -exec xpuLoad --metadata {} \;
```

## 输出格式详解

### 完整输出格式
xpuLoad默认输出包含JSON元数据和PCM音频数据：

```
JSON元数据(换行分隔)
8字节数据大小头
PCM音频数据
```

### JSON元数据格式
```json
{
  "success": true,
  "metadata": {
    "file_path": "/path/to/music.flac",
    "format": "FLAC",
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name",
    "year": "2023",
    "genre": "Rock",
    "track_number": 1,
    "duration": 245.67,
    "sample_rate": 48000,
    "original_sample_rate": 48000,
    "bit_depth": 24,
    "original_bit_depth": 24,
    "channels": 2,
    "sample_count": 11796960,
    "bitrate": 1024000,
    "is_lossless": true,
    "is_high_res": false
  }
}
```

### 字段说明

#### 基本信息
- **file_path**：输入文件路径
- **format**：音频格式（FLAC、WAV、MP3等）
- **title**：标题
- **artist**：艺术家
- **album**：专辑
- **year**：年份
- **genre**：流派
- **track_number**：音轨号

#### 技术参数
- **duration**：持续时间（秒）
- **sample_rate**：当前采样率（Hz）
- **original_sample_rate**：原始采样率（Hz）
- **bit_depth**：当前位深度
- **original_bit_depth**：原始位深度
- **channels**：声道数
- **sample_count**：总采样点数
- **bitrate**：比特率（bps）

#### 质量标识
- **is_lossless**：是否为无损格式
- **is_high_res**：是否为高分辨率音频（≥96kHz）

### PCM数据格式

#### 数据头
```cpp
uint64_t data_size;  // 8字节，数据大小
```

#### 音频数据
- **格式**：32位浮点数
- **声道**：立体声交错
- **采样率**：由JSON元数据指定
- **字节序**：小端序

#### 数据结构
```
[左声道采样][右声道采样][左声道采样][右声道采样]...
```

## 管道通信

### 管道用法
xpuLoad专门设计用于Unix/Linux管道：

```bash
# 基本管道：xpuLoad -> xpuIn2Wav
xpuLoad music.flac | xpuIn2Wav

# 完整音频处理管道
xpuLoad music.flac | xpuIn2Wav --rate 96000 | xpuProcess --volume 150 | xpuPlay

# 获取元数据后处理
xpuLoad --metadata music.flac | jq '.metadata.title'
```

### 数据流设计
```
输入文件 → xpuLoad → JSON头 + PCM数据 → 下一模块
```

#### 优势
- **流式处理**：适合大文件，无需加载到内存
- **模块化**：可灵活组合不同处理模块
- **标准化**：统一的接口格式
- **高效性**：避免中间文件创建

### 错误处理
管道中的错误会通过适当的机制传播：

```bash
# 错误会被输出到stderr
xpuLoad nonexistent.flac 2>&1 | xpuIn2Wav
```

## 支持格式

### 无损格式
| 格式 | 扩展名 | 支持程度 | 最大规格 |
|------|--------|----------|----------|
| **FLAC** | .flac | ✅ 完全支持 | 768kHz/32-bit |
| **WAV** | .wav | ✅ 完全支持 | 768kHz/32-bit |
| **ALAC** | .m4a | ✅ 完全支持 | 768kHz/32-bit |
| **DSD** | .dsf | ✅ 完全支持 | DSD512 (22.6MHz) |
| **DSDIFF** | .dff | ✅ 完全支持 | DSD512 (22.6MHz) |

### 有损格式
| 格式 | 扩展名 | 支持程度 | 最大规格 |
|------|--------|----------|----------|
| **MP3** | .mp3 | ✅ 完全支持 | 48kHz/320kbps |
| **AAC** | .m4a | ✅ 完全支持 | 48kHz/256kbps |
| **OGG** | .ogg | ✅ 完全支持 | 48kHz/500kbps |
| **OPUS** | .opus | ✅ 完全支持 | 48kHz/64kbps |

### 特殊支持

#### DSD格式特性
- **DSD64** (2.8MHz)：标准DSD
- **DSD128** (5.6MHz)：Double DSD
- **DSD256** (11.2MHz)：Quad DSD
- **DSD512** (22.6MHz)：Octa DSD
- **自动DSD到PCM转换**：内置高质量转换算法

#### 高分辨率音频
- **768kHz支持**：极限采样率支持
- **32位深度**：浮点和整数格式
- **多声道**：最高支持8声道
- **DXD格式**：352.8kHz/384kHz PCM

## 高级选项

### 1. 采样率转换策略
```bash
# 保持原始采样率（推荐）
xpuLoad music.flac

# 转换到标准采样率
xpuLoad -r 48000 music.flac

# 高分辨率转换
xpuLoad -r 192000 music.flac
```

### 2. 输出模式选择
```bash
# 完整输出（默认）
xpuLoad music.flac

# 仅元数据（用于批量分析）
xpuLoad -m *.flac | jq -r '.metadata | "\(.artist) - \(.title)"'

# 仅音频数据（用于特定处理）
xpuLoad -d music.flac > audio.pcm
```

### 3. 批处理与自动化
```bash
# 批量转换采样率
for file in *.flac; do
    xpuLoad -r 48000 "$file" | xpuIn2Wav -r 48000 "${file%.flac}_48k.wav"
done

# 元数据提取脚本
find . -name "*.flac" -exec xpuLoad -m {} \; > all_metadata.json

# 质量分析
xpuLoad -m music.flac | jq '.metadata | select(.is_high_res == true)'
```

## 性能特性

### 解码性能
- **CD质量** (44.1kHz/16-bit)：>1000x 实时
- **高分辨率** (192kHz/24-bit)：>100x 实时
- **DSD128**：>200x 实时
- **内存使用**：<50MB 典型文件
- **CPU使用**：单核可实时解码

### 并发处理
- **多线程解码**：自动检测CPU核心数
- **异步I/O**：高效的文件读取
- **内存池**：减少内存分配开销
- **缓存优化**：智能的解码缓存

### 格式特性对比
| 格式 | 解码速度 | CPU使用 | 内存使用 | 质量损失 |
|------|----------|---------|----------|----------|
| FLAC | 极快 | 极低 | 低 | 无损 |
| WAV | 极快 | 极低 | 低 | 无损 |
| ALAC | 很快 | 低 | 低 | 无损 |
| DSD | 快 | 中 | 中 | 无损 |
| MP3 | 极快 | 极低 | 极低 | 有损 |

## 错误处理

### 常见错误码
- **文件不存在** (Error 60)：输入文件路径错误
- **不支持格式** (Error 61)：音频格式不被支持
- **文件损坏** (Error 62)：音频文件数据损坏
- **内存不足** (Error 100)：系统资源不足
- **权限拒绝** (Error 61)：文件访问权限问题

### 错误输出格式
```json
{
  "success": false,
  "error_code": 60,
  "message": "File not found",
  "timestamp": "2025-01-08T12:34:56Z",
  "http_status": 404
}
```

### 错误处理示例
```bash
# 检查错误并处理
if ! xpuLoad music.flac; then
    echo "解码失败，尝试其他格式"
    # 错误处理逻辑
fi

# 管道错误处理
xpuLoad music.flac 2>/dev/null | xpuIn2Wav || echo "管道处理失败"
```

## 示例用法

### 1. 基础音乐播放
```bash
# 完整播放流程
xpuLoad music.flac | xpuIn2Wav --rate 48000 | xpuPlay
```

### 2. 元数据分析
```bash
# 获取音乐库信息
xpuLoad --metadata *.flac | jq -r '.metadata | "\(.artist) - \(.album) - \(.title) (\(.duration)s)"'

# 查找高分辨率音频
xpuLoad --metadata *.flac | jq '.metadata | select(.is_high_res == true) | .file_path'

# 统计音乐库
xpuLoad --metadata *.flac | jq 'reduce .[] as $item ({}; .total += 1; .lossless += ($item.is_lossless | if . then 1 else 0 end))'
```

### 3. 格式转换
```bash
# 转换到标准格式
xpuLoad music.flac | xpuIn2Wav -r 48000 -b 24 -c 2 output.wav

# DSD转换
xpuLoad track.dsf | xpuIn2Wav -r 192000 high_res.wav
```

### 4. 批处理脚本
```bash
#!/bin/bash
# 批量处理脚本
for file in "$@"; do
    echo "处理: $file"
    
    # 获取元数据
    xpuLoad --metadata "$file" > "${file%.*}_meta.json"
    
    # 转换格式
    xpuLoad "$file" | xpuIn2Wav "${file%.*}_converted.wav"
    
    echo "完成: $file"
done
```

### 5. 实时处理
```bash
# 实时音频流处理
tail -f audio_stream.log | while read line; do
    file=$(echo "$line" | jq -r '.file')
    xpuLoad "$file" | xpuIn2Wav | xpuPlay &
done
```

## 最佳实践

### 1. 性能优化
- **保持原始采样率**：避免不必要的重采样
- **使用适当格式**：FLAC用于存储，MP3用于播放
- **批量处理**：对多个文件使用脚本处理
- **内存管理**：大文件考虑流式处理

### 2. 质量保证
- **无损优先**：尽量使用无损格式存储
- **高分辨率**：录音源文件保持高规格
- **元数据完整**：确保音乐标签信息完整
- **错误检查**：定期检查音频文件完整性

### 3. 管道使用
- **错误处理**：在管道中添加错误检查
- **进度监控**：大文件处理时添加进度显示
- **资源管理**：及时释放不需要的资源
- **日志记录**：启用详细日志用于调试

### 4. 脚本编写
```bash
# 推荐的脚本结构
#!/bin/bash
set -euo pipefail  # 严格模式

# 错误处理函数
handle_error() {
    echo "错误: $1" >&2
    exit 1
}

# 主处理逻辑
main() {
    local input_file="$1"
    
    # 检查输入文件
    [[ -f "$input_file" ]] || handle_error "文件不存在: $input_file"
    
    # 处理音频文件
    xpuLoad --metadata "$input_file" || handle_error "xpuLoad失败"
    
    # 转换处理
    xpuLoad "$input_file" | xpuIn2Wav "${input_file%.*}_out.wav" || handle_error "转换失败"
}

main "$@"
```

## 故障排除

### 常见问题

#### 1. 编译错误
```bash
# 错误：找不到FFmpeg
# 解决：安装FFmpeg开发库
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev

# 错误：CMake版本过低
# 解决：升级CMake
sudo apt-get install cmake3
```

#### 2. 运行时错误
```bash
# 错误：权限拒绝
# 解决：检查文件权限
chmod 644 music.flac

# 错误：格式不支持
# 解决：检查FFmpeg支持的格式
ffmpeg -formats | grep -i flac
```

#### 3. 性能问题
```bash
# 症状：解码速度慢
# 解决：
# 1. 检查CPU使用率
top -p $(pgrep xpuLoad)

# 2. 使用更多核心
export OMP_NUM_THREADS=$(nproc)

# 3. 检查磁盘I/O
iostat -x 1
```

#### 4. 内存问题
```bash
# 症状：内存使用过高
# 解决：
# 1. 检查大文件
du -sh *.flac

# 2. 限制并发处理
ulimit -v 2097152  # 2GB限制
```

### 调试技巧

#### 1. 详细日志
```bash
# 启用详细输出
xpuLoad --verbose music.flac

# 保存日志到文件
xpuLoad --verbose music.flac 2>debug.log
```

#### 2. 逐步调试
```bash
# 分步测试
xpuLoad --metadata music.flac  # 测试元数据提取
xpuLoad --data music.flac > /dev/null  # 测试数据解码
xpuLoad music.flac > /dev/null  # 测试完整流程
```

#### 3. 性能分析
```bash
# CPU分析
time xpuLoad music.flac

# 内存分析
valgrind --tool=massif xpuLoad music.flac
```

## 技术细节

### 架构设计

#### 模块化结构
```
xpuLoad
├── CLIInterface     # 命令行接口处理
├── AudioFileLoader  # FFmpeg音频解码
├── DSDDecoder       # DSD格式解码
├── MetadataExtractor # 元数据提取
└── OutputFormatter  # 输出格式化
```

#### 核心组件

##### AudioFileLoader
- **职责**：FFmpeg集成，通用音频格式解码
- **算法**：多线程解码，智能缓冲
- **输出**：标准化PCM + 完整元数据

##### DSDDecoder
- **职责**：DSD格式专用解码
- **算法**：硬件友好DSD-to-PCM转换
- **优化**：保持DSD原始特性的转换

##### MetadataExtractor
- **职责**：统一元数据提取接口
- **支持**：ID3v2、FLAC Vorbis、Vorbis Comment
- **扩展**：自定义标签支持

### 数据流设计

#### 输入处理
```
文件I/O → 格式检测 → 解码器选择 → 解码 → 格式标准化
```

#### 输出生成
```
解码数据 → 元数据提取 → JSON格式化 → PCM封装 → 输出
```

#### 内存管理
```
文件映射 → 解码缓冲 → 格式转换 → 输出缓冲 → 清理
```

### 性能优化

#### 解码优化
- **多线程**：自动负载均衡
- **SIMD**：向量化指令优化
- **缓存友好**：内存访问模式优化
- **零拷贝**：减少内存复制

#### I/O优化
- **异步I/O**：非阻塞文件读取
- **预读取**：智能缓冲策略
- **压缩流**：高效数据压缩/解压

### 扩展性设计

#### 未来扩展点
- **插件架构**：第三方解码器支持
- **GPU加速**：CUDA/OpenCL集成
- **网络流**：HTTP/RTSP流支持
- **实时处理**：低延迟流媒体

#### 接口设计
```cpp
// 可扩展的解码器接口
class IAudioDecoder {
public:
    virtual ErrorCode open(const std::string& path) = 0;
    virtual ErrorCode decode(std::vector<float>& output) = 0;
    virtual AudioMetadata getMetadata() const = 0;
    virtual bool isSeekable() const = 0;
    virtual ~IAudioDecoder() = default;
};
```

---

## 总结

xpuLoad作为XPU音频处理系统的核心模块，体现了现代音频软件设计的最佳实践：

- **专业化**：专注于音频文件解析，保持高质量标准
- **模块化**：遵循Unix哲学，易于集成和扩展
- **高性能**：优化的解码算法，支持高分辨率音频
- **跨平台**：一致的体验，跨操作系统兼容
- **标准化**：统一的输出格式，便于管道通信

通过合理使用xpuLoad，您可以构建高效、可靠的音频处理工作流，满足从个人音乐播放到专业音频制作的各种需求。

---

**版本信息**：xpuLoad v0.1.0  
**最后更新**：2025年1月8日  
**文档版本**：1.0  
**兼容性**：XPU v0.1.0+

如有问题或建议，请查阅相关技术文档或联系开发团队。