# XPU AI-Native 音乐播放系统 设计文档 v3.2

## 1. 项目概述

XPU 是一款专为 AI 时代设计的模块化音乐播放系统。每个功能模块都是独立的 CLI 工具，可以被人类直接使用，也可以被 LLM/AI Agent 通过 MCP、API 或 Agent-to-Agent 协议调用。

### 核心特性

- **CLI-First 设计**：每个模块都是独立的可执行文件，可单独使用
- **AI-Native**：内置 MCP (Model Context Protocol) 支持，LLM 可直接调用
- **远程部署**：支持服务端+边缘设备分离架构，xpuPlay 可部署在树莓派等设备
- **DLNA/AirPlay 支持**：xpuPlay 作为标准 DLNA/UPnP 和 AirPlay 播放器
- **多层封装**：CLI → REST API → MCP → Agent，逐层抽象
- **完全模块化**：Unix 哲学，一个程序只做一件事
- **跨平台**：Windows, macOS, Linux, ARM (树莓派)
- **专业音质**：只支持无损格式（FLAC/WAV/ALAC/DSD），最高支持 768kHz
- **高性能**：C/C++ 实现，极低延迟
- **网络流传输**：支持服务器到播放器的实时音频流传输

### 设计理念

```
┌─────────────────────────────────────────────────────────────────┐
│                        本地部署模式                               │
├─────────────────────────────────────────────────────────────────┤
│ 人类用户 ←─ 命令行 ──→ CLI 模块                                   │
│                         ↓                                        │
│ AI Agent ←─ MCP/API ──→ xpuDaemon (语义层)                      │
│                         ↓                                        │
│                    CLI 模块编排                                   │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        远程部署模式                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              服务器端 (Server)                            │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │   │
│  │  │ xpuLoad  │→│ xpuIn2Wav│→│xpuFingerp│→│xpuProcess│    │   │
│  │  │ (解析)   │ │ (转WAV)  │ │ (指纹)   │ │ (DSP)    │    │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └────┬─────┘    │   │
│  │                                           │             │   │
│  │  ┌────────────────────────────────────────▼─────────┐   │   │
│  │  │            xpuStream (网络流传输)                  │   │   │
│  │  └────────────────────────────────────────┬─────────┘   │   │
│  └───────────────────────────────────────────┼─────────────┘   │
│                                                │                 │
│                            ────────────────────┼──────           │
│                           HTTP/WebSocket/TCP  │                 │
│                            ────────────────────┼──────           │
│                                                │                 │
│  ┌───────────────────────────────────────────▼─────────────┐   │
│  │         边缘播放设备 (Edge Player)                        │   │
│  │  ┌──────────────────────────────────────────────────┐   │   │
│  │  │         xpuPlay (接收流 → 播放)                     │   │   │
│  │  │  ┌─────────────────┐  ┌─────────────────┐          │   │   │
│  │  │  │  Stream Receiver│→│  Audio Output   │          │   │   │
│  │  │  │  (HTTP/TCP)     │  │  (ALSA/CoreAudio)│         │   │   │
│  │  │  └─────────────────┘  └─────────────────┘          │   │   │
│  │  │                                                     │   │   │
│  │  │  ┌─────────────────┐  ┌─────────────────┐          │   │   │
│  │  │  │  DLNA Server    │  │  AirPlay Server │          │   │   │
│  │  │  │  (UPnP/DMP)     │  │  (RAOP)         │          │   │   │
│  │  │  └─────────────────┘  └─────────────────┘          │   │   │
│  │  └──────────────────────────────────────────────────┘   │   │
│  │                                                          │   │
│  │  硬件: 树莓派 4/5, NVIDIA Jetson, Intel NUC, 等        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│  AI Agent ──→ xpuDaemon (服务器) ──→ xpuPlay (边缘设备)        │
└─────────────────────────────────────────────────────────────────┘
```

## 2. 系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        AI 客户端层                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │ Claude   │  │  GPT-4   │  │  Web UI  │  │  Agent   │        │
│  │   MCP    │  │   MCP    │  │   API    │  │   A2A    │        │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘        │
└───────┼────────────┼─────────────┼─────────────┼────────────────┘
        │            │             │             │
┌───────▼────────────▼─────────────▼─────────────▼────────────────┐
│                  xpuDaemon (AI 原生守护进程)                      │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │  MCP Server  │  │  REST API    │  │  Agent Protocol     │   │
│  │  (stdio)     │  │  (HTTP/WS)   │  │  (WebSub/gRPC)      │   │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘   │
│         │                  │                     │              │
│         └──────────────────┼─────────────────────┘              │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              统一语义层 (Semantic Layer)                   │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │   │
│  │  │ 意图识别     │  │ 参数规范化   │  │ 结果格式化   │     │   │
│  │  │ (Intent)     │  │ (Normalize)  │  │ (Format)     │     │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │   │
│  └──────────────────────────────────────────────────────────┘   │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              模块编排引擎 (Orchestrator)                   │   │
│  │   • 管道构建     • 资源调度     • 生命周期管理              │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                   CLI 模块层 (独立可执行文件)                     │
│                                                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ xpuLoad  │→│ xpuIn2Wav│→│xpuFingerp│→│xpuProcess│→│xpuStream │ │
│  │ (解析)   │ │ (转WAV)  │ │ (指纹)   │ │ (DSP)    │ │ (网络流)  │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └────┬─────┘ │
│                                                              │      │
│                      ────────────────────────────────────────┼────── │
│                     HTTP/WebSocket/TCP (网络传输)            │      │
│                      ────────────────────────────────────────┼────── │
│                                                              │      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────▼─────┐ │
│  │ xpuClassi│→│xpuVisual│→│xpuOutWave│→│ xpuPlay  │←│xpuPlayRec  │ │
│  │ (分类)   │ │ (可视化) │ │ (输出)   │ │ (播放)   │ │ (流接收)   │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                         │
│  │ xpuQueue │ │ xpuMeta  │ │ xpuCache │                         │
│  │ (队列)   │ │ (元数据) │ │ (缓存)   │                         │
│  └──────────┘ └──────────┘ └──────────┘                         │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │          远程部署模式 (Server → Edge Player)              │   │
│  │                                                            │   │
│  │   服务器端              网络传输              边缘设备     │   │
│  │  ┌─────────┐          ──────────         ┌─────────┐     │   │
│  │  │xpuLoad  │──→xpuIn2Wav→xpuProcess→xpuStream─┼──────→│xpuPlay │     │   │
│  │  └─────────┘                                     │       └─────────┘     │   │
│  │          │                                     │            │            │   │
│  │          │     ──────────────────────          │            │            │   │
│  │          └────→ DLNA/AirPlay 发现 ────────────┘            │            │   │
│  │                                                       DLNA/AirPlay │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 架构原则

1. **CLI-First**: 每个模块都可独立运行，人类可直接使用
2. **AI-Native**: 所有功能都通过语义 API 暴露给 AI
3. **渐进式封装**: CLI → API → MCP，每层增加抽象
4. **状态无关**: CLI 模块无状态，状态由 Daemon 管理
5. **可观测性**: 所有操作都产生结构化日志
6. **容错设计**: 模块崩溃不影响其他模块

### 2.3 多层接口设计

```
┌─────────────────────────────────────────────────────────────┐
│ 第4层: Agent Protocol (Agent-to-Agent 通信)                  │
│   - WebSub/PubSub 事件推送                                   │
│   - gRPC 高性能通信                                          │
│   - 服务发现和注册                                           │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│ 第3层: MCP (Model Context Protocol)                          │
│   - stdio JSON-RPC 2.0                                       │
│   - Tools/Resources/Prompts 定义                             │
│   - LLM 友好的接口                                           │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│ 第2层: REST API (HTTP/WebSocket)                             │
│   - OpenAPI 3.0 规范                                         │
│   - JSON 请求/响应                                           │
│   - WebSocket 实时事件                                       │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│ 第1层: CLI (命令行接口)                                       │
│   - 标准输入/输出                                            │
│   - 人类可读的参数                                           │
│   - 管道组合                                                │
└─────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────┐
│ 第0层: Core (核心功能)                                        │
│   - xpuLoad, xpuIn2Wav, xpuPlay, ...                         │
└─────────────────────────────────────────────────────────────┘
```

## 3. CLI 模块设计

### 3.1 设计原则

每个 CLI 模块遵循以下原则：

1. **单一职责**: 只做一件事，做好它
2. **标准接口**: stdin 输入，stdout 输出，stderr 日志
3. **JSON 输出**: 机器可读的结构化输出
4. **错误码**: 标准化的退出码
5. **无状态**: 不维护持久化状态

### 3.2 核心 CLI 模块

#### 3.2.1 xpuLoad (音频解析)

```bash
# 基本用法
xpuLoad <file> [--format json|binary]

# 输出 (stdout)
{
  "success": true,
  "metadata": {
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name",
    "duration": 245.8,
    "sample_rate": 44100,
    "channels": 2,
    "bit_depth": 24,
    "codec": "flac"
  },
  "audio_stream": "base64_encoded_pcm_data"
}

# 管道模式
xpuLoad song.flac --format binary | xpuIn2Wav

# 支持的无损格式
# FLAC, WAV, ALAC, DSD (DSF/DSD)
```

#### 3.2.2 xpuIn2Wav (统一转换为 WAV + FFT 缓存)

**核心功能：将无损音频文件转换为标准 WAV 格式，并计算 FFT 频谱数据缓存到本地**

这是音频管道的"统一化"模块，确保后续所有模块只需处理 WAV 一种格式。同时计算 FFT 数据并缓存，避免后续模块重复计算。只支持无损格式：FLAC、WAV、ALAC、DSD。

```bash
# 基本用法（使用配置文件中的参数）
xpuIn2Wav

# 指定输出参数
xpuIn2Wav --rate 96000 --depth 32 --channels 2

# 最高质量模式
xpuIn2Wav --high-quality

# 快速模式（较低质量）
xpuIn2Wav --fast

# FFT 缓存选项
xpuIn2Wav --fft-cache                # 启用 FFT 缓存（默认）
xpuIn2Wav --no-fft-cache             # 禁用 FFT 缓存
xpuIn2Wav --fft-cache-dir ~/.cache/xpu/fft  # 指定缓存目录

# FFT 参数
xpuIn2Wav --fft-size 2048            # FFT 窗口大小（1024/2048/4096/8192）
xpuIn2Wav --fft-hop 1024              # FFT 跳跃大小（默认 fft-size/2）
xpuIn2Wav --fft-window hann          # 窗函数（hann/hamming/blackman）

# 输入 (stdin)
# 来自 xpuLoad 的二进制格式

# 输出 (stdout)
# 标准 WAV 格式 PCM 数据（44字节头部 + 数据）

# 输出 (stderr)
# FFT 缓存信息 JSON
```

**工作流程：**

```
┌─────────────────────────────────────────────────────────────┐
│ xpuIn2Wav 处理流程                                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 读取 xpuLoad 输出的二进制格式                          │
│     ├── 解析元数据和编码信息                                 │
│     └── 验证文件完整性                                       │
│                                                             │
│  2. 读取配置参数 (xpuSetting.conf)                        │
│     ├── target_sample_rate (目标采样率)                    │
│     ├── target_bit_depth (目标位深)                        │
│     └── channels (声道数)                                   │
│                                                             │
│  3. 解码音频数据                                            │
│     ├── 使用 FFmpeg 解码无损格式                           │
│     ├── FLAC, WAV, ALAC, DSD (DSF/DSD)                      │
│     └── 输出为原始 PCM                                       │
│                                                             │
│  4. 重采样和格式转换                                        │
│     ├── 转换到目标采样率 (最高 768kHz)                      │
│     ├── 转换到目标位深                                     │
│     └── 转换声道数                                          │
│                                                             │
│  5. 计算 FFT 频谱数据                                       │
│     ├── 分帧处理（重叠窗口）                                 │
│     ├── 应用窗函数                                          │
│     ├── FFT 变换                                            │
│     ├── 计算幅度谱和相位谱                                   │
│     └── 缓存到磁盘                                           │
│                                                             │
│  6. 输出标准 WAV 格式                                        │
│     ├── 44字节 WAV 头部                                     │
│     └── PCM 音频数据 → stdout                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**FFT 缓存输出格式：**

```json
{
  "cache_id": "a1b2c3d4e5f6",
  "cache_path": "/home/user/.cache/xpu/fft/a1b2c3d4e5f6",
  "audio_info": {
    "duration": 245.8,
    "sample_rate": 96000,
    "channels": 2,
    "bit_depth": 32
  },
  "fft_info": {
    "fft_size": 2048,
    "hop_size": 1024,
    "window_function": "hann",
    "num_frames": 22988,
    "freq_bins": 1025,
    "freq_resolution": 46.875
  },
  "files": {
    "magnitude": "magnitude.bin",      # 幅度谱（二进制浮点数组）
    "phase": "phase.bin",               # 相位谱（二进制浮点数组）
    "meta": "meta.json"                 # FFT 元数据
  }
}
```

**FFT 缓存结构：**

```
~/.cache/xpu/fft/<cache_id>/
├── meta.json             # 元数据
├── magnitude.bin         # 幅度谱 [frames × freq_bins]
├── phase.bin             # 相位谱 [frames × freq_bins]
└── config.json           # FFT 配置参数
```

**数据格式：**

```
magnitude.bin 和 phase.bin 格式：
- 二进制 float32 数组
- 维度：[num_frames × freq_bins × channels]
- 行优先存储（C 顺序）
- 大小计算：num_frames × freq_bins × channels × 4 字节

示例（96kHz, 2048 FFT, 50% overlap）：
- 245.8 秒音频 → 22988 帧
- 2048 FFT → 1025 频率箱
- 2 声道
- 总大小：22988 × 1025 × 2 × 4 = 188 MB
```

**支持的无损格式：**

| 格式 | 扩展名 | 最高采样率 | 最高位深 |
|------|--------|-----------|---------|
| FLAC | .flac | 384 kHz | 32-bit |
| WAV | .wav | 768 kHz | 32-bit |
| ALAC | .m4a | 192 kHz | 32-bit |
| DSD | .dsf, .dsd | 705.6/768 kHz | 1-bit |

**采样率支持：**

```
标准采样率：    44.1 kHz, 48 kHz
高采样率：      88.2 kHz, 96 kHz
超高采样率：    176.4 kHz, 192 kHz
极高采样率：    352.8 kHz, 384 kHz
顶级采样率：    705.6 kHz, 768 kHz
```

**质量模式：**

| 模式 | 重采样算法 | 抖动 | 说明 |
|------|-----------|------|------|
| `--high-quality` | Sinc 最佳质量 | TPDF | 适合音乐欣赏 |
| 默认 | Sinc 高质量 | TPDF | 平衡质量和速度 |
| `--fast` | 线性插值 | 无 | 快速转换，适合预览 |

**配置文件对应：**

```toml
# xpuSetting.conf
[playback]
target_sample_rate = 96000     # xpuIn2Wav 使用的采样率
target_bit_depth = 32          # xpuIn2Wav 使用的位深
channels = 2                   # xpuIn2Wav 使用的声道数

resample_quality = "high"      # high/medium/fast
dither = true                  # 24-bit 以下启用抖动

[fft_cache]
enabled = true                 # 默认启用 FFT 缓存
cache_dir = "~/.cache/xpu/fft"
fft_size = 2048                # FFT 窗口大小
hop_size = 1024                 # 跳跃大小（默认 fft_size/2）
window = "hann"                 # 窗函数
```

**设计理念：**

```
输入格式多样性                统一的 WAV 格式 + FFT 缓存
┌──────────────┐              ┌──────────────┐
│   FLAC      │              │              │
│   WAV       │    ┌─────┐    │              │
│   ALAC      │───→│  WAV │───→│   xpuProcess │
│   DSD       │    └─────┘    │  (标准化)    │
│  (DSF/DSD)  │        ↓       │              │
└──────────────┘    ┌─────┐    └──────────────┘
                    │ FFT │          ↓
                    │缓存│    后续模块可复用
                    └─────┘    FFT 数据
```

**为什么这样设计：**

1. **简化后续模块**：xpuProcess、xpuOutWave 只需处理 WAV
2. **保证音质**：统一的无损高质量转换起点
3. **易于缓存**：WAV 格式便于缓存和重用
4. **符合 WAV 规范**：标准格式，兼容性好
5. **FFT 预计算**：一次计算，多次复用，避免重复 FFT 变换
6. **性能优化**：频域操作直接使用缓存，速度快 10-100 倍
7. **可选设计**：可以禁用 FFT 缓存，保持向后兼容

#### 3.2.3 xpuFingerprint (音频指纹)

生成音频的唯一指纹标识，用于重复检测、版权识别和音乐匹配。

```bash
# 基本用法
xpuFingerprint [选项]

# 生成指纹（输出到 stdout）
xpuFingerprint

# 从 FFT 缓存读取（推荐，更快）
xpuFingerprint --fft-cache a1b2c3d4e5f6

# 从 WAV 读取（传统方式）
xpuFingerprint --from-wav

# 指定算法
xpuFingerprint --algorithm chromaprint   # Chromaprint/Acoustid (默认)
xpuFingerprint --algorithm acousticbrainz # AcousticBrainz
xpuFingerprint --algorithm bliss         # Bliss 音乐分析

# 输出格式
xpuFingerprint --format json            # JSON 格式 (默认)
xpuFingerprint --format binary          # 二进制格式
xpuFingerprint --format base64          # Base64 编码

# 分析长度（秒）
xpuFingerprint --duration 120           # 分析前 120 秒

# 输入 (stdin)
# WAV 格式 PCM 数据 或 FFT 缓存 ID

# 输出 (stdout)
# 音频指纹 JSON
```

**输出格式：**

```json
{
  "success": true,
  "fingerprint": {
    "algorithm": "chromaprint",
    "version": "1.0",
    "duration": 245.8,
    "duration_analyzed": 120.0,
    "fingerprint": "AQAAA0mUaEkSRZGSE4yW...",
    "hash": "a1b2c3d4e5f6...",
    "bitrate": 0,
    "sample_rate": 96000,
    "channels": 2
  },
  "acoustic_analysis": {
    "bpm": 120.5,
    "key": "A minor",
    "energy": 0.75,
    "danceability": 0.68,
    "acousticness": 0.23,
    "instrumentalness": 0.12,
    "liveness": 0.05,
    "speechiness": 0.08,
    "valence": 0.67
  }
}
```

**指纹算法对比：**

| 算法 | 用途 | 长度 | 精度 |
|------|------|------|------|
| Chromaprint | 通用音乐识别 | 120-180秒 | 高 |
| AcousticBrainz | 音乐特征分析 | 全曲 | 极高 |
| Bliss | 播放列表生成 | 全曲 | 中 |

**典型用法：**

```bash
# 生成指纹并查询在线数据库
xpuFingerprint --format json | \
    xpuQuery --api acousticbrainz

# 本地重复检测
xpuFingerprint --algorithm chromaprint | \
    xpuMatch --database ~/Music/fingerprints.db

# 音乐分析
xpuFingerprint --algorithm acousticbrainz

# 从 FFT 缓存生成（更快）
xpuFingerprint --fft-cache a1b2c3d4e5f6

# 完整管道（使用 FFT 缓存）
CACHE_ID=$(xpuLoad song.flac | xpuIn2Wav 2>&1 | jq -r '.cache_id')
xpuFingerprint --fft-cache $CACHE_ID
xpuClassify --fft-cache $CACHE_ID
```

**设计理念：**

```
┌──────────────┐     ┌──────────────┐
│   音频数据   │ ──→ │  xpuFingerprint │
│   (WAV)      │     │  (特征提取)   │
└──────────────┘     └──────────────┘
                            │
                    ┌───────┴────────┐
                    │                │
                ↓ 指纹           ↓ 音乐特征
            唯一标识符        BPM/调性/情绪
```

**为什么这样设计：**

1. **唯一标识**：每首音乐都有独特的指纹
2. **快速匹配**：指纹比较比音频比较快100倍
3. **特征提取**：为后续分类提供数据基础
4. **离线优先**：支持本地指纹数据库
5. **FFT 缓存支持**：可从缓存直接读取频谱数据，避免重复计算

**离线模式行为：**

```bash
# 离线模式（明确指定）
xpuFingerprint --offline --algorithm acousticbrainz
# 行为：仅使用本地算法，不尝试网络连接
# 退出码：0（成功）或 64（音频解码错误）
# 不返回网络相关错误

# 在线模式（默认，自动检测网络）
xpuFingerprint --algorithm acousticbrainz
# 行为：优先使用在线 API，失败时降级到本地分析
# 退出码：
#   0 - 成功（在线或本地）
#   72 - 网络错误（仅在强制在线模式时）

# 强制在线模式
xpuFingerprint --algorithm acousticbrainz --online
# 行为：必须使用在线 API，失败时返回错误
# 退出码：
#   0 - 在线查询成功
#   72 - 网络不可达
#   74 - API 服务不可用

# 混合模式（推荐）
xpuFingerprint --algorithm acousticbrainz --fallback local
# 行为：尝试在线 API，失败时自动使用本地算法
# 输出包含来源标识：
{
  "fingerprint": {...},
  "source": "local",  // 或 "online"
  "offline_mode": true
}
```

**离线模式规范：**

| 参数 | 行为 | 网络依赖 | 错误处理 |
|------|------|---------|---------|
| `--offline` | 完全本地，不尝试网络 | 无 | 无网络错误 |
| `--online` | 强制在线，失败报错 | 有 | 返回网络错误码 |
| 无参数（默认） | 智能模式，自动降级 | 可选 | 在线失败自动降级 |
| `--fallback local` | 明确降级行为 | 可选 | 返回降级状态 |

**网络错误码定义：**

```cpp
// 新增错误码（类别 Network = 0x6x）
enum class ErrorCode {
    NetworkUnreachable = 0x60,    // 网络不可达
    ApiServiceUnavailable = 0x61, // API 服务不可用
    ApiTimeout = 0x62,             // API 请求超时
    ApiRateLimit = 0x63,           // API 速率限制
    OfflineModeRequired = 0x64     // 需要离线模式
};

// 错误响应示例
{
  "error": {
    "code": 97,
    "category": "Network",
    "name": "ApiServiceUnavailable",
    "message": "AcousticBrainz API is currently unavailable",
    "context": {
      "api": "acousticbrainz",
      "url": "https://acousticbrainz.org/api/v1/...",
      "offline_mode_available": true
    },
    "suggestion": "Use --offline flag to use local analysis, or --fallback local for automatic fallback"
  }
}
```

#### 3.2.4 xpuClassify (音乐分类)

根据音频指纹和特征对音乐进行智能分类。

```bash
# 基本用法
xpuClassify [选项]

# 自动分类
xpuClassify

# 指定分类维度
xpuClassify --dimension genre        # 流派分类
xpuClassify --dimension mood         # 情绪分类
xpuClassify --dimension activity     # 活动分类
xpuClassify --dimension era          # 年代分类
xpuClassify --dimension all          # 全部分类

# 指定分类器
xpuClassify --classifier auto        # 自动选择 (默认)
xpuClassify --classifier rule        # 基于规则
xpuClassify --classifier ml          # 机器学习
xpuClassify --classifier hybrid      # 混合模式

# 输出格式
xpuClassify --format json            # JSON 格式 (默认)
xpuClassify --format tags           # 标签格式

# 置信度阈值
xpuClassify --confidence 0.7         # 只保留置信度 >0.7 的结果

# 输入 (stdin)
# 来自 xpuFingerprint 的 JSON

# 输出 (stdout)
# 分类结果 JSON
```

**分类维度：**

```bash
# 流派 (Genre)
# Rock, Pop, Jazz, Classical, Electronic, Hip-Hop,
# Country, R&B, Folk, Reggae, Metal, Punk, Blues, Soul

# 情绪 (Mood)
# Happy, Sad, Energetic, Calm, Angry, Romantic,
# Melancholic, Uplifting, Dark, Bright, Peaceful

# 活动 (Activity)
# Workout, Focus, Sleep, Party, Relax, Study,
# Driving, Running, Yoga, Meditation

# 年代 (Era)
# 50s, 60s, 70s, 80s, 90s, 2000s, 2010s, 2020s
```

**输出格式：**

```json
{
  "success": true,
  "classification": {
    "genre": [
      {"label": "Rock", "confidence": 0.92},
      {"label": "Classic Rock", "confidence": 0.87},
      {"label": "Hard Rock", "confidence": 0.65}
    ],
    "mood": [
      {"label": "Energetic", "confidence": 0.95},
      {"label": "Uplifting", "confidence": 0.78}
    ],
    "activity": [
      {"label": "Workout", "confidence": 0.88},
      {"label": "Party", "confidence": 0.72}
    ],
    "era": [
      {"label": "70s", "confidence": 0.91}
    ],
    "tags": [
      "electric-guitar", "drums", "vocals",
      "upbeat", "fast-tempo"
    ]
  },
  "suggestions": {
    "playlists": ["Classic Rock", "Workout Mix", "70s Hits"],
    "similar_tracks": ["track_id_1", "track_id_2", "track_id_3"]
  }
}
```

**分类规则示例：**

```
BPM > 120 + Energy > 0.7 → Workout
BPM < 60 + Acousticness > 0.8 → Sleep/Relax
Key: Minor + Valence < 0.3 → Sad/Melancholic
Instrumentalness > 0.8 → Background/Focus
Danceability > 0.8 → Party
```

**典型用法：**

```bash
# 完整分类
xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all

# 只分类流派
xpuIn2Wav | xpuFingerprint | xpuClassify --dimension genre

# 从 FFT 缓存分类（更快）
xpuClassify --fft-cache a1b2c3d4e5f6 --dimension all

# 基于分类自动创建播放列表
xpuIn2Wav | xpuFingerprint | xpuClassify | \
    xpuPlaylist --auto-organize

# 完整管道（使用 FFT 缓存）
CACHE_ID=$(xpuLoad song.flac | xpuIn2Wav 2>&1 | jq -r '.cache_id')
xpuFingerprint --fft-cache $CACHE_ID
xpuClassify --fft-cache $CACHE_ID --dimension all
```

**设计理念：**

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ xpuFingerprint │ ──→ │  xpuClassify  │ ──→ │  结构化标签   │
│  (音频特征)   │     │  (智能分类)   │     │  (可搜索)    │
└──────────────┘     └──────────────┘     └──────────────┘
                            │
                    ┌───────┴────────┐
                    │                │
                ↓ 规则引擎       ↓ ML 模型
            BPM/调性规则      预训练模型
```

**为什么这样设计：**

1. **多维度分类**：不只是流派，还包括情绪、活动、年代
2. **置信度评分**：知道分类的可靠性
3. **规则+ML**：结合规则引擎和机器学习
4. **可扩展**：可以添加新的分类维度和规则
5. **FFT 缓存支持**：从缓存直接读取频域特征，避免重复 FFT 计算

#### 3.2.5 xpuVisualize (音频可视化)

生成音频的时间域和频率域可视化数据，缓存到本地，支持按需读取和降采样。

**注意：xpuVisualize 可以复用 xpuIn2Wav 生成的 FFT 缓存数据。**

```bash
# 基本用法（生成完整可视化数据）
xpuVisualize

# 复用 FFT 缓存（推荐，更快）
xpuVisualize --fft-cache a1b2c3d4e5f6

# 从 WAV 生成（传统方式）
xpuVisualize --from-wav

# 指定缓存目录
xpuVisualize --cache-dir ~/.cache/xpu/visualize

# 输出格式
xpuVisualize --format json           # JSON 格式 (默认)
xpuVisualize --format binary         # 二进制格式（更紧凑）
xpuVisualize --format msgpack        # MessagePack 格式

# 时间域分辨率
xpuVisualize --time-resolution 100   # 每秒 100 个采样点
xpuVisualize --time-resolution 50    # 每秒 50 个采样点（更低精度）

# 频率域分辨率（复用 FFT 缓存时自动匹配）
xpuVisualize --fft-size 2048         # FFT 窗口大小
xpuVisualize --freq-bands 64         # 频率带数量

# 频率范围
xpuVisualize --freq-min 20           # 最低频率 20Hz
xpuVisualize --freq-max 20000        # 最高频率 20kHz

# 只生成特定类型的数据
xpuVisualize --type waveform         # 只生成波形数据
xpuVisualize --type spectrum         # 只生成频谱数据
xpuVisualize --type all              # 生成所有数据（默认）

# 降采样模式（用于快速预览）
xpuVisualize --downsample 4          # 4倍降采样
xpuVisualize --overview              # 生成概览模式（极低分辨率）

# 输入 (stdin)
# WAV 格式 PCM 数据

# 输出 (stdout)
# 可视化数据缓存信息 JSON
```

**输出格式：**

```json
{
  "success": true,
  "cache_id": "a1b2c3d4e5f6",
  "cache_path": "/home/user/.cache/xpu/visualize/a1b2c3d4e5f6",
  "duration": 245.8,
  "sample_rate": 96000,
  "channels": 2,
  "data_types": {
    "waveform": {
      "path": "waveform.bin",
      "size": 24580,
      "resolution": 100,
      "points_per_second": 100
    },
    "spectrum": {
      "path": "spectrum.bin",
      "size": 1573120,
      "fft_size": 2048,
      "freq_bands": 64,
      "frames_per_second": 50
    }
  },
  "metadata": {
    "created_at": "2026-01-07T12:34:56Z",
    "format_version": "1.0"
  }
}
```

**时间域数据（波形）：**

```json
{
  "type": "waveform",
  "sample_rate": 100,
  "channels": 2,
  "duration": 245.8,
  "points": 24580,
  "data": [
    {"time": 0.0, "left": -0.2, "right": -0.18},
    {"time": 0.01, "left": -0.15, "right": -0.12},
    ...
  ]
}
```

**频率域数据（频谱）：**

```json
{
  "type": "spectrum",
  "fft_size": 2048,
  "freq_bands": 64,
  "freq_min": 20,
  "freq_max": 20000,
  "frames_per_second": 50,
  "total_frames": 12290,
  "data": [
    {
      "time": 0.0,
      "bands": [
        {"freq": 20, "magnitude": 0.1},
        {"freq": 50, "magnitude": 0.15},
        {"freq": 100, "magnitude": 0.2},
        ...
      ]
    },
    ...
  ]
}
```

**缓存结构：**

```
~/.cache/xpu/visualize/<cache_id>/
├── meta.json              # 元数据
├── waveform.bin           # 波形数据（二进制）
├── spectrum.bin           # 频谱数据（二进制）
├── overview.bin           # 概览数据（极低分辨率）
└── thumbnail.png          # 缩略图（可选）
```

**读取缓存数据：**

```bash
# 读取完整波形数据
xpuVisualize --read a1b2c3d4e5f6 --type waveform

# 读取部分波形数据（10-20秒）
xpuVisualize --read a1b2c3d4e5f6 --type waveform --start 10 --end 20

# 读取降采样数据（降低解析度）
xpuVisualize --read a1b2c3d4e5f6 --type waveform --downsample 10

# 读取概览数据（极低分辨率，快速加载）
xpuVisualize --read a1b2c3d4e5f6 --overview

# 读取频谱数据
xpuVisualize --read a1b2c3d4e5f6 --type spectrum

# 读取特定时间段的频谱
xpuVisualize --read a1b2c3d4e5f6 --type spectrum --start 30 --end 60

# 输出为 JSON
xpuVisualize --read a1b2c3d4e5f6 --output json

# 输出为二进制（更快）
xpuVisualize --read a1b2c3d4e5f6 --output binary

# 输出为图像（PNG）
xpuVisualize --read a1b2c3d4e5f6 --output image --width 1920 --height 1080
```

**数据格式对比：**

| 格式 | 大小 | 读取速度 | 适用场景 |
|------|------|---------|---------|
| 完整数据 | 100% | 慢 | 精细分析 |
| 降采样 2x | 50% | 中等 | 一般预览 |
| 降采样 10x | 10% | 快 | 快速浏览 |
| 概览模式 | 1% | 极快 | 缩略图 |

**降采样示例：**

```bash
# 生成完整数据
xpuLoad song.flac | xpuIn2Wav | xpuVisualize
# 输出：24580 个波形点，每秒 100 点

# 读取 10 倍降采样数据
xpuVisualize --read a1b2c3d4e5f6 --downsample 10
# 输出：2458 个波形点，每秒 10 点

# 读取 100 倍降采样数据
xpuVisualize --read a1b2c3d4e5f6 --downsample 100
# 输出：246 个波形点，每秒 1 点

# 读取概览模式
xpuVisualize --read a1b2c3d4e5f6 --overview
# 输出：25 个波形点，每首歌曲 25 点
```

**可视化类型：**

```bash
# 波形图（Waveform）
xpuVisualize --type waveform

# 频谱图（Spectrum）
xpuVisualize --type spectrum

# 频谱图（Spectrogram）- 时间-频率热图
xpuVisualize --type spectrogram

# 音量包络（Envelope）
xpuVisualize --type envelope

# 立体声声场（Stereo Field）
xpuVisualize --type stereo

# 所有类型
xpuVisualize --type all
```

**缓存管理：**

```bash
# 列出所有缓存
xpuVisualize --cache-list

# 删除特定缓存
xpuVisualize --cache-delete a1b2c3d4e5f6

# 清空所有缓存
xpuVisualize --cache-clear

# 缓存统计
xpuVisualize --cache-stats
# 输出：
{
  "total_caches": 125,
  "total_size": 2147483648,
  "oldest_cache": "2025-12-01",
  "newest_cache": "2026-01-07"
}
```

**设计理念：**

```
┌──────────────────────────────────────────────────────────┐
│                 xpuVisualize 处理流程                     │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  WAV 数据输入                                            │
│      ↓                                                   │
│  ┌─────────────────────────────────────┐                │
│  │   时间域分析（波形）                │                │
│  │   • 采样：100-1000 点/秒           │                │
│  │   • 输出：时间-幅度序列             │                │
│  └─────────────────────────────────────┘                │
│      ↓                                                   │
│  ┌─────────────────────────────────────┐                │
│  │   频率域分析（频谱）                │                │
│  │   • FFT 窗口：2048/4096            │                │
│  │   • 频率带：32-128                 │                │
│  │   • 输出：时间-频率-幅度 3D 数据   │                │
│  └─────────────────────────────────────┘                │
│      ↓                                                   │
│  ┌─────────────────────────────────────┐                │
│  │   缓存到磁盘                        │                │
│  │   • 元数据：JSON                    │                │
│  │   • 波形数据：二进制               │                │
│  │   • 频谱数据：二进制               │                │
│  └─────────────────────────────────────┘                │
│      ↓                                                   │
│  ┌─────────────────────────────────────┐                │
│  │   按需读取                          │                │
│  │   • 完整数据 / 降采样               │                │
│  │   • 时间范围筛选                    │                │
│  │   • 数据类型筛选                    │                │
│  └─────────────────────────────────────┘                │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

**为什么这样设计：**

1. **分离关注点**：可视化不影响音频播放流程
2. **缓存优先**：避免重复计算，提高响应速度
3. **按需读取**：支持读取部分数据和降采样，适应不同场景
4. **多分辨率**：概览、预览、完整数据三级精度
5. **磁盘缓存**：大型数据持久化，节省内存

**典型用法：**

```bash
# 生成可视化数据（完整管道）
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify | \
    xpuVisualize --cache-dir ~/.cache/xpu/visualize | \
    xpuProcess | xpuOutWave | xpuPlay

# 复用 FFT 缓存（更快）
xpuVisualize --fft-cache a1b2c3d4e5f6

# 后台生成可视化（不阻塞播放）
xpuLoad song.flac | xpuIn2Wav | xpuVisualize &

# 读取波形数据用于 UI 显示
xpuVisualize --read a1b2c3d4e5f6 --type waveform --downsample 10

# 读取频谱数据用于实时可视化
xpuVisualize --read a1b2c3d4e5f6 --type spectrum --start 0 --end 5

# 生成缩略图
xpuVisualize --read a1b2c3d4e5f6 --overview --output image \
    --width 400 --height 100 > thumbnail.png
```

#### 3.2.6 xpuProcess (DSP 处理器)

数字信号处理模块，对音频进行实时效果处理。支持从 WAV 或 FFT 缓存读取数据。支持 GPU 加速计算。

```bash
# 基本用法
xpuProcess [选项]

# 计算后端选择（新增）
xpuProcess --backend auto          # 自动选择（优先GPU，降级CPU）
xpuProcess --backend gpu           # 强制使用GPU加速
xpuProcess --backend cpu           # 强制使用CPU
xpuProcess --backend cuda          # 使用CUDA（NVIDIA）
xpuProcess --backend opencl        # 使用OpenCL（跨平台）
xpuProcess --backend metal         # 使用Metal（Apple Silicon）
xpuProcess --backend vulkan        # 使用Vulkan

# 查看可用计算设备
xpuProcess --list-devices
# 输出:
# {
#   "available_backends": ["cuda", "opencl", "cpu"],
#   "devices": [
#     {"id": 0, "name": "NVIDIA RTX 4090", "type": "gpu", "memory": 24576},
#     {"id": 1, "name": "Intel Core i9-13900K", "type": "cpu", "cores": 24}
#   ],
#   "recommended": 0
# }

# 指定GPU设备
xpuProcess --device 0              # 使用第一个GPU
xpuProcess --device "RTX 4090"     # 指定设备名称

# 从 WAV 读取（传统方式）
xpuProcess --from-wav

# 从 FFT 缓存读取（推荐，频域操作更快）
xpuProcess --fft-cache a1b2c3d4e5f6

# 音量控制（时域）
xpuProcess --volume 0.8           # 设置音量 0.0-1.0
xpuProcess --volume +10           # 增加 10dB
xpuProcess --volume -5            # 减少 5dB

# 淡入淡出（时域）
xpuProcess --fade-in 2000         # 2秒淡入
xpuProcess --fade-out 3000        # 3秒淡出

# 均衡器（频域，使用 FFT 缓存更快）
xpuProcess --eq preset=rock       # 预设均衡器
xpuProcess --eq custom:60=+3,250=-2,1000=0,4000=+2,16000=+4
xpuProcess --eq flat              # 平坦响应

# 动态范围
xpuProcess --compress 3:1 --threshold -18dB
xpuProcess --limit -0.1dB

# 速度和音调
xpuProcess --speed 1.5            # 1.5倍速
xpuProcess --pitch +2             # 升2个半音
xpuProcess --preserve-pitch       # 变速不变调

# 3D 音效
xpuProcess --3d surround          # 环绕效果
xpuProcess --3d headphone         # 耳机优化

# 空间效果
xpuProcess --reverb hall --decay 2.5 --wet 0.3     # 大厅混响
xpuProcess --reverb room --decay 1.0 --wet 0.2     # 房间混响
xpuProcess --reverb plate --decay 1.5 --wet 0.25   # 板式混响
xpuProcess --delay 500 --feedback 0.4 --mix 0.3    # 500ms延迟
xpuProcess --echo 300 --decay 0.5                   # 回声效果

# 调制效果
xpuProcess --chorus --rate 1.5 --depth 0.5 --mix 0.3  # 合唱
xpuProcess --chorus --rate 0.5 --depth 0.8 --mix 0.5  # 深度合唱
xpuProcess --flanger --rate 0.2 --depth 0.8 --mix 0.5 # 镶边
xpuProcess --phaser --rate 0.5 --depth 0.7 --mix 0.4  # 移相

# 失真和饱和
xpuProcess --distortion 30 --tone 0.6              # 30%失真
xpuProcess --overdrive 20                          # 过载
xpuProcess --saturation analog --drive 0.7         # 模拟饱和

# 电子管/真空管模拟（新增）
xpuProcess --tube 12AX7 --drive 0.7 --blend 0.5    # 12AX7电子管
xpuProcess --tube EL34 --drive 0.8 --blend 0.6    # EL34电子管（英式失真）
xpuProcess --tube 6L6 --drive 0.6 --blend 0.5      # 6L6电子管（美式失真）
xpuProcess --tube 6V6 --drive 0.5 --blend 0.4      # 6V6电子管（经典美式）
xpuProcess --tube EL84 --drive 0.7 --blend 0.5     # EL84电子管（英式清音）
xpuProcess --tube preamp --drive 0.4 --blend 0.3   # 前级电子管
xpuProcess --tube poweramp --drive 0.8 --blend 0.7 # 功放电子管
xpuProcess --tube triode --drive 0.6               # 三极管模式
xpuProcess --tube pentode --drive 0.8              # 五极管模式

# 电子管预设（新增）
xpuProcess --tube-preset fender-clean   # Fender清音
xpuProcess --tube-preset fender-crunch  # Fender过载
xpuProcess --tube-preset marshall-drive # Marshall失真
xpuProcess --tube-preset vox-chime      # Vox清脆
xpuProcess --tube-preset mesa-boogie    # Mesa Boogie高增益
xpuProcess --tube-preset hiwatt        # Hiwatt英式
xpuProcess --tube-preset soldano       # Soldano现代高增益

# 电子管参数详解（新增）
# --tube <型号>       # 电子管型号（12AX7, EL34, 6L6, 6V6, EL84等）
# --drive <0.0-1.0>   # 驱动强度（电子管饱和度）
# --blend <0.0-1.0>   # 混合比例（干声/湿声）
# --bias <cold/warm>  # 偏置方式（cold=冷偏置，warm=暖偏置）
# --sag <0.0-1.0>     # 电源下垂（模拟电源压缩效应）
# --rectifier <tube/ss> # �流管类型（tube=电子管，ss=固态）

# 立体声处理
xpuProcess --stereo-width 1.5                      # 扩展立体声宽度
xpuProcess --stereo-swap                           # 交换左右声道
xpuProcess --mid-side --mid-gain +2 --side-gain -1  # M/S处理

# 性能监控（新增）
xpuProcess --benchmark                              # 性能基准测试
xpuProcess --profile                               # 输出性能分析
xpuProcess --verbose-gpu                           # 显示GPU使用详情

# 输入 (stdin)
# WAV 格式 PCM 数据

# 输出 (stdout)
# 处理后的 WAV 格式 PCM 数据
```

**GPU加速支持：**

| 后端 | 说明 | 平台 | 性能提升 |
|------|------|------|----------|
| CUDA | NVIDIA GPU加速 | Linux/Windows | 10-50x |
| Metal | Apple Silicon加速 | macOS | 5-20x |
| OpenCL | 跨平台GPU加速 | Linux/Windows/macOS | 3-15x |
| Vulkan | 现代图形API | Linux/Windows | 5-20x |
| CPU | 降级方案 | 所有平台 | 1x (基准) |

**GPU加速的效果：**

| 效果类型 | CPU | GPU | 加速比 |
|---------|-----|-----|--------|
| FFT变换 | 慢 | 极快 | 20-50x |
| 卷积混响 | 慢 | 快 | 10-30x |
| 电子管模拟 | 慢 | 快 | 5-15x |
| 均衡器 | 中等 | 快 | 5-10x |
| 压缩器 | 快 | 中等 | 2-5x |

```bash
# GPU加速示例
# 使用GPU加速FFT计算和混响
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend cuda --reverb hall --decay 2.5 | \
    xpuPlay

# Apple Silicon GPU加速
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend metal --tube EL34 --drive 0.7 | \
    xpuPlay

# 自动检测最佳后端
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend auto | \
    xpuPlay

# 性能对比测试
xpuProcess --benchmark --effect "reverb hall --decay 2.5"
# 输出:
# {
#   "cpu_time": 2.45,
#   "gpu_time": 0.12,
#   "speedup": 20.4,
#   "gpu_used": "NVIDIA RTX 4090"
# }
```

**电子管型号特性：**

| 型号 | 特性 | 用途 |
|------|------|------|
| 12AX7 | 高增益，低噪音 | 前级放大 |
| EL34 | 英式失真，中频突出 | Marshall风格 |
| 6L6 | 美式清音，低频饱满 | Fender风格 |
| 6V6 | 温暖清音，早期美式 | 经典Fender |
| EL84 | 英式清音到过载 | Vox, Bluesbreaker |
| 6550 | 高功率，低失真 | Hi-Fi功放 |
| KT88 | 高功率，丰满低音 | Hi-Fi，Bass |

```bash
# 电子管模拟示例

# 经典Fender清音（6L6 + 暖偏置）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube 6L6 --drive 0.4 --bias warm --blend 0.3 | \
    xpuPlay

# Marshall失真（EL34 + 冷偏置）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube EL34 --drive 0.8 --bias cold --blend 0.7 | \
    xpuPlay

# Vox清脆音色（EL84 + 三极管模式）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube EL84 --drive 0.5 --triode --blend 0.4 | \
    xpuPlay

# 高增益现代音色（12AX7前级 + EL34功放）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube preamp --drive 0.7 --blend 0.4 | \
    xpuProcess --tube poweramp --drive 0.8 --blend 0.6 | \
    xpuPlay

# 电源压缩效应（模拟电源不够时的音色压缩）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube 6L6 --drive 0.7 --sag 0.6 --rectifier tube | \
    xpuPlay

# 使用预设
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube-preset marshall-drive | \
    xpuPlay

# GPU加速的电子管模拟
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend cuda --tube EL34 --drive 0.8 | \
    xpuPlay
```

**支持的均衡器预设：**

```bash
# 预设列表
xpuProcess --eq list-presets

输出:
{
  "presets": [
    {"name": "flat", "description": "平坦响应"},
    {"name": "rock", "description": "摇滚"},
    {"name": "pop", "description": "流行"},
    {"name": "jazz", "description": "爵士"},
    {"name": "classical", "description": "古典"},
    {"name": "electronic", "description": "电子"},
    {"name": "bass", "description": "低音增强"},
    {"name": "vocal", "description": "人声增强"},
    {"name": "speech", "description": "语音"}
  ]
}
```

**链式处理示例：**

```bash
# 组合多个效果
xpuIn2Wav | xpuProcess --volume 0.8 --eq rock --compress 2:1 | xpuPlay

# 从 FFT 缓存应用频域处理（更快）
xpuProcess --fft-cache a1b2c3d4e5f6 --eq rock

# 跨文件渐变
xpuProcess song1.wav --fade-out 2000 > out1.wav
xpuProcess song2.wav --fade-in 2000 > out2.wav

# 完整效果链（空间+动态+立体声）
xpuIn2Wav | \
    xpuProcess --reverb hall --decay 2.0 --wet 0.25 | \
    xpuProcess --compress 3:1 --threshold -16dB | \
    xpuProcess --stereo-width 1.3 | \
    xpuPlay
```

**DSP 预设管理（新增）：**

```bash
# 保存当前效果链为预设
xpuProcess --save-preset my_preset \
    --reverb hall --decay 2.0 --wet 0.25 \
    --compress 3:1 --threshold -16dB \
    --eq rock

# 列出所有预设
xpuProcess --list-presets
# 输出
{
  "presets": [
    {"name": "my_preset", "description": "用户自定义预设"},
    {"name": "studio_reverb", "description": "录音棚混响"},
    {"name": "live_vocal", "description": "现场人声"},
    {"name": "bass_boost", "description": "低音增强"}
  ]
}

# 加载预设
xpuProcess --load-preset my_preset

# 导出预设配置
xpuProcess --export-preset my_preset --output preset.json

# 导入预设
xpuProcess --import-preset --input preset.json

# 删除预设
xpuProcess --delete-preset my_preset

# 内置预设列表
# - studio_reverb: 录音棚混响（reverb+compressor）
# - live_vocal: 现场人声（eq+vocal+compressor）
# - bass_boost: 低音增强（eq bass+stereo-width）
# - concert_hall: 音乐厅效果（reverb hall+delay）
# - analog_warmth: 模拟温暖（saturation analog+compressor）
# - fender_clean: Fender清音（tube 6L6+bias warm）
# - fender_crunch: Fender过载（tube 6L6+drive 0.6）
# - marshall_drive: Marshall失真（tube EL34+bias cold）
# - marshall_high: Marshall高增益（tube EL34+drive 0.9）
# - vox_chime: Vox清脆（tube EL84+triode）
# - mesa_boogie: Mesa Boogie高增益（tube 6L6+drive 0.85）
# - hiwatt: Hiwatt英式（tube EL84+bias warm）
# - soldano: Soldano现代高增益（tube 12AX7+EL34）
# - dumble: Dumble传奇音色（tube 6L6+drive 0.7+sag 0.4）
```

**预设存储位置：**
```
~/.config/xpu/dsp/presets/
├── my_preset.json
├── studio_reverb.json
├── live_vocal.json
├── bass_boost.json
├── fender_clean.json          # 电子管预设
├── marshall_drive.json        # 电子管预设
└── custom_tube_chain.json     # 自定义电子管链
```

**预设格式：**
```json
{
  "name": "studio_reverb",
  "description": "录音棚混响效果",
  "backend": "auto",
  "effects": [
    {
      "type": "reverb",
      "params": {
        "room": "hall",
        "decay": 2.0,
        "wet": 0.25
      }
    },
    {
      "type": "compressor",
      "params": {
        "ratio": "3:1",
        "threshold": "-16dB"
      }
    },
    {
      "type": "eq",
      "params": {
        "preset": "rock"
      }
    }
  ]
}
```

**电子管预设格式：**
```json
{
  "name": "marshall_drive",
  "description": "Marshall失真音色",
  "backend": "auto",
  "tube_chain": [
    {
      "stage": "preamp",
      "tube": "12AX7",
      "drive": 0.7,
      "blend": 0.5,
      "bias": "cold"
    },
    {
      "stage": "poweramp",
      "tube": "EL34",
      "drive": 0.8,
      "blend": 0.7,
      "bias": "cold"
    }
  ],
  "effects": [
    {
      "type": "eq",
      "params": {
        "preset": "marshall"
      }
    },
    {
      "type": "compressor",
      "params": {
        "ratio": "4:1",
        "threshold": "-14dB",
        "attack": 5,
        "release": 50
      }
    }
  ]
}
```

**创建电子管预设：**
```bash
# 保存电子管预设
xpuProcess --save-preset my_tone \
    --tube 12AX7 --drive 0.7 --blend 0.4 \
    --tube EL34 --drive 0.8 --blend 0.6 \
    --eq custom:60=+2,250=-1,1000=0,2000=+2,4000=+3,8000=+2 \
    --compress 3:1 --threshold -14dB

# 查看预设内容
cat ~/.config/xpu/dsp/presets/my_tone.json

# 编辑预设（手动调整）
vim ~/.config/xpu/dsp/presets/my_tone.json

# 分享预设
xpuProcess --export-preset my_tone --output my_tone.json
# 共享给朋友
xpuProcess --import-preset --input friend_tone.json
```

#### 3.2.7 xpuOutWave (输出转换器)

将音频转换为输出设备最佳格式的最终转换器。只支持无损格式输出，输出保持为 WAV。

```bash
# 基本用法
xpuOutWave [选项]

# 从 WAV 读取（传统方式）
xpuOutWave --from-wav

# 从 FFT 缓存读取（用于格式转换时更高效）
xpuOutWave --fft-cache a1b2c3d4e5f6

# 自动检测设备最佳格式
xpuOutWave --auto

# 指定输出采样率和位深
xpuOutWave --rate 192000 --depth 32 --channels 2

# 支持的无损输出格式（仅用于保存，播放始终为 WAV）
xpuOutWave --format wav           # WAV PCM (默认)
xpuOutWave --format flac          # FLAC 无损压缩
xpuOutWave --format dsd           # DSD 格式 (需要设备支持)

# FLAC 压缩级别 (仅用于 FLAC 格式)
xpuOutWave --compression 0        # 0-8, 0=最快, 8=最小

# 抖动 (位深降低时)
xpuOutWave --dither tpdf          # TPDF 抖动 (推荐)
xpuOutWave --dither none          # 无抖动

# 元数据处理
xpuOutWave --embed-metadata       # 嵌入元数据
xpuOutWave --strip-metadata       # 移除元数据

# 输入 (stdin)
# WAV 格式 PCM 数据

# 输出 (stdout)
# 转换后的音频数据
```

**设备格式自动检测：**

```bash
# 查询设备支持格式
xpuOutWave --query-device default

输出:
{
  "device": "default",
  "supported_formats": {
    "sample_rates": [44100, 48000, 96000, 192000, 384000],
    "bit_depths": [16, 24, 32],
    "channels": [1, 2],
    "native_dsd": false
  },
  "recommended": {
    "sample_rate": 192000,
    "bit_depth": 32,
    "channels": 2
  },
  "latency_ms": 23
}
```

**格式转换示例：**

```bash
# 转换为 FLAC (无损压缩)
xpuIn2Wav | xpuOutWave --format flac --compression 0 > output.flac

# 输出为高分辨率 WAV
xpuIn2Wav | xpuOutWave --rate 192000 --depth 32 > output.wav

# 自动检测最佳格式
xpuIn2Wav | xpuOutWave --auto | xpuPlay

# 为高端设备优化 (32-bit 192kHz)
xpuIn2Wav | xpuOutWave --rate 192000 --depth 32 --dither tpdf | xpuPlay
```

#### 3.2.8 xpuStream (网络流传输)

xpuStream 是服务器端模块，负责将处理后的音频数据流式传输到远程播放设备。支持 HTTP、WebSocket 和 TCP 三种传输协议。

**基本用法：**

```bash
# 从管道接收音频并流式传输到远程设备
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave | \
    xpuStream --target http://192.168.1.100:8080/stream

# 使用 WebSocket（推荐，支持双向通信）
xpuStream --target ws://192.168.1.100:8080/stream --protocol websocket

# 使用原始 TCP（最低延迟）
xpuStream --target tcp://192.168.1.100:8080 --protocol tcp

# 从缓存流式传输（边读边传，节省内存）
xpuStream --cache a1b2c3d4e5f6 --target http://192.168.1.100:8080/stream
```

**协议选择：**

| 协议 | 延迟 | 带宽开销 | 双向通信 | 适用场景 |
|------|------|---------|---------|---------|
| HTTP | 中 | 低 | 否 | 简单部署、防火墙友好 |
| WebSocket | 低 | 低 | 是 | 实时控制、状态同步 |
| TCP | 极低 | 极低 | 否 | 局域网、最低延迟 |

**传输选项：**

```bash
# 编码格式
xpuStream --codec wav               # 无压缩 WAV（默认）
xpuStream --codec flac              # FLAC 无损压缩（节省带宽）
xpuStream --codec opus --bitrate 256 # Opus 有损压缩（低带宽）

# 缓冲控制
xpuStream --buffer-size 65536       # 64KB 缓冲（默认）
xpuStream --buffer-size 131072      # 128KB 缓冲（更稳定）
xpuStream --buffer-size 16384       # 16KB 缓冲（更低延迟）

# 网络优化
xpuStream --mtu 1500                # 设置 MTU
xpuStream --tos 0x80                # 设置 TOS（低延迟）
xpuStream --ttl 64                  # 设置 TTL

# 重连策略
xpuStream --reconnect               # 自动重连
xpuStream --reconnect-delay 5       # 重连延迟（秒）
xpuStream --reconnect-max 10        # 最大重连次数

# 元数据传输
xpuStream --send-metadata           # 发送元数据
xpuStream --metadata-interval 5     # 元数据更新间隔（秒）
```

**远程设备发现：**

```bash
# 发现局域网内的 XPU 播放设备
xpuStream --discover

# 输出:
{
  "devices": [
    {
      "id": "xpu_player_001",
      "name": "Living Room Speaker",
      "host": "192.168.1.100",
      "port": 8080,
      "protocols": ["http", "websocket", "tcp", "dlna", "airplay"],
      "capabilities": {
        "sample_rates": [44100, 48000, 96000],
        "max_bit_depth": 24,
        "codecs": ["wav", "flac"]
      },
      "status": "idle"
    },
    {
      "id": "xpu_player_002",
      "name": "Bedroom Speaker",
      "host": "192.168.1.101",
      "port": 8080,
      "protocols": ["http", "websocket", "dlna"],
      "status": "playing"
    }
  ]
}

# 发现 DLNA 设备
xpuStream --discover-dlna

# 发现 AirPlay 设备
xpuStream --discover-airplay
```

**使用发现的目标：**

```bash
# 流式传输到发现的设备
xpuStream --target xpu_player_001

# 等价于
xpuStream --target http://192.168.1.100:8080/stream
```

**HTTP 流式传输模式：**

```bash
# 服务器端：启动 HTTP 流服务器
xpuStream --mode server --port 9000 \
    --codec flac --buffer-size 131072

# 输出:
{
  "server_url": "http://0.0.0.0:9000/stream",
  "stream_id": "stream_20250107_123456",
  "protocol": "http",
  "codec": "flac"
}

# 客户端（边缘设备）：接收流
xpuPlay --stream-url http://192.168.1.10:9000/stream

# 或者使用 xpuPlayRec（流接收模式）
xpuPlay --mode receive --url http://192.168.1.10:9000/stream
```

**WebSocket 流式传输模式：**

```bash
# 服务器端
xpuStream --mode server --protocol websocket --port 9000

# 客户端（支持双向控制）
xpuPlay --mode receive --url ws://192.168.1.10:9000/stream

# 从客户端发送控制命令
echo '{"command": "pause"}' | \
    curl -X POST http://192.168.1.10:9000/control
```

**状态同步：**

```bash
# 启用状态同步
xpuStream --target xpu_player_001 \
    --status-sync --status-interval 1

# 状态输出（stderr）
{"event": "connected", "device": "xpu_player_001"}
{"event": "playing", "position": 12.5}
{"event": "buffer", "percent": 85}
{"event": "disconnected", "reason": "client_closed"}
```

**性能优化：**

```bash
# FLAC 压缩传输（节省 50-70% 带宽）
xpuStream --target xpu_player_001 --codec flac --compression 5

# 多播传输（局域网多设备同时播放）
xpuStream --mode multicast --group 239.255.0.1 --port 8080

# 自适应比特率（根据网络状况调整）
xpuStream --adaptive-bitrate --min-bitrate 128 --max-bitrate 512
```

**设计理念：**

```
┌─────────────────────────────────────────────────────────────┐
│                    xpuStream 工作流程                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  服务器端                          边缘设备                   │
│  ┌─────────┐      ──────────→     ┌─────────┐              │
│  │ 音频源  │      网络流           │ 播放器  │              │
│  │ (WAV)   │      HTTP/WS/TCP      │ (接收)  │              │
│  └────┬────┘                       └────┬────┘              │
│       │                                 │                   │
│       │  ────────── ←───────────────────│                   │
│       │      控制命令                        │                   │
│       │    (pause/volume/seek)              │                   │
│       │                                 │                   │
│  ┌────▼────┐                       ┌────▼────┐              │
│  │ 编码器  │                       │ 解码器  │              │
│  │ (可选)  │                       │ (可选)  │              │
│  └─────────┘                       └─────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**为什么这样设计：**

1. **协议灵活**：支持 HTTP/WebSocket/TCP，适应不同场景
2. **带宽优化**：可选 FLAC/Opus 压缩，节省 50-90% 带宽
3. **设备发现**：自动发现局域网内播放设备，无需手动配置 IP
4. **双向通信**：WebSocket 支持从播放器发送控制命令到服务器
5. **状态同步**：实时同步播放状态，便于监控和控制
6. **容错机制**：自动重连、缓冲恢复

**典型用法：**

```bash
# 场景1：服务器到树莓派播放
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave | \
    xpuStream --target xpu_player_001 --codec flac

# 场景2：服务器启动，多设备可连接
xpuStream --mode server --port 9000 --codec flac
# 树莓派连接：xpuPlay --stream-url http://server:9000/stream

# 场景3：多播同步播放（派对模式）
xpuStream --mode multicast --group 239.255.0.1 --port 8080
# 多个设备同时加入组播组

# 场景4：DLNA 推送
xpuStream --discover-dlna
xpuStream --target dlna://Living Room TV --codec flac

# 场景5：AirPlay 推送
xpuStream --discover-airplay
xpuStream --target airplay://HomePod --codec wav
```

#### 3.2.9 xpuPlay (音频播放)

音频播放模块，支持本地文件播放和网络流媒体播放。

```bash
# 本地文件播放（通过管道）
xpuLoad song.flac | xpuIn2Wav | xpuPlay

# 基本用法
xpuPlay [--device default] [--volume 0.8]

# 指定设备
xpuPlay --device alsa_output.pci-0000_00_1f.3.analog-stereo

# 缓冲大小
xpuPlay --buffer-size 1024        # 1024 samples (默认)
xpuPlay --buffer-size 2048        # 2048 samples (更稳定，更高延迟)
xpuPlay --buffer-size 512         # 512 samples (更低延迟)

# 输入 (stdin)
# WAV 格式 PCM 数据

# 输出 (stdout)
# 实时状态 JSON
{"event": "playing", "position": 45.2}
{"event": "finished", "position": 245.8}
{"event": "error", "message": "Device not found"}

# 输出 (stderr)
# 日志信息
```

**网络播放：**

```bash
# HTTP/HTTPS 流媒体
xpuPlay --url http://example.com/stream.flac
xpuPlay --url https://example.com/music.flac

# 网络电台 (ICY/SHOUTcast)
xpuPlay --url http://radio.example.com:8000/stream

# RTSP 流
xpuPlay --url rtsp://server.example.com/stream

# 网络流缓冲设置
xpuPlay --url http://example.com/stream.flac --buffer-size 4096
xpuPlay --url http://example.com/stream.flac --network-buffer 524288
xpuPlay --url http://example.com/stream.flac --reconnect 5

# 网络超时设置
xpuPlay --url http://example.com/stream.flac --timeout 30
xpuPlay --url http://example.com/stream.flac --connect-timeout 10

# 带认证的网络流
xpuPlay --url http://user:pass@example.com/stream.flac
xpuPlay --url http://example.com/stream.flac --auth user:password

# 播放列表文件 (M3U, PLS)
xpuPlay --playlist http://example.com/playlist.m3u
xpuPlay --playlist http://example.com/station.pls --shuffle

# 网络流状态监控
xpuPlay --url http://radio.example.com:8000/stream --status-interval 5
```

**支持的网络协议：**

| 协议 | 格式 | 说明 |
|------|------|------|
| HTTP | FLAC, WAV, ALAC | 标准 HTTP 流媒体 |
| HTTPS | FLAC, WAV, ALAC | 加密 HTTP 流媒体 |
| ICY | MP3, AAC | 网络电台 (SHOUTcast/Icecast) |
| RTSP | 各种 | 实时流协议 |

**网络播放参数：**

```bash
--url <url>                  # 网络流地址
--playlist <url>             # 播放列表 URL
--network-buffer <size>      # 网络缓冲大小（字节），默认 256KB
--reconnect <attempts>       # 断线重连次数，默认 3 次
--timeout <seconds>          # 读取超时（秒），默认 30 秒
--connect-timeout <seconds>  # 连接超时（秒），默认 10 秒
--auth <user:pass>           # HTTP 认证
--user-agent <string>        # 自定义 User-Agent
--status-interval <seconds>  # 状态输出间隔，默认 1 秒
--metadata                   # 解析网络流元数据（如电台信息）
```

**网络播放输出格式：**

```json
{
  "event": "streaming",
  "url": "http://radio.example.com:8000/stream",
  "status": "connected",
  "bitrate": 128000,
  "codec": "MP3",
  "metadata": {
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name"
  }
}

{
  "event": "buffer",
  "percent": 75,
  "bytes": 196608
}

{
  "event": "reconnect",
  "attempt": 2,
  "max_attempts": 5
}
```

**网络电台播放示例：**

```bash
# 播放网络电台
xpuPlay --url http://stream.radio.com:8000/ --metadata

# 播放多个电台（按顺序）
xpuPlay --playlist http://radios.example.com/stations.m3u

# 随机播放播放列表中的电台
xpuPlay --playlist http://radios.example.com/stations.m3u --shuffle

# 监控电台流状态
xpuPlay --url http://radio.example.com:8000/ --status-interval 10

# 断线自动重连
xpuPlay --url http://radio.example.com:8000/ --reconnect 10
```

**设计理念：**

```
┌─────────────────────────────────────────────────────────┐
│                   xpuPlay 播放模式                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  本地播放:    stdin → WAV PCM → 音频设备                │
│                  ↓                                      │
│             实时播放                                     │
│                                                         │
│  网络播放:    URL → HTTP → 解码 → WAV PCM → 音频设备     │
│                  ↓        ↓                             │
│               缓冲      流式解码                         │
│                  ↓                                      │
│            断线重连/元数据                               │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**为什么这样设计：**

1. **统一接口**：本地和网络播放使用相同的模块
2. **流式播放**：支持边下载边播放，无需等待完整下载
3. **智能缓冲**：自动调整缓冲区大小，平衡延迟和稳定性
4. **断线重连**：网络不稳定时自动重连
5. **元数据支持**：解析网络电台的歌曲信息

**典型用法：**

```bash
# 本地文件播放
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify | \
    xpuProcess | xpuOutWave | xpuPlay

# 网络流直接播放（无需 xpuLoad）
xpuPlay --url http://example.com/music.flac

# 网络电台播放
xpuPlay --url http://radio.example.com:8000/stream --metadata --reconnect 10

# 高质量网络流（需要更大缓冲）
xpuPlay --url http://example.com/hires.flac --network-buffer 1048576 --buffer-size 4096
```

### 远程部署模式（边缘播放器）

xpuPlay 可以部署在树莓派等边缘设备上，作为远程播放终端接收服务器发送的音频流。

**流接收模式：**

```bash
# 接收来自 xpuStream 的音频流
xpuPlay --mode receive --url http://192.168.1.10:9000/stream

# WebSocket 连接（支持双向控制）
xpuPlay --mode receive --url ws://192.168.1.10:9000/stream

# TCP 流（最低延迟）
xpuPlay --mode receive --url tcp://192.168.1.10:8080

# 从缓存接收（断点续传）
xpuPlay --mode receive --url http://server:9000/stream --resume
```

**服务器启动（边缘设备）：**

```bash
# 在树莓派上启动 xpuPlay 作为接收服务器
xpuPlay --mode server --port 8080 \
    --name "Living Room Speaker" \
    --device hw:0,0

# 输出设备信息
{
  "player_id": "xpu_player_001",
  "name": "Living Room Speaker",
  "host": "192.168.1.100",
  "port": 8080,
  "protocols": ["http", "websocket", "tcp"],
  "status": "ready"
}

# 服务器端（xpuStream）发送音频
xpuLoad song.flac | xpuIn2Wav | xpuProcess | \
    xpuStream --target http://192.168.1.100:8080/stream
```

**远程播放配置：**

```bash
# 配置文件 /etc/xpu/play.conf
[server]
player_id = xpu_player_001
player_name = Living Room Speaker
bind_host = 0.0.0.0
bind_port = 8080

[audio]
device = hw:0,0
buffer_size = 4096
sample_rate = 96000

[network]
max_bitrate = 1411200  # 16-bit 96kHz stereo
codec_preference = flac,wav
auto_reconnect = true
reconnect_delay = 5

[discovery]
enable_mdns = true
enable_dlna = true
enable_airplay = true
```

**自动发现服务：**

```bash
# 启动 mDNS/Bonjour 服务（设备发现）
xpuPlay --mode server --mdns

# 设备在局域网内自动发现
# macOS:Finder → 网络中显示 "Living Room Speaker"
# Windows:网络中显示设备
# Linux:avahi-browse -a | grep xpu

# 从服务器端发现设备
xpuStream --discover
```

### DLNA/UPnP 播放器模式

xpuPlay 可以作为标准 DLNA/UPnP 媒体渲染器（Media Renderer），被任何 DLNA 控制器发现和控制。

**启动 DLNA 模式：**

```bash
# 启动 DLNA 媒体渲染器
xpuPlay --dlna --name "XPU Player" --port 8080

# 输出:
{
  "dlna_enabled": true,
  "device_name": "XPU Player",
  "device_uuid": "12345678-1234-1234-1234-123456789abc",
  "friendly_name": "XPU Player (192.168.1.100)",
  "services": {
    "avtransport": "http://192.168.1.100:8080/AVTransport",
    "renderingcontrol": "http://192.168.1.100:8080/RenderingControl",
    "connectionmanager": "http://192.168.1.100:8080/ConnectionManager"
  }
}
```

**DLNA 设备发现：**

```bash
# 服务器端发现 DLNA 设备
xpuStream --discover-dlna

# 输出:
{
  "dlna_devices": [
    {
      "name": "Living Room TV",
      "location": "http://192.168.1.100:7676/description.xml",
      "uuid": "...",
      "manufacturer": "Samsung",
      "model_name": "UE55KS7000"
    },
    {
      "name": "XPU Player",
      "location": "http://192.168.1.101:8080/description.xml",
      "uuid": "12345678-1234-1234-1234-123456789abc",
      "manufacturer": "XPU Project"
    }
  ]
}
```

**DLNA 推送播放：**

```bash
# 从 xpuStream 推送到 DLNA 设备
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave | \
    xpuStream --target dlna://XPU+Player --codec flac

# 使用设备名称（空格替换为 +）
xpuStream --target dlna://Living+Room+TV

# 使用设备 UUID
xpuStream --target dlna://uuid:12345678-1234-1234-1234-123456789abc
```

**DLNA 控制协议：**

```cpp
// AVTransport 服务实现
class DlnaAvTransport {
public:
    // SetAVTransportURI - 设置播放 URI
    void setUri(const std::string& uri, const std::string& metadata);

    // Play - 播放
    void play(const std::string& speed = "1");

    // Pause - 暂停
    void pause();

    // Stop - 停止
    void stop();

    // Seek - 跳转
    void seek(const std::string& unit, const std::string& target);

    // Next/Previous - 上下曲
    void next();
    void previous();
};

// RenderingControl 服务实现
class DlnaRenderingControl {
public:
    // SetVolume - 设置音量
    void setVolume(int channel, int volume);

    // GetVolume - 获取音量
    int getVolume(int channel);

    // SetMute - 静音
    void setMute(bool mute);
};
```

**与第三方 DLNA 控制器兼容：**

| 控制器 | 平台 | 兼容性 |
|-------|------|--------|
| BubbleUPnP | Android | 完全兼容 |
| foobar2000 + upnp | Windows | 完全兼容 |
| VLC | 跨平台 | 完全兼容 |
| iOS 音乐 | iOS | 部分兼容 |
| Windows 媒体播放器 | Windows | 部分兼容 |

### AirPlay 播放器模式

xpuPlay 可以作为 AirPlay 接收器（AirPlay Receiver），被 iOS/macOS 设备发现和播放。

**启动 AirPlay 模式：**

```bash
# 启动 AirPlay 接收器（RAOP 协议）
xpuPlay --airplay --name "XPU AirPlay" --port 5000

# 输出:
{
  "airplay_enabled": true,
  "device_name": "XPU AirPlay",
  "device_id": "XX:XX:XX:XX:XX:XX",
  "features": {
    "audio": true,
    "metadata": true,
    "progress": true,
    "volume": true,
    "authentication": false
  },
  "protocols": {
    "raop": "tcp://192.168.1.100:5000",
    "airplay": "http://192.168.1.100:7000"
  }
}
```

**AirPlay 设备发现：**

```bash
# 服务器端发现 AirPlay 设备
xpuStream --discover-airplay

# 输出:
{
  "airplay_devices": [
    {
      "name": "Living Room Speaker",
      "host": "192.168.1.100",
      "port": 7000,
      "device_id": "XX:XX:XX:XX:XX:XX",
      "features": 0x5A7FFFF7,
      "model": "XPU Player"
    },
    {
      "name": "Apple TV",
      "host": "192.168.1.200",
      "port": 7000,
      "device_id": "YY:YY:YY:YY:YY:YY",
      "model": "AppleTV6,2"
    }
  ]
}
```

**AirPlay 推送播放：**

```bash
# 从 xpuStream 推送到 AirPlay 设备
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave | \
    xpuStream --target airplay://XPU+AirPlay --codec wav

# iOS/macOS 原生支持：
# 1. 控制中心 → 屏幕镜像/音频 → 选择 "XPU AirPlay"
# 2. 音乐应用 → 空投图标 → 选择 "XPU AirPlay"
# 3. 系统音频将自动推送到 xpuPlay
```

**AirPlay 加密（FairPlay）：**

```bash
# 启用 AirPlay 加密（需要 FairPlay 密钥，通常仅用于官方认证设备）
xpuPlay --airplay --encryption --key /path/to/private.key

# 无加密模式（默认，用于非认证设备）
xpuPlay --airplay --no-encryption
```

**AirPlay 元数据：**

```bash
# 启用元数据解析（显示封面、标题、艺术家）
xpuPlay --airplay --metadata

# 元数据输出到 D-Bus/MPRIS（Linux）
# 或到 stdout（JSON 格式）
{
  "event": "metadata",
  "title": "Song Title",
  "artist": "Artist Name",
  "album": "Album Name",
  "duration": 245.8,
  "artwork": "base64_encoded_image_data"
}
```

### 多协议统一部署

**同时启动所有协议：**

```bash
# 在树莓派上启动完整的接收服务器
xpuPlay --mode server \
    --name "Living Room Speaker" \
    --port 8080 \
    --dlna \
    --airplay \
    --mdns \
    --device hw:0,0

# 同时支持：
# 1. xpuStream 推送（HTTP/WebSocket/TCP）
# 2. DLNA 控制器控制
# 3. AirPlay 设备推送
# 4. mDNS/Bonjour 自动发现
```

**协议优先级：**

```bash
# 配置协议优先级
xpuPlay --mode server \
    --protocol-priority airplay,dlna,xpustream \
    --fallback any

# 行为：
# 1. 优先接受 AirPlay 连接（来自 iOS/macOS）
# 2. 其次接受 DLNA 连接（来自 Android/PC）
# 3. 最后接受 xpuStream 连接（来自 XPU 服务器）
# 4. 如果所有协议都不可用，回退到任何可用协议
```

**并发连接：**

```bash
# 支持多个并发音频流（混音）
xpuPlay --mode server --mixer enable --max-streams 3

# 场景：
# - 流1：背景音乐（xpuStream）
# - 流2：通知音效（xpuStream）
# - 流3：电话铃声（AirPlay）

# 混音配置
xpuPlay --mixer enable \
    --mixer-mode sum \
    --normalization true
```

### 树莓派/香橙派部署指南

**硬件支持：**

| 硬件 | CPU | 推荐场景 |
|------|-----|---------|
| 树莓派 5 | 2.4GHz 四核 ARM Cortex-A76 | 最佳性能，支持 DSP 处理 |
| 香橙派 5 (RK3588) | 2.4GHz 八核 ARM Cortex-A76/A53 | 最佳性能，支持 DSP 处理 |

**安装（树莓派 OS / Orange Pi OS）：**

```bash
# 1. 更新系统
sudo apt update && sudo apt upgrade -y

# 2. 安装依赖
sudo apt install -y \
    libasound2-dev \
    libavahi-client-dev \
    libplist-dev \
    libssl-dev \
    libcurl4-openssl-dev

# 3. 安装 XPU
wget https://github.com/xpu-music/xpu/releases/latest/xpu-arm64.deb
sudo dpkg -i xpu-arm64.deb

# 4. 配置音频输出
# 树莓派 5:
sudo raspi-config
# 选择: Advanced Options → Audio → 选择 HDMI 或 3.5mm Jack

# 香橙派 5:
# 通常默认启用 HDMI 音频输出
# 可通过 alsamixer 配置

# 5. 启用服务
sudo systemctl enable xpu-player
sudo systemctl start xpu-player

# 6. 检查状态
sudo systemctl status xpu-player
xpuPlay --status
```

**systemd 服务配置：**

```ini
# /etc/systemd/system/xpu-player.service
[Unit]
Description=XPU Player Service
After=network.target sound.target

[Service]
Type=simple
User=xpu
Group=audio
ExecStart=/usr/bin/xpuPlay \
    --mode server \
    --config /etc/xpu/play.conf \
    --dlna \
    --airplay \
    --mdns
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**性能优化：**

```bash
# 树莓派 5 优化
# 1. 启用 64 位模式
# 2. 超频（谨慎）
sudo raspi-config
# Performance Options → GPU Memory → 16MB
# Performance Options → Overclock → Pi5: Medium (3.0GHz)

# 3. CPU 性能模式
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 4. 音频缓冲优化
xpuPlay --mode server --buffer-size 8192 --network-buffer 524288

# 香橙派 5 (RK3588) 优化
# 1. CPU 性能模式
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 2. NPU 加速（可选，用于 DSP 处理）
# RK3588 内置 6 TOPS NPU，可加速音频处理
xpuPlay --mode server --npure 6

# 3. 音频缓冲优化
xpuPlay --mode server --buffer-size 8192 --network-buffer 524288
```

**音频输出配置：**

```bash
# HDMI 输出（推荐）
xpuPlay --device hw:0,0

# 3.5mm 模拟输出
xpuPlay --device hw:1,0

# USB DAC
xpuPlay --device hw:2,0

# 查看可用设备
aplay -l
xpuPlay --list-devices
```

**网络配置：**

```bash
# 有线网络（推荐）
# 自动配置，无需额外设置

# Wi-Fi 配置
sudo raspi-config
# Localisation Options → Wireless LAN

# 固定 IP（推荐用于服务器）
sudo nano /etc/dhcpcd.conf
# 添加：
interface eth0
static ip_address=192.168.1.100/24
static routers=192.168.1.1
static domain_name_servers=192.168.1.1

# 测试网络
ping 192.168.1.1
```

### 设备发现与配对

**mDNS/Bonjour 服务：**

```bash
# 启动 mDNS 宣告
xpuPlay --mode server --mdns

# avahi 服务配置（Linux）
# /etc/avahi/services/xpu-player.service
<?xml version="1.0" standalone='no'?>
<service-group>
  <name replace-wildcards="yes">XPU Player (%h)</name>
  <service>
    <type>_xpu-stream._tcp</type>
    <port>8080</port>
    <txt-record>protocols=http,websocket,tcp</txt-record>
    <txt-record>codecs=wav,flac</txt-record>
  </service>
  <service>
    <type>_raop._tcp</type>
    <port>5000</port>
    <txt-record>features=0x5A7FFFF7</txt-record>
  </service>
</service-group>

# 重启 avahi
sudo systemctl restart avahi-daemon
```

**设备配对：**

```bash
# 自动配对（零配置）
xpuStream --discover
xpuStream --target xpu_player_001

# 手动配对
xpuPlay --pair --pin 1234  # 显示 PIN 码
xpuStream --pair 1234       # 输入 PIN 码

# 取消配对
xpuPlay --unpair xpu_player_001
```

#### 3.2.9.1 xpuPlayCtrl (播放控制)

xpuPlayCtrl 是 xpuPlay 的配套控制工具，通过命名管道发送控制命令到正在运行的 xpuPlay 实例。

**控制管道命名规范：**

```
/tmp/xpu_play_<pid>_control    # 单实例控制管道
/tmp/xpu_play_<session_id>_control  # 会话级控制管道
```

**命名规则：**
- `<pid>`: xpuPlay 进程的 PID
- `<session_id>`: 可选的会话标识符（用于多实例管理）
- 格式: `xpu_play_{{pid}}_control` 或 `xpu_play_{{session_id}}_control`

**基本用法：**

```bash
# 方法1：使用 PID 控制（推荐用于单实例）
xpuPlay &
PLAY_PID=$!
xpuPlayCtrl --pid $PLAY_PID pause
xpuPlayCtrl --pid $PLAY_PID resume
xpuPlayCtrl --pid $PLAY_PID stop

# 方法2：使用会话 ID（推荐用于多实例）
xpuPlay --session-id my_session &
xpuPlayCtrl --session my_session pause
xpuPlayCtrl --session my_session volume 0.8
xpuPlayCtrl --session my_session seek 60

# 方法3：使用管道文件直接控制
xpuPlay &
CONTROL_PIPE="/tmp/xpu_play_$!_control"
echo "pause" > $CONTROL_PIPE
echo "volume:0.8" > $CONTROL_PIPE
```

**控制命令格式：**

```bash
# 播放控制
xpuPlayCtrl <instance> pause                # 暂停
xpuPlayCtrl <instance> resume               # 恢复
xpuPlayCtrl <instance> stop                 # 停止
xpuPlayCtrl <instance> seek <seconds>       # 跳转
xpuPlayCtrl <instance> restart              # 重新开始

# 音量控制
xpuPlayCtrl <instance> volume <0.0-1.0>     # 设置音量
xpuPlayCtrl <instance> volume-up <0.1>      # 增加音量
xpuPlayCtrl <instance> volume-down <0.1>    # 减少音音
xpuPlayCtrl <instance> mute                 # 静音
xpuPlayCtrl <instance> unmute               # 取消静音

# 状态查询
xpuPlayCtrl <instance> status               # 获取状态
xpuPlayCtrl <instance> position             # 获取当前位置
xpuPlayCtrl <instance> duration             # 获取总时长

# 设备控制
xpuPlayCtrl <instance> device <name>        # 切换输出设备
xpuPlayCtrl <instance> list-devices         # 列出可用设备
```

**命令格式规范：**

```
格式: <command>[:<args>]

示例:
pause                    # 暂停
resume                   # 恢复
volume:0.8               # 设置音量 0.8
seek:120                 # 跳转到 120 秒
device:hw:0,0            # 切换到设备 hw:0,0
status                   # 查询状态
```

**多实例管理：**

```bash
# 启动多个播放实例
xpuPlay --session-id player1 song1.flac &
xpuPlay --session-id player2 song2.flac &

# 分别控制
xpuPlayCtrl --session player1 pause
xpuPlayCtrl --session player2 volume 0.5

# 列出所有活动实例
xpuPlayCtrl --list
# 输出:
# {
#   "instances": [
#     {"session_id": "player1", "pid": 12345, "status": "paused"},
#     {"session_id": "player2", "pid": 12346, "status": "playing"}
#   ]
# }
```

**管道断裂检测：**

```cpp
// xpuPlay 中的管道监控
class ControlPipeMonitor {
public:
    void startMonitoring(const std::string& pipe_path) {
        // 1. 创建管道（如果不存在）
        mkfifo(pipe_path.c_str(), 0660);

        // 2. 以非阻塞模式打开管道
        int fd = open(pipe_path.c_str(), O_RDONLY | O_NONBLOCK);

        // 3. 启动监控线程
        monitor_thread_ = std::thread([this, fd]() {
            char buffer[256];
            while (running_) {
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0) {
                    // 处理命令
                    handleCommand(std::string(buffer, n));
                } else if (n == 0) {
                    // 管道写入端关闭
                    if (!hasWriter(fd)) {
                        // 检测到管道断裂
                        onPipeBroken();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    // 错误处理
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        onError(errno);
                    }
                }
            }
        });
    }

private:
    bool hasWriter(int fd) {
        // 使用 poll 检查管道是否有写入端
        struct pollfd pfd = {fd, POLLIN | POLLHUP, 0};
        return poll(&pfd, 1, 0) > 0 && !(pfd.revents & POLLHUP);
    }

    void onPipeBroken() {
        // 管道断裂处理：
        // 1. 记录日志
        logWarning("Control pipe broken, recreating...");

        // 2. 尝试重新创建管道
        // 3. 如果连续失败多次，进入静默模式
    }

    std::thread monitor_thread_;
    std::atomic<bool> running_{true};
};
```

**管道清理机制：**

```bash
# xpuPlay 退出时自动清理管道
# 使用 trap 确保清理
cleanup() {
    rm -f /tmp/xpu_play_$$*_control
}
trap cleanup EXIT TERM INT

# xpuPlayCtrl 检测管道是否存在
if [ ! -p "$CONTROL_PIPE" ]; then
    echo "Error: Control pipe not found" >&2
    echo "Target process may not be running" >&2
    exit 1
fi
```

**心跳机制：**

```bash
# xpuPlay 定期发送心跳到状态文件
echo "heartbeat:$PID:$(date +%s)" > /tmp/xpu_play_$$_status

# xpuPlayCtrl 检查心跳
xpuPlayCtrl --session my_session check
# 输出:
# {
#   "alive": true,
#   "last_heartbeat": 1704556800,
#   "uptime": 120
# }
```

**权限控制：**

```bash
# 管道文件权限设置为 660（用户和组可读写）
# 防止其他用户控制播放

# 可通过配置允许组共享
xpuPlay --control-group audio
# 管道组设置为 audio，组成员都可以控制
```

#### 3.2.10 xpuQueue (队列管理)

```bash
# 添加歌曲
xpuQueue add <file> [--position <index>]

# 输出
{"success": true, "queue_id": "uuid", "position": 5}

# 列出队列
xpuQueue list

# 输出
{
  "items": [
    {"id": "uuid1", "file": "/path/to/song1.flac", "metadata": {...}},
    {"id": "uuid2", "file": "/path/to/song2.flac", "metadata": {...}}
  ],
  "current": 0
}

# 移除歌曲
xpuQueue remove <index>

# 清空队列
xpuQueue clear
```

#### 3.2.11 xpuMeta (元数据管理)

xpuMeta 是音频元数据管理模块，负责读取、编辑和搜索音频文件的元数据信息。**这是系统中唯一的搜索模块**。

**基本用法：**

```bash
# 读取元数据
xpuMeta read <file>

# 编辑元数据
xpuMeta edit <file> --set "artist=New Artist"
xpuMeta edit <file> --set "title=New Title" --set "album=New Album"

# 批量编辑
xpuMeta edit ~/Music/*.flac --set "genre=Rock"

# 删除元数据字段
xpuMeta edit <file> --delete "comment"
```

**搜索功能（系统唯一搜索模块）：**

```bash
# 基本搜索
xpuMeta search "Queen" --in ~/Music

# 高级搜索语法
xpuMeta search "artist:Queen" --in ~/Music
xpuMeta search "genre:Rock year:>2000" --in ~/Music
xpuMeta search "bpm:>120 mood:energetic" --in ~/Music

# 组合条件
xpuMeta search "artist:Queen OR artist:Freddie" --in ~/Music
xpuMeta search "genre:Rock year:1990-1999" --in ~/Music

# 输出格式
xpuMeta search "genre:Rock" --format json
xpuMeta search "genre:Rock" --format plain
```

**搜索语法说明：**

| 语法 | 说明 | 示例 |
|------|------|------|
| `field:value` | 字段精确匹配 | `artist:Queen` |
| `field:>value` | 大于 | `year:>2000` |
| `field:<value` | 小于 | `bpm:<100` |
| `field:min-max` | 范围 | `year:1990-2000` |
| `value1 OR value2` | 或逻辑 | `Rock OR Jazz` |
| `value1 AND value2` | 与逻辑（默认） | `Rock Fast` |

**支持的字段：**

| 字段 | 说明 | 示例值 |
|------|------|--------|
| artist | 艺术家 | `Queen`, `Pink Floyd` |
| album | 专辑 | `The Dark Side of the Moon` |
| title | 标题 | `Bohemian Rhapsody` |
| genre | 流派 | `Rock`, `Jazz`, `Classical` |
| year | 年份 | `1975`, `>2000`, `1990-2000` |
| bpm | BPM | `>120`, `<80` |
| key | 调性 | `A minor`, `C major` |
| mood | 情绪 | `energetic`, `relaxed` |
| duration | 时长（秒） | `>180` (3分钟以上) |

**搜索输出格式：**

```json
{
  "query": "genre:Rock year:>2000",
  "total_matches": 25,
  "results": [
    {
      "file": "/path/to/song1.flac",
      "metadata": {
        "artist": "Queen",
        "title": "Bohemian Rhapsody",
        "album": "A Night at the Opera",
        "year": 1975,
        "genre": "Rock"
      },
      "match_score": 0.95
    },
    {
      "file": "/path/to/song2.flac",
      "metadata": {
        "artist": "Pink Floyd",
        "title": "Comfortably Numb",
        "album": "The Wall",
        "year": 1979,
        "genre": "Rock"
      },
      "match_score": 0.92
    }
  ],
  "search_time_ms": 45
}
```

**典型用法：**

```bash
# 创建播放列表（使用搜索结果）
xpuMeta search "genre:Rock year:>2000" --in ~/Music | \
    xpuPlaylist create --name "2000年代摇滚"

# 批量播放
xpuMeta search "mood:energetic" --in ~/Music | \
    xargs -I {} xpuPlay "{}"

# 统计信息
xpuMeta search "genre:*" --in ~/Music --stats
# 输出各流派数量

# 查找重复
xpuMeta search "artist:Queen album:*" --in ~/Music --find-duplicates
```

**设计理念：**

```
┌─────────────────────────────────────────────────────────────┐
│                    xpuMeta 搜索架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  用户查询 → 解析语法 → 元数据索引 → 匹配 → 排序 → 返回结果    │
│                 ↓                                           │
│            SQLite 全文搜索 (FTS5)                           │
│                                                             │
│  支持的字段：artist, album, title, genre, year, bpm, key   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**为什么 xpuMeta 是唯一的搜索模块：**

1. **职责明确**：xpuMeta 负责所有元数据操作（读取、编辑、搜索）
2. **避免混淆**：单一搜索入口，语法统一
3. **性能优化**：统一的元数据索引，一次建立，多处使用
4. **维护简单**：搜索逻辑集中在一个模块

**注意**：系统中**没有 xpuSearch 模块**，所有搜索功能通过 xpuMeta 实现。

#### 3.2.12 xpuCache (缓存管理)

统一管理各类缓存（FFT、可视化、指纹等），提供索引、查询、清理功能。

```bash
# 列出所有缓存
xpuCache list

# 输出
{
  "caches": [
    {
      "cache_id": "a1b2c3d4e5f6",
      "type": "fft",
      "created_at": "2025-01-07T10:00:00Z",
      "size_bytes": 188000000,
      "access_count": 5,
      "last_accessed": "2025-01-07T15:30:00Z"
    },
    {
      "cache_id": "b2c3d4e5f6a",
      "type": "visual",
      "created_at": "2025-01-06T14:00:00Z",
      "size_bytes": 45000000,
      "access_count": 1,
      "last_accessed": "2025-01-06T14:00:00Z"
    }
  ],
  "total_size_bytes": 233000000,
  "total_count": 2
}

# 查看特定缓存详情
xpuCache stat <cache_id>

# 输出
{
  "cache_id": "a1b2c3d4e5f6",
  "type": "fft",
  "path": "/home/user/.cache/xpu/fft/a1b2c3d4e5f6",
  "created_at": "2025-01-07T10:00:00Z",
  "size_bytes": 188000000,
  "files": ["magnitude.bin", "phase.bin", "meta.json", "config.json"],
  "source": {
    "file": "/home/user/Music/song.flac",
    "hash": "sha256:abc123...",
    "duration": 245.8
  },
  "access_count": 5,
  "last_accessed": "2025-01-07T15:30:00Z"
}

# 清理过期缓存
xpuCache gc --keep 7d                    # 保留7天内的缓存
xpuCache gc --max-size 5G                # 限制总大小为5GB
xpuCache gc --min-access 3               # 删除访问次数少于3次的缓存

# 批量失效缓存
xpuCache invalidate --pattern "*.flac"   # 失效所有FLAC相关缓存
xpuCache invalidate --before 2025-01-01  # 失效指定日期前的缓存

# 清空所有缓存
xpuCache clear --all

# 仅清空特定类型缓存
xpuCache clear --type fft
xpuCache clear --type visual

# 导出/导入缓存索引（用于备份或迁移）
xpuCache export --output cache_index.json
xpuCache import --input cache_index.json
```

**缓存索引结构：**

缓存索引存储在 `~/.cache/xpu/index.db`（SQLite），包含以下字段：

```sql
CREATE TABLE caches (
  cache_id TEXT PRIMARY KEY,
  type TEXT NOT NULL,              -- 'fft', 'visual', 'fingerprint'
  path TEXT NOT NULL,
  created_at TIMESTAMP NOT NULL,
  size_bytes INTEGER NOT NULL,
  access_count INTEGER DEFAULT 0,
  last_accessed TIMESTAMP,
  source_file TEXT,
  source_hash TEXT,
  metadata JSON
);

CREATE INDEX idx_type ON caches(type);
CREATE INDEX idx_created ON caches(created_at);
CREATE INDEX idx_accessed ON caches(last_accessed);
```

**自动清理策略：**

可通过 `~/.config/xpu/cache.conf` 配置自动清理：

```toml
[gc]
# 启用自动垃圾回收
auto_gc = true

# 每天凌晨3点执行
schedule = "0 3 * * *"

# 保留策略（满足任一条件即保留）
keep_min_count = 100               # 至少保留100个缓存
keep_max_size = "10G"              # 总大小不超过10GB
keep_max_age = "30d"               # 不保留超过30天的缓存
keep_min_access = 2                # 访问次数至少2次

# 优先保留策略（LRU优先级）
prioritize_accessed = true         # 优先保留最近访问的
prioritize_type = ["fft", "visual", "fingerprint"]
```

#### 3.2.13 xpuDevice (设备管理)

```bash
# 列出设备
xpuDevice list

# 输出
{
  "devices": [
    {
      "id": "0",
      "name": "Default Audio Device",
      "driver": "wasapi",
      "sample_rates": [44100, 48000, 96000],
      "is_default": true
    }
  ]
}

# 测试设备
xpuDevice test <device_id>

# 输出
{"success": true, "latency_ms": 23}
```

#### 3.2.14 xpuPlaylist (播放列表管理)

播放列表文件管理模块，支持多种格式的导入导出。

```bash
# 创建播放列表
xpuPlaylist create --name "我的摇滚收藏" --output rock_playlist.m3u

# 添加文件到播放列表
xpuPlaylist add rock_playlist.m3u ~/Music/rock/*.flac

# 从搜索结果创建播放列表（使用 xpuMeta 搜索）
xpuMeta search "genre:rock year:>2000" --in ~/Music | \
    xpuPlaylist create --name "2000年代摇滚"

# 导入播放列表（支持多种格式）
xpuPlaylist import --input playlist.m3u
xpuPlaylist import --input playlist.m3u8
xpuPlaylist import --input playlist.pls
xpuPlaylist import --input playlist.xspf
xpuPlaylist import --input playlist.asx

# 导出播放列表
xpuPlaylist export --input my_playlist.json --output rock_playlist.m3u
xpuPlaylist export --input my_playlist.json --output rock_playlist.m3u8
xpuPlaylist export --input my_playlist.json --output rock_playlist.pls

# 列出播放列表
xpuPlaylist list

# 查看播放列表内容
xpuPlaylist show rock_playlist.m3u

# 输出
{
  "name": "rock_playlist",
  "format": "m3u",
  "track_count": 25,
  "total_duration": 5847.2,
  "tracks": [
    {
      "file": "/home/user/Music/song1.flac",
      "title": "Song Title",
      "artist": "Artist Name",
      "album": "Album Name",
      "duration": 245.8
    },
    ...
  ]
}

# 验证播放列表（检查文件是否存在）
xpuPlaylist validate rock_playlist.m3u

# 输出
{
  "valid": true,
  "total_tracks": 25,
  "missing_files": 2,
  "missing": [
    "/home/user/Music/missing_song.flac",
    "/home/user/Music/another_missing.flac"
  ]
}

# 自动组织播放列表（基于元数据规则）
xpuPlaylist auto-organize --rule "genre:rock" --output rock.m3u
xpuPlaylist auto-organize --rule "mood:energetic activity:workout" --output workout.m3u
xpuPlaylist auto-organize --rule "year:1990-1999 genre:pop" --output 90s_pop.m3u

# 合并播放列表
xpuPlaylist merge playlist1.m3u playlist2.m3u --output merged.m3u

# 播放列表排序
xpuPlaylist sort rock_playlist.m3u --by artist --output rock_sorted.m3u
xpuPlaylist sort rock_playlist.m3u --by title --shuffle --output rock_random.m3u

# 去重（基于指纹）
xpuPlaylist dedup large_playlist.m3u --output unique_playlist.m3u

# 转换播放列表格式
xpuPlaylist convert --input playlist.m3u --output playlist.xspf
```

**支持的播放列表格式：**

| 格式 | 扩展名 | 说明 |
|------|--------|------|
| M3U | .m3u | 基础播放列表格式 |
| M3U8 | .m3u8 | UTF-8编码的M3U |
| PLS | .pls | SHOUTcast播放列表格式 |
| XSPF | .xspf | XML Shareable Playlist Format |
| ASX | .asx | Advanced Stream Redirector |
| JSON | .json | XPU原生播放列表格式 |

**XPU JSON播放列表格式：**

```json
{
  "name": "我的摇滚收藏",
  "description": "精选摇滚音乐",
  "created_at": "2025-01-07T10:00:00Z",
  "modified_at": "2025-01-07T15:30:00Z",
  "tracks": [
    {
      "file": "/home/user/Music/song1.flac",
      "title": "Bohemian Rhapsody",
      "artist": "Queen",
      "album": "A Night at the Opera",
      "genre": "Rock",
      "year": 1975,
      "duration": 354.5,
      "track_number": 1
    },
    {
      "file": "/home/user/Music/song2.flac",
      "title": "Stairway to Heaven",
      "artist": "Led Zeppelin",
      "album": "Led Zeppelin IV",
      "genre": "Rock",
      "year": 1971,
      "duration": 482.0,
      "track_number": 2
    }
  ],
  "metadata": {
    "total_duration": 836.5,
    "total_tracks": 2,
    "genres": ["Rock"],
    "year_range": "1971-1975"
  }
}
```

**播放列表自动组织规则：**

```bash
# 语法：field:pattern
# 支持的字段：genre, mood, activity, era, artist, album, year

# 单条件
xpuPlaylist auto-organize --rule "genre:rock"

# 多条件（AND）
xpuPlaylist auto-organize --rule "genre:rock year:>2000"

# 多条件（OR）
xpuPlaylist auto-organize --rule "genre:rock|metal"

# 范围
xpuPlaylist auto-organize --rule "year:1990-1999"
xpuPlaylist auto-organize --rule "year:>2010"

# 组合
xpuPlaylist auto-organize --rule "genre:rock mood:energetic activity:workout"
```

**播放列表导入示例：**

```bash
# 从iTunes导出的播放列表导入
xpuPlaylist import --input ~/Music/iTunes/Library.xml --format itunes

# 从foobar2000播放列表导入
xpuPlaylist import --input ~/playlists/foobar_playlist.fpl

# 从Spotify导出（需要第三方工具）
# 1. 使用spotify-playlist-export导出为JSON
# 2. 导入到XPU
xpuPlaylist import --input spotify_export.json

# 从网络播放列表导入
xpuPlaylist import --url "https://example.com/playlist.m3u"

# 批量导入
for file in ~/old_player/playlists/*.pls; do
    xpuPlaylist import --input "$file"
done
```

**播放列表与队列集成：**

```bash
# 从播放列表创建播放队列
xpuPlaylist show my_rock.m3u | jq -r '.tracks[].file' | \
    xargs -I {} xpuQueue add --name "$(basename {})" --command "xpuLoad '{}' | xpuPlay"

# 播放整个播放列表
xpuPlaylist show my_rock.m3u | jq -r '.tracks[].file' | \
    while read file; do
        xpuLoad "$file" | xpuIn2Wav | xpuPlay
    done
```

### 3.3 组合使用示例

```bash
# 简单播放（完整管道）
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify | xpuVisualize | xpuProcess | xpuOutWave | xpuPlay

# 带音量和均衡器
xpuLoad song.flac | xpuIn2Wav | \
    xpuFingerprint | xpuClassify | \
    xpuProcess --volume 0.8 --eq rock | \
    xpuOutWave --auto | \
    xpuPlay

# 高解析度播放 (192kHz/32-bit)
xpuLoad song.flac | xpuIn2Wav --rate 192000 --depth 32 | \
    xpuProcess --volume 0.9 | \
    xpuOutWave | \
    xpuPlay

# 耳机优化 + 3D 效果
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --eq vocal --3d headphone | \
    xpuOutWave --auto | \
    xpuPlay

# 播放列表
for file in *.flac; do
    xpuLoad "$file" | xpuIn2Wav | xpuProcess | xpuOutWave | xpuPlay
done

# 格式转换：FLAC → FLAC（重新编码到更高采样率）
xpuLoad song.flac | xpuIn2Wav --rate 192000 | \
    xpuProcess --compress 2:1 | \
    xpuOutWave --format flac --compression 0 > output.flac

# 格式转换：DSD → WAV
xpuLoad song.dsf | xpuIn2Wav --rate 384000 --depth 32 | \
    xpuOutWave --format wav > output.wav

# 淡入淡出处理
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --fade-in 2000 --fade-out 3000 | \
    xpuOutWave | xpuPlay

# 变速不变调（1.5倍速）
xpuLoad podcast.flac | xpuIn2Wav | \
    xpuProcess --speed 1.5 --preserve-pitch | \
    xpuOutWave --auto | xpuPlay

# === 指纹和分类相关示例 ===

# 生成音频指纹
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint

# 音乐分类（流派、情绪、活动）
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all

# 只分析流派
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension genre

# 只分析情绪
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension mood

# === FFT 缓存优化示例 ===

# 获取 FFT 缓存 ID
CACHE_ID=$(xpuLoad song.flac | xpuIn2Wav 2>&1 | jq -r '.cache_id')

# 使用 FFT 缓存进行指纹识别（避免重复 FFT 计算）
xpuFingerprint --fft-cache $CACHE_ID

# 使用 FFT 缓存进行分类（更快）
xpuClassify --fft-cache $CACHE_ID --dimension all

# 使用 FFT 缓存进行频域处理（均衡器等）
xpuProcess --fft-cache $CACHE_ID --eq rock --compress 2:1

# 完整优化管道（复用 FFT 缓存）
xpuLoad song.flac | xpuIn2Wav > /dev/null  # 生成 WAV 并缓存 FFT
CACHE_ID=$(xpuLoad song.flac | xpuIn2Wav 2>&1 | jq -r '.cache_id')
xpuFingerprint --fft-cache $CACHE_ID
xpuClassify --fft-cache $CACHE_ID --dimension all
xpuProcess --fft-cache $CACHE_ID --eq rock | \
    xpuOutWave --auto | xpuPlay

# 批量处理（每个文件只计算一次 FFT）
for file in ~/Music/*.flac; do
    CACHE_ID=$(xpuLoad "$file" | xpuIn2Wav 2>&1 | jq -r '.cache_id')
    xpuFingerprint --fft-cache $CACHE_ID
    xpuClassify --fft-cache $CACHE_ID --dimension all
    xpuProcess --fft-cache $CACHE_ID --eq flat
done

# 重复检测（比较两个文件）
xpuLoad song1.flac | xpuIn2Wav | xpuFingerprint > fp1.json
xpuLoad song2.flac | xpuIn2Wav | xpuFingerprint > fp2.json
xpuMatch fp1.json fp2.json

# 批量分析音乐库（使用 FFT 缓存优化）
for file in ~/Music/**/*.flac; do
    CACHE_ID=$(xpuLoad "$file" | xpuIn2Wav 2>&1 | jq -r '.cache_id')
    xpuFingerprint --fft-cache $CACHE_ID
    xpuClassify --fft-cache $CACHE_ID --dimension all
done > music_library_analysis.json

# 基于情绪自动播放列表
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | \
    xpuClassify --dimension mood | \
    xpuPlaylist --filter mood=Energetic | \
    xpuPlay

# 查询在线音乐数据库
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | \
    xpuQuery --api acousticbrainz --format json

# === 网络播放示例 ===

# 直接播放网络流
xpuPlay --url http://example.com/music.flac

# 播放网络电台（带元数据）
xpuPlay --url http://radio.example.com:8000/stream --metadata

# 播放网络电台播放列表
xpuPlay --playlist http://radios.example.com/stations.m3u

# 随机播放电台列表
xpuPlay --playlist http://radios.example.com/stations.m3u --shuffle

# 高质量网络流（需要更大缓冲）
xpuPlay --url http://example.com/hires.flac \
    --network-buffer 1048576 \
    --buffer-size 4096

# 网络电台（自动重连）
xpuPlay --url http://radio.example.com:8000/stream \
    --metadata \
    --reconnect 10 \
    --status-interval 5

# 带认证的网络流
xpuPlay --url http://example.com/stream.flac \
    --auth username:password \
    --timeout 60

# 网络流 + DSP 处理（使用命名管道）
mkfifo /tmp/audio_pipe
xpuPlay --url http://example.com/music.flac --output /tmp/audio_pipe &
xpuProcess --volume 0.8 --eq rock < /tmp/audio.pipe | \
    xpuOutWave --auto | xpuPlay

# HTTPS 加密流
xpuPlay --url https://secure.example.com/stream.flac

# === 可视化相关示例 ===

# 生成可视化数据
xpuLoad song.flac | xpuIn2Wav | xpuVisualize

# 生成完整可视化数据（波形+频谱）
xpuLoad song.flac | xpuIn2Wav | xpuVisualize --type all

# 只生成波形数据
xpuLoad song.flac | xpuIn2Wav | xpuVisualize --type waveform

# 只生成频谱数据
xpuLoad song.flac | xpuIn2Wav | xpuVisualize --type spectrum

# 生成概览模式（极低分辨率，快速）
xpuLoad song.flac | xpuIn2Wav | xpuVisualize --overview

# 完整管道（包含可视化）
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify | \
    xpuVisualize --cache-dir ~/.cache/xpu/vis | \
    xpuProcess | xpuOutWave | xpuPlay

# 后台生成可视化（不阻塞播放）
xpuLoad song.flac | xpuIn2Wav | xpuVisualize &
CACHE_ID=$(xpuLoad song.flac | xpuIn2Wav | xpuVisualize | jq -r '.cache_id')
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave | xpuPlay

# 读取完整波形数据
xpuVisualize --read a1b2c3d4e5f6 --type waveform

# 读取部分波形数据（10-20秒范围）
xpuVisualize --read a1b2c3d4e5f6 --type waveform --start 10 --end 20

# 读取降采样数据（降低解析度）
xpuVisualize --read a1b2c3d4e5f6 --type waveform --downsample 10

# 读取概览数据（缩略图级别）
xpuVisualize --read a1b2c3d4e5f6 --overview

# 读取频谱数据
xpuVisualize --read a1b2c3d4e5f6 --type spectrum

# 读取特定时间段的频谱
xpuVisualize --read a1b2c3d4e5f6 --type spectrum --start 30 --end 60

# 输出为图像
xpuVisualize --read a1b2c3d4e5f6 --overview --output image \
    --width 800 --height 200 > waveform.png

# 批量生成可视化数据
for file in ~/Music/*.flac; do
    xpuLoad "$file" | xpuIn2Wav | xpuVisualize --cache-dir ~/.cache/xpu/vis
done

# 缓存管理
xpuVisualize --cache-list
xpuVisualize --cache-stats
xpuVisualize --cache-delete a1b2c3d4e5f6
xpuVisualize --cache-clear
```

### 3.4 管道优化建议

```bash
# 推荐：让 xpuOutWave 自动检测设备最佳格式
xpuLoad song.flac | xpuIn2Wav | xpuProcess | xpuOutWave --auto | xpuPlay

# 顶级音质：32-bit 384kHz + 无压缩
xpuLoad song.flac | xpuIn2Wav --rate 384000 --depth 32 | \
    xpuProcess --volume 0.8 --eq flat | \
    xpuOutWave --format wav --dither none | xpuPlay

# 高端音质：24-bit 192kHz + FLAC 压缩
xpuLoad song.flac | xpuIn2Wav --rate 192000 --depth 24 | \
    xpuProcess --volume 0.8 | \
    xpuOutWave --format flac --compression 0 | xpuPlay

# 标准音质：24-bit 96kHz
xpuLoad song.flac | xpuIn2Wav --rate 96000 --depth 24 | \
    xpuProcess | xpuOutWave | xpuPlay

# 低延迟：降低缓冲
xpuLoad song.flac | xpuIn2Wav --rate 44100 --depth 16 | \
    xpuProcess | xpuOutWave | xpuPlay --buffer-size 512
```

### 3.5 新功能使用示例

#### 3.5.1 缓存管理示例

```bash
# === 缓存管理 ===

# 查看所有缓存
xpuCache list

# 查看缓存统计
xpuCache stat a1b2c3d4e5f6

# 清理7天前的缓存
xpuCache gc --keep 7d

# 限制缓存总大小为5GB
xpuCache gc --max-size 5G

# 清除访问次数少于3次的缓存
xpuCache gc --min-access 3

# 清空所有FFT缓存
xpuCache clear --type fft

# 导出缓存索引（备份）
xpuCache export --output cache_backup.json

# 导入缓存索引（迁移）
xpuCache import --input cache_backup.json

# === 缓存优化工作流 ===

# 批量生成缓存（一次性FFT计算，多次复用）
for file in ~/Music/*.flac; do
    echo "处理: $file"
    CACHE_ID=$(xpuLoad "$file" | xpuIn2Wav --fft-cache 2>&1 | jq -r '.cache_id')
    echo "  Cache ID: $CACHE_ID"

    # 复用缓存进行多项分析
    xpuFingerprint --fft-cache $CACHE_ID > "fingerprints/$(basename "$file").json"
    xpuClassify --fft-cache $CACHE_ID --dimension all > "classifications/$(basename "$file").json"
done

# 定期清理（cron任务）
# 0 3 * * * xpuCache gc --keep 7d --max-size 10G
```

#### 3.5.2 任务队列示例

```bash
# === 批量处理音乐库 ===

# 创建任务列表
cat > music_library_tasks.json <<EOF
{
  "tasks": [
    $(for file in ~/Music/*.flac; do
        cat <<TASK
    {
      "name": "处理 $(basename "$file")",
      "command": "xpuLoad '$file' | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all",
      "priority": "normal",
      "metadata": {"file": "$file"}
    },
TASK
      done | sed '$ s/,$//')
  ]
}
EOF

# 批量添加任务
xpuQueue add --batch music_library_tasks.json

# 实时监控任务进度
watch -n 2 'xpuQueue list | jq -r ".summary"'

# 获取失败任务
xpuQueue list --status failed

# 重试失败任务
FAILED_TASKS=$(xpuQueue list --status failed | jq -r '.tasks[].task_id')
for task_id in $FAILED_TASKS; do
    xpuQueue remove $task_id
done
xpuQueue add --batch music_library_tasks.json

# === 并发生成FFT缓存 ===

# 高优先级：生成FFT缓存
for file in ~/Music/*.flac; do
    xpuQueue add --name "FFT缓存: $(basename "$file")" \
        --command "xpuLoad '$file' | xpuIn2Wav --fft-cache > /dev/null" \
        --priority high \
        --metadata {"file": "$file", "stage": "fft_cache"}
done

# 等待所有FFT缓存完成
while xpuQueue list | jq -e '.summary.running > 0' > /dev/null; do
    sleep 5
    xpuQueue list | jq -r '.summary'
done

# === 错误处理与任务队列 ===

# 创建带错误处理的任务
cat > robust_tasks.json <<EOF
{
  "tasks": [
    {
      "name": "处理音乐文件（带错误恢复）",
      "command": "xpuLoad song.flac | xpuIn2Wav | xpuFingerprint || xpuError explain \$?",
      "priority": "normal"
    }
  ]
}
EOF
```

#### 3.5.3 错误处理示例

```bash
# === 管道错误处理 ===

# 方法1：基本错误捕获
xpuLoad song.flac | xpuIn2Wav | xpuPlay || {
    echo "播放失败，退出码: $?"
}

# 方法2：捕获并显示错误详情
{ xpuLoad song.flac | xpuIn2Wav | xpuPlay; } 2>error.log || {
    ERROR_CODE=$(cat error.log | jq -r '.error.code // 1')
    ERROR_MSG=$(cat error.log | jq -r '.error.message // "未知错误"')
    SUGGESTION=$(cat error.log | jq -r '.error.suggestion // ""')

    echo "错误 [$ERROR_CODE]: $ERROR_MSG" >&2
    [ -n "$SUGGESTION" ] && echo "建议: $SUGGESTION" >&2
}

# 方法3：使用xpuError工具
xpuLoad song.flac | xpuIn2Wav 2>error.txt || {
    xpuError explain $(cat error.txt | jq -r '.error.code')
    xpuError suggest < error.txt
}

# 方法4：脚本中的错误处理
process_music() {
    local file="$1"

    # 捕获错误到文件
    if ! xpuLoad "$file" | xpuIn2Wav 2>"$file.error"; then
        ERROR_CODE=$(cat "$file.error" | jq -r '.error.code')

        case $ERROR_CODE in
            64)  # AudioDecodeError
                echo "解码失败，尝试重新编码..."
                ffmpeg -i "$file" -c:a flac "${file%.flac}_fixed.flac"
                xpuLoad "${file%.flac}_fixed.flac" | xpuIn2Wav
                ;;
            80)  # CacheNotFound
                echo "缓存未找到，重新生成..."
                xpuLoad "$file" | xpuIn2Wav --fft-cache
                ;;
            *)
                cat "$file.error" >&2
                return 1
                ;;
        esac
    fi
}

# === 错误日志分析 ===

# 查看今天的错误
xpuError log --today

# 统计错误类型
xpuError stats --days 7 | jq '.by_category'

# 查找特定模块的错误
xpuError log --module xpuIn2Wav

# 导出错误报告
xpuError log --since 2025-01-01 > error_report.json
```

#### 3.5.4 完整工作流示例

```bash
#!/bin/bash
# music_library_processor.sh - 完整的音乐库处理脚本

set -e  # 遇到错误立即退出

# 配置
MUSIC_DIR="$HOME/Music"
OUTPUT_DIR="$HOME/MusicAnalysis"
MAX_CONCURRENT=4

# 创建输出目录
mkdir -p "$OUTPUT_DIR"/{fingerprints,classifications,errors}

# === 阶段1：批量生成FFT缓存 ===

echo "=== 阶段1：生成FFT缓存 ==="

# 创建缓存任务列表
TASKS_FILE="$OUTPUT_DIR/fft_cache_tasks.json"
echo '{"tasks": []}' > "$TASKS_FILE"

for file in "$MUSIC_DIR"/**/*.flac; do
    # 检查缓存是否已存在
    CACHE_ID=$(xpuCache list | jq -r ".caches[] | select(.source.file == \"$file\") | .cache_id")

    if [ -z "$CACHE_ID" ]; then
        # 添加缓存生成任务
        cat >> "$TASKS_FILE" <<EOF
  {
    "name": "FFT缓存: $(basename "$file")",
    "command": "xpuLoad '$file' | xpuIn2Wav --fft-cache > /dev/null",
    "priority": "high",
    "metadata": {"file": "$file"}
  },
EOF
    else
        echo "缓存已存在: $(basename "$file")"
    fi
done

# 修复JSON格式（移除最后的逗号）
sed -i '$ s/,$//' "$TASKS_FILE"
echo ']}' >> "$TASKS_FILE"

# 批量执行
if [ "$(cat "$TASKS_FILE" | jq '.tasks | length')" -gt 0 ]; then
    xpuQueue add --batch "$TASKS_FILE"

    # 等待完成
    while xpuQueue list | jq -e '.summary.running > 0' > /dev/null; do
        sleep 5
        xpuQueue list | jq -r '.summary'
    done
fi

# === 阶段2：使用缓存进行分析 ===

echo "=== 阶段2：分析音频 ==="

for file in "$MUSIC_DIR"/**/*.flac; do
    BASENAME=$(basename "$file")

    # 获取缓存ID
    CACHE_ID=$(xpuCache list | jq -r ".caches[] | select(.source.file == \"$file\") | .cache_id")

    if [ -n "$CACHE_ID" ]; then
        echo "分析: $BASENAME"

        # 指纹识别（使用FFT缓存）
        xpuFingerprint --fft-cache $CACHE_ID \
            > "$OUTPUT_DIR/fingerprints/${BASENAME%.flac}.json" 2>error.log || {
            echo "指纹识别失败: $BASENAME"
            cat error.log >&2
            cp error.log "$OUTPUT_DIR/errors/${BASENAME%.flac}.error"
            continue
        }

        # 分类（使用FFT缓存）
        xpuClassify --fft-cache $CACHE_ID --dimension all \
            > "$OUTPUT_DIR/classifications/${BASENAME%.flac}.json" 2>error.log || {
            echo "分类失败: $BASENAME"
            cat error.log >&2
            cp error.log "$OUTPUT_DIR/errors/${BASENAME%.flac}.error"
            continue
        }
    fi
done

# === 阶段3：清理和统计 ===

echo "=== 阶段3：统计和清理 ==="

# 统计结果
echo "处理完成："
echo "  指纹: $(ls -1 "$OUTPUT_DIR/fingerprints" | wc -l)"
echo "  分类: $(ls -1 "$OUTPUT_DIR/classifications" | wc -l)"
echo "  错误: $(ls -1 "$OUTPUT_DIR/errors" | wc -l)"

# 显示错误统计
if [ "$(ls -1 "$OUTPUT_DIR/errors" | wc -l)" -gt 0 ]; then
    echo "=== 错误摘要 ==="
    xpuError stats --days 1 | jq '.by_category'
fi

# 清理旧缓存（保留7天内）
xpuCache gc --keep 7d

echo "处理完成！结果保存在: $OUTPUT_DIR"
```

#### 3.5.5 DSP效果链示例

```bash
# === 空间效果示例 ===

# 录音棚混响效果
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --reverb hall --decay 2.5 --wet 0.3 | \
    xpuPlay

# 房间混响（小房间）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --reverb room --decay 1.0 --wet 0.2 | \
    xpuPlay

# 延迟回声效果
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --delay 500 --feedback 0.4 --mix 0.3 | \
    xpuPlay

# === 调制效果示例 ===

# 合唱效果（人声增强）
xpuLoad vocals.flac | xpuIn2Wav | \
    xpuProcess --chorus --rate 1.5 --depth 0.5 --mix 0.3 | \
    xpuPlay

# 深度合唱（创造厚度）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --chorus --rate 0.5 --depth 0.8 --mix 0.5 | \
    xpuPlay

# 镶边效果（创造 swept 感觉）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --flanger --rate 0.2 --depth 0.8 --mix 0.5 | \
    xpuPlay

# 移相效果（ psychedelics）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --phaser --rate 0.5 --depth 0.7 --mix 0.4 | \
    xpuPlay

# === 失真和饱和示例 ===

# 吉他失真
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --distortion 30 --tone 0.6 | \
    xpuPlay

# 过载效果
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --overdrive 20 | \
    xpuPlay

# 模拟饱和（温暖感）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --saturation analog --drive 0.7 | \
    xpuPlay

# === 立体声处理示例 ===

# 扩展立体声宽度
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --stereo-width 1.5 | \
    xpuPlay

# 交换左右声道
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --stereo-swap | \
    xpuPlay

# M/S处理（增强中间，减弱侧面）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --mid-side --mid-gain +2 --side-gain -1 | \
    xpuPlay

# === 完整DSP效果链示例 ===

# 专业混音效果链
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --eq rock | \
    xpuProcess --compress 3:1 --threshold -16dB | \
    xpuProcess --reverb hall --decay 2.0 --wet 0.25 | \
    xpuProcess --stereo-width 1.3 | \
    xpuPlay

# 人声处理链
xpuLoad vocals.flac | xpuIn2Wav | \
    xpuProcess --eq vocal | \
    xpuProcess --compress 2:1 --threshold -12dB | \
    xpuProcess --chorus --rate 1.2 --depth 0.4 --mix 0.25 | \
    xpuProcess --reverb room --decay 1.5 --wet 0.2 | \
    xpuPlay

# 吉他处理链
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --overdrive 15 | \
    xpuProcess --eq bass --bass +3 | \
    xpuProcess --compress 4:1 --threshold -14dB | \
    xpuProcess --reverb plate --decay 1.8 --wet 0.15 | \
    xpuPlay

# === DSP预设管理示例 ===

# 保存自定义效果链为预设
xpuProcess --save-preset my_vocal \
    --eq vocal \
    --compress 2:1 --threshold -12dB \
    --chorus --rate 1.2 --depth 0.4 --mix 0.25 \
    --reverb room --decay 1.5 --wet 0.2

# 使用预设
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --load-preset my_vocal | \
    xpuPlay

# 导出预设（分享给朋友）
xpuProcess --export-preset my_vocal --output vocal_preset.json
# 共享给朋友后，他们可以导入
xpuProcess --import-preset --input vocal_preset.json
```

#### 3.5.7 GPU加速和电子管模拟示例

```bash
# === GPU加速检测和选择 ===

# 查看可用计算设备
xpuProcess --list-devices
# 输出:
# {
#   "available_backends": ["cuda", "opencl", "cpu"],
#   "devices": [
#     {"id": 0, "name": "NVIDIA RTX 4090", "type": "gpu", "memory": 24576, "compute_capability": "8.9"},
#     {"id": 1, "name": "Intel Core i9-13900K", "type": "cpu", "cores": 24}
#   ],
#   "recommended": 0,
#   "recommended_reason": "Fastest GPU available"
# }

# 自动选择最佳后端（优先GPU）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend auto | \
    xpuPlay

# 强制使用CUDA（NVIDIA GPU）
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend cuda | \
    xpuPlay

# Apple Silicon GPU加速
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend metal | \
    xpuPlay

# 跨平台OpenCL加速
xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend opencl | \
    xpuPlay

# 性能基准测试
xpuProcess --benchmark --effect "reverb hall --decay 2.5"
# 输出:
# {
#   "effect": "reverb hall --decay 2.5",
#   "audio_length": 245.8,
#   "cpu_time": 2.45,
#   "gpu_time": 0.12,
#   "speedup": 20.4,
#   "gpu_used": "NVIDIA RTX 4090",
#   "gpu_memory_used": 1024,
#   "backend": "cuda"
# }

# GPU加速FFT计算（巨大提升）
xpuLoad song.flac | xpuIn2Wav --fft-cache | \
    xpuProcess --backend cuda --eq rock | \
    xpuPlay

# === 电子管模拟示例 ===

# 经典Fender清音（6L6 + 暖偏置）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube 6L6 --drive 0.4 --bias warm --blend 0.3 | \
    xpuPlay

# Marshall失真（EL34 + 冷偏置）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube EL34 --drive 0.8 --bias cold --blend 0.7 | \
    xpuPlay

# Vox清脆音色（EL84 + 三极管模式）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube EL84 --drive 0.5 --triode --blend 0.4 | \
    xpuPlay

# 高增益现代音色（12AX7前级 + EL34功放）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube preamp --drive 0.7 --blend 0.4 | \
    xpuProcess --tube poweramp --drive 0.8 --blend 0.6 | \
    xpuPlay

# 电源压缩效应（模拟电源不够时的音色压缩）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube 6L6 --drive 0.7 --sag 0.6 --rectifier tube | \
    xpuPlay

# === GPU加速的电子管模拟 ===

# NVIDIA GPU加速电子管模拟
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend cuda --tube EL34 --drive 0.8 | \
    xpuPlay

# Apple Silicon加速
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend metal --tube 6L6 --drive 0.7 | \
    xpuPlay

# 性能监控
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend cuda --verbose-gpu --tube EL34 --drive 0.8 | \
    xpuPlay
# 输出GPU使用详情:
# Backend: CUDA (NVIDIA RTX 4090)
# GPU Memory: 512 MB / 24576 MB
# Processing Time: 0.08s (GPU) vs 1.2s (CPU)
# Speedup: 15x

# === 电子管预设示例 ===

# 使用内置预设
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube-preset fender-clean | \
    xpuPlay

xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube-preset marshall-drive | \
    xpuPlay

xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube-preset vox-chime | \
    xpuPlay

xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --tube-preset mesa-boogie | \
    xpuPlay

# === GPU加速 + 电子管完整效果链 ===

# 现代高增益吉他音色（GPU加速）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend cuda \
        --tube 12AX7 --drive 0.6 --blend 0.4 | \
    xpuProcess --backend cuda \
        --tube EL34 --drive 0.85 --blend 0.7 | \
    xpuProcess --backend cuda \
        --eq custom:60=+2,250=-1,1000=0,2000=+3,4000=+4,8000=+3 | \
    xpuProcess --backend cuda \
        --compress 4:1 --threshold -12dB | \
    xpuProcess --backend cuda \
        --reverb hall --decay 2.2 --wet 0.2 | \
    xpuPlay

# Dumble传奇音色（GPU加速）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend metal \
        --tube-preset dumble | \
    xpuProcess --backend metal \
        --compress 2:1 --threshold -18dB | \
    xpuPlay

# === 批量处理（GPU加速）===

# 批量生成电子管音色（GPU加速）
for file in ~/Music/guitar/*.flac; do
    xpuLoad "$file" | xpuIn2Wav | \
        xpuProcess --backend cuda --tube-preset marshall-drive | \
        xpuOutWave --format flac > "processed/$(basename "$file")"
done

# 性能对比测试（CPU vs GPU）
echo "测试CPU性能..."
time xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend cpu --tube EL34 --drive 0.8 | \
    xpuOutWave > /dev/null
# real: 5.2s

echo "测试GPU性能..."
time xpuLoad song.flac | xpuIn2Wav | \
    xpuProcess --backend cuda --tube EL34 --drive 0.8 | \
    xpuOutWave > /dev/null
# real: 0.35s (约15x加速)

# === 创建自定义电子管预设 ===

# 保存个人音色预设
xpuProcess --save-preset my_signature \
    --tube 12AX7 --drive 0.6 --blend 0.4 \
    --tube EL34 --drive 0.75 --blend 0.65 \
    --eq custom:60=+1,250=0,500=-1,1000=+1,2000=+2,4000=+3,8000=+2 \
    --compress 3:1 --threshold -14dB \
    --reverb hall --decay 2.0 --wet 0.2

# 查看预设内容
cat ~/.config/xpu/dsp/presets/my_signature.json

# 使用预设（GPU加速）
xpuLoad guitar.flac | xpuIn2Wav | \
    xpuProcess --backend auto --load-preset my_signature | \
    xpuPlay

# 导出预设并分享
xpuProcess --export-preset my_signature --output signature_tone.json
# 发送给朋友，他们可以导入：
xpuProcess --import-preset --input signature_tone.json
```

#### 3.5.8 播放列表导入导出示例

```bash
# === 播放列表导入 ===

# 从M3U播放列表导入
xpuPlaylist import --input ~/Music/playlists/rock.m3u

# 从PLS格式导入
xpuPlaylist import --input ~/old_player/playlists/jazz.pls

# 从XSPF格式导入
xpuPlaylist import --input ~/playlists/electronic.xspf

# 自动检测格式
xpuPlaylist import --input playlist.m3u

# 指定格式
xpuPlaylist import --input playlist.txt --format m3u

# 自定义播放列表名称
xpuPlaylist import --input playlist.m3u --name "我的收藏"

# 从网络URL导入
xpuPlaylist import --url "https://example.com/playlist.m3u"

# === 从其他播放器迁移 ===

# 从iTunes迁移
xpuPlaylist import --input ~/Music/iTunes/Library.xml --format itunes

# 从foobar2000迁移
xpuPlaylist import --input ~/playlists/foobar_playlist.fpl

# 从Spotify导出后导入（需要第三方工具）
# 1. 使用 spotify-playlist-export 导出
# 2. 导入到XPU
xpuPlaylist import --input spotify_export.json

# === 播放列表导出 ===

# 导出为M3U格式（兼容性最好）
xpuPlaylist export --input my_playlist.json --output rock.m3u

# 导出为M3U8格式（UTF-8编码）
xpuPlaylist export --input my_playlist.json --output rock.m3u8

# 导出为PLS格式
xpuPlaylist export --input my_playlist.json --output rock.pls

# 导出为XSPF格式（XML格式，功能丰富）
xpuPlaylist export --input my_playlist.json --output rock.xspf

# === 播放列表验证和清理 ===

# 验证播放列表（检查文件是否存在）
xpuPlaylist validate rock.m3u

# 输出
# {
#   "valid": false,
#   "total_tracks": 25,
#   "missing_files": 3,
#   "missing": [
#     "/home/user/Music/missing_song.flac",
#     ...
#   ]
# }

# 去重播放列表（基于音频指纹）
xpuPlaylist dedup large_playlist.m3u --output unique_playlist.m3u

# 排序播放列表
xpuPlaylist sort playlist.m3u --by artist --output sorted.m3u
xpuPlaylist sort playlist.m3u --by title --shuffle --output random.m3u

# === 播放列表自动组织 ===

# 按流派组织
xpuPlaylist auto-organize --rule "genre:rock" --output rock.m3u
xpuPlaylist auto-organize --rule "genre:jazz" --output jazz.m3u

# 按年代组织
xpuPlaylist auto-organize --rule "year:1990-1999" --output 90s.m3u
xpuPlaylist auto-organize --rule "year:>2010" --output recent.m3u

# 按情绪组织
xpuPlaylist auto-organize --rule "mood:energetic" --output workout.m3u
xpuPlaylist auto-organize --rule "mood:relaxed" --output chill.m3u

# 组合条件
xpuPlaylist auto-organize --rule "genre:rock mood:energetic year:>2000" --output modern_rock.m3u

# === 批量导入播放列表 ===

# 从旧播放器批量导入
for file in ~/old_player/playlists/*.{m3u,pls,xspf}; do
    xpuPlaylist import --input "$file"
done

# 验证所有导入的播放列表
for file in ~/.config/xpu/playlists/*.m3u; do
    xpuPlaylist validate "$file"
done

# === 播放列表与队列集成 ===

# 从播放列表创建播放任务
xpuPlaylist show my_rock.m3u | jq -r '.tracks[].file' | \
    while read file; do
        xpuQueue add --name "$(basename "$file")" \
            --command "xpuLoad '$file' | xpuIn2Wav | xpuPlay"
    done

# 播放整个播放列表
xpuPlaylist show my_rock.m3u | jq -r '.tracks[].file' | \
    while read file; do
        xpuLoad "$file" | xpuIn2Wav | xpuProcess --load-preset my_preset | xpuPlay
    done
```

## 4. xpuDaemon 设计

### 4.1 架构

```cpp
class XpuDaemon {
public:
    void run();

private:
    // 接口层
    std::unique_ptr<MCPServer> mcp_server_;
    std::unique_ptr<RestApiServer> api_server_;
    std::unique_ptr<AgentProtocolServer> agent_server_;

    // 语义层
    std::unique_ptr<IntentRecognizer> intent_recognizer_;
    std::unique_ptr<ParameterNormalizer> param_normalizer_;
    std::unique_ptr<ResponseFormatter> response_formatter_;

    // 编排层
    std::unique_ptr<Orchestrator> orchestrator_;

    // 状态管理
    std::unique_ptr<StateManager> state_manager_;

    // 缓存管理（统一管理所有缓存索引和GC）
    std::unique_ptr<CacheManager> cache_manager_;

    // 任务队列
    std::unique_ptr<TaskQueue> task_queue_;
    std::unique_ptr<TaskExecutor> task_executor_;

    // 模块注册表
    ModuleRegistry module_registry_;
};
```

**架构职责明确：**

| 组件 | 职责 | 不负责 |
|------|------|--------|
| CacheManager | 缓存索引、LRU淘汰、定期GC、依赖追踪 | 缓存数据存储（由模块负责） |
| Orchestrator | 简单线性管道编排、基本错误传播 | 复杂流程控制（交给脚本） |
| TaskQueue | 任务并发、状态跟踪、优先级 | 任务依赖图（DAG） |
| StateManager | 播放状态、队列状态 | 缓存状态（由CacheManager负责） |

### 4.2 CLI 与 Daemon 状态同步机制

#### 4.2.1 设计目标

确保 CLI 直接操作和 Daemon 调度操作的状态一致性，解决多入口状态不同步问题。

**问题场景：**
```bash
# 场景1：CLI 直接操作
xpuQueue add ~/song1.flac          # CLI 直接操作

# 场景2：Daemon 调度操作
# 通过 MCP/REST API 调用 xpu_queue_add
# Daemon 内部调用 CLI 模块

# 场景3：查询状态
xpuQueue list                       # 可能看到不同的状态
GET /api/v3/queue                   # 可能看到不同的状态
```

#### 4.2.2 状态同步模式

**模式1：强制 Daemon 模式（推荐）**

```bash
# 环境变量控制
export XPU_FORCE_DAEMON=1           # 所有 CLI 操作强制通过 Daemon

# 行为：
# xpuQueue add ~/song.flac
#   → 不直接操作队列
#   → 发送到 Daemon → Daemon 执行 → 状态统一管理
```

**实现：**

```cpp
// CLI 模块中
class XpuCli {
public:
    int run(int argc, char* argv[]) {
        // 检查是否强制 Daemon 模式
        if (getenv("XPU_FORCE_DAEMON")) {
            return forwardToDaemon(argc, argv);
        }

        // 否则直接执行
        return executeDirectly(argc, argv);
    }

private:
    int forwardToDaemon(int argc, char* argv[]) {
        // 1. 连接到 Daemon
        DaemonClient client(getDaemonSocket());

        // 2. 发送命令
        json request = {
            {"command", argv[0]},
            {"args", json::array()}
        };
        for (int i = 1; i < argc; i++) {
            request["args"].push_back(argv[i]);
        }

        // 3. 等待响应
        json response = client.send(request);

        // 4. 输出结果
        if (response.contains("error")) {
            std::cerr << response["error"] << std::endl;
            return response["error"]["code"];
        }

        std::cout << response["result"].dump() << std::endl;
        return 0;
    }

    std::string getDaemonSocket() {
        // 优先使用环境变量
        if (const char* socket = getenv("XPU_DAEMON_SOCKET")) {
            return socket;
        }
        // 默认位置
        return "/tmp/xpu_daemon.sock";
    }
};
```

**模式2：自动同步模式（默认）**

```bash
# CLI 操作后自动同步到 Daemon
xpuQueue add ~/song.flac           # CLI 直接操作
# → 操作完成后自动通知 Daemon
# → Daemon 更新内部状态

# 启用自动同步
export XPU_AUTO_SYNC=1             # 默认启用
```

**实现：**

```cpp
// CLI 模块中
class StateSyncClient {
public:
    // 操作完成后调用
    static void syncState(const std::string& operation,
                         const json& result) {
        if (!getenv("XPU_AUTO_SYNC")) {
            return;  // 禁用自动同步
        }

        try {
            DaemonClient client(getDaemonSocket());

            // 发送状态更新
            json update = {
                {"operation", operation},
                {"result", result},
                {"timestamp", std::time(nullptr)}
            };

            client.send("/state/sync", update);
        } catch (...) {
            // Daemon 不可用时静默失败
            // 不影响 CLI 正常使用
        }
    }
};

// xpuQueue 示例
int xpuQueue_add(const std::string& file) {
    // 1. 执行操作
    json result = do_add(file);

    // 2. 同步状态
    StateSyncClient::syncState("queue_add", result);

    // 3. 返回结果
    std::cout << result.dump() << std::endl;
    return 0;
}
```

#### 4.2.3 状态数据结构

```cpp
// Daemon 中的状态管理
class StateManager {
public:
    // 获取完整状态
    json getFullState() const {
        return {
            {"queue", queue_state_},
            {"playback", playback_state_},
            {"devices", device_state_},
            {"cache", cache_state_},
            {"version", state_version_}
        };
    }

    // CLI 操作同步
    void syncFromCli(const std::string& operation, const json& result) {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (operation == "queue_add") {
            queue_state_.items.push_back(result["item"]);
            queue_state_.version++;
        }
        else if (operation == "queue_remove") {
            auto it = std::remove_if(queue_state_.items.begin(),
                                     queue_state_.items.end(),
                                     [&](const auto& item) {
                                         return item["id"] == result["id"];
                                     });
            queue_state_.items.erase(it, queue_state_.items.end());
            queue_state_.version++;
        }
        // ... 其他操作

        // 触发状态变更事件
        notifyStateChanged();
    }

    // 订阅状态变更
    void subscribe(StateChangeCallback callback) {
        subscribers_.push_back(callback);
    }

private:
    void notifyStateChanged() {
        state_version_++;
        for (auto& callback : subscribers_) {
            callback(getFullState());
        }
    }

    json queue_state_;
    json playback_state_;
    json device_state_;
    json cache_state_;
    uint64_t state_version_ = 0;

    std::mutex state_mutex_;
    std::vector<StateChangeCallback> subscribers_;
};
```

#### 4.2.4 WebSocket 状态推送

```cpp
// Daemon 通过 WebSocket 推送状态变更
class WebSocketStatePusher {
public:
    void onStateChanged(const json& state) {
        // 推送到所有订阅的客户端
        for (auto& client : clients_) {
            try {
                client->send(json({
                    {"type", "state_changed"},
                    {"state", state},
                    {"version", state["version"]}
                }));
            } catch (...) {
                // 客户端断开，移除
                markForRemoval(client);
            }
        }
    }

    void subscribe(ws_server* client) {
        clients_.push_back(client);

        // 立即发送当前状态
        client->send(json({
            {"type", "state_snapshot"},
            {"state", state_manager_->getFullState()}
        }));
    }

private:
    std::vector<ws_server*> clients_;
    StateManager* state_manager_;
};
```

**客户端订阅：**

```javascript
// 客户端订阅状态变更
const ws = new WebSocket('ws://localhost:8080/api/v3/events');

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);

    if (msg.type === 'state_changed') {
        // 状态变更，更新 UI
        updateUI(msg.state);
    }
    else if (msg.type === 'state_snapshot') {
        // 初始状态快照
        initializeUI(msg.state);
    }
};
```

#### 4.2.5 状态一致性保证

**乐观锁机制：**

```cpp
// 状态更新时检查版本
class StateManager {
public:
    bool updateQueue(const json& update, uint64_t expected_version) {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (state_version_ != expected_version) {
            // 版本冲突，状态已被其他操作修改
            return false;
        }

        // 应用更新
        applyUpdate(update);
        state_version_++;

        return true;
    }

    // 获取当前版本
    uint64_t getVersion() const {
        return state_version_;
    }
};
```

**客户端使用：**

```bash
# 获取状态和版本
STATE=$(xpuQueue list --with-version)
VERSION=$(echo "$STATE" | jq -r '.version')

# 修改状态
xpuQueue add --expected-version $VERSION ~/song.flac

# 如果版本冲突，返回错误
# {"error": {"code": 75, "name": "VersionConflict", ...}}
```

#### 4.2.6 配置选项

```bash
# ~/.config/xpu/daemon.conf
[state]
# 强制所有 CLI 操作通过 Daemon
force_daemon_mode = false      # 默认 false

# CLI 操作后自动同步
auto_sync_enabled = true        # 默认 true

# 状态推送
enable_websocket_push = true    # 默认 true
push_debounce_ms = 100          # 防抖延迟（毫秒）

# 状态持久化
persist_state = true            # 持久化到磁盘
persist_interval = 60           # 持久化间隔（秒）
state_file = ~/.local/state/xpu/daemon_state.json
```

#### 4.2.7 CLI 命令示例

```bash
# 强制 Daemon 模式
export XPU_FORCE_DAEMON=1
xpuQueue add ~/song.flac
# → 所有操作通过 Daemon，状态天然一致

# 自动同步模式（默认）
export XPU_AUTO_SYNC=1          # 默认启用
xpuQueue add ~/song.flac
# → CLI 直接操作，完成后自动同步

# 禁用同步（不推荐）
export XPU_AUTO_SYNC=0
xpuQueue add ~/song.flac
# → CLI 直接操作，不同步到 Daemon
# → MCP/API 查询时看不到这个变化

# 查看状态版本
xpuQueue list --with-version
# {"items": [...], "version": 123}

# 带版本检查的更新
xpuQueue remove --id abc123 --expected-version 123
```

#### 4.2.8 错误处理

**版本冲突错误：**

```json
{
  "error": {
    "code": 75,
    "category": "Process",
    "name": "StateVersionConflict",
    "message": "State was modified by another operation",
    "context": {
      "expected_version": 123,
      "current_version": 125
    },
    "suggestion": "Retry the operation with the current version"
  }
}
```

**Daemon 不可用错误：**

```cpp
// 强制模式下 Daemon 不可用
if (getenv("XPU_FORCE_DAEMON")) {
    if (!daemonAvailable()) {
        throw XpuException(ErrorCode::DaemonUnavailable,
            "Daemon is not running but required by XPU_FORCE_DAEMON")
            .withSuggestion("Start xpuDaemon or unset XPU_FORCE_DAEMON");
    }
}
```

#### 4.2.9 状态恢复

```bash
# Daemon 启动时恢复状态
xpuDaemon --restore-state

# 从持久化文件恢复
# ~/.local/state/xpu/daemon_state.json

# 状态文件格式
{
  "version": 123,
  "timestamp": 1704556800,
  "queue": {
    "items": [...],
    "current": 0
  },
  "playback": {
    "state": "stopped",
    "position": 0
  }
}
```

#### 4.2.10 监控和调试

```bash
# 查看状态同步日志
xpuDaemon --log-level debug | grep "state_sync"

# 输出示例:
# [DEBUG] state_sync: Received sync from CLI: queue_add
# [DEBUG] state_sync: State updated: version 123 → 124
# [DEBUG] state_push: Broadcasting state change to 3 clients

# 强制状态同步
xpuDaemon --sync-state-from-cli

# 重置状态（清空所有状态）
xpuDaemon --reset-state
```

### 4.3 缓存统一管理

#### 4.3.1 CacheManager设计

```cpp
class CacheManager {
public:
    // 模块创建缓存后调用（通过Unix socket或HTTP API）
    void registerCache(const CacheInfo& info);

    // 查询缓存（快速返回，用于优化决策）
    std::optional<CacheInfo> lookupCache(
        const std::string& source_file,
        CacheType type);

    // 批量查询（用于优化批量处理）
    std::vector<CacheInfo> lookupCaches(
        const std::vector<std::string>& source_files,
        CacheType type);

    // 缓存失效通知（源文件修改时）
    void invalidateCache(const std::string& cache_id);

    // 定期垃圾回收
    void runGC();

    // 获取统计信息
    CacheStats getStats() const;

private:
    // SQLite索引（持久化）
    std::unique_ptr<CacheIndex> index_;

    // 内存缓存（快速查询）
    std::unordered_map<std::string, CacheInfo> memory_cache_;

    // LRU队列
    std::list<std::string> lru_list_;

    // 配置
    CacheConfig config_;
};

struct CacheInfo {
    std::string cache_id;
    CacheType type;                    // fft, visual, fingerprint
    std::string source_file;           // 源音频文件
    std::string source_hash;           // 源文件hash
    std::filesystem::path cache_path;  // 缓存目录
    size_t size_bytes;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    uint32_t access_count;
    json metadata;                     // 模块自定义数据
};

enum class CacheType {
    FFT,
    Visual,
    Fingerprint
};
```

#### 4.2.2 CLI模块通知Daemon

**方法1：Unix Socket（推荐）**

```bash
# Daemon启动时创建缓存socket
xpuDaemon start --cache-socket /tmp/xpu_cache.sock

# 环境变量自动注入
export XPU_CACHE_SOCKET=/tmp/xpu_cache.sock

# xpuIn2Wav完成FFT缓存后，自动通知
# 模块内部代码：
if (fft_cache_enabled) {
    std::string cache_id = generateCacheId();
    writeFFTCache(cache_id);

    // 通知Daemon
    notifyDaemon(cache_id, CacheType::FFT, input_file);
}

// notifyDaemon实现
void notifyDaemon(const std::string& cache_id,
                 CacheType type,
                 const std::string& source_file) {
    const char* socket_path = std::getenv("XPU_CACHE_SOCKET");
    if (!socket_path) return;  // Daemon未运行，跳过

    // 发送注册请求
    sendHttpRequest("http://localhost:8080/api/cache/register", {
        {"cache_id", cache_id},
        {"type", to_string(type)},
        {"source_file", source_file},
        {"size_bytes", getCacheSize(cache_id)}
    });
}
```

**方法2：HTTP API**

```bash
# REST API注册缓存
POST /api/cache/register
{
  "cache_id": "a1b2c3d4e5f6",
  "type": "fft",
  "source_file": "/home/user/Music/song.flac",
  "source_hash": "sha256:abc123...",
  "size_bytes": 188000000,
  "metadata": {
    "fft_size": 2048,
    "sample_rate": 96000
  }
}

# 查询缓存
GET /api/cache/lookup?file=/home/user/Music/song.flac&type=fft
# 返回
{
  "exists": true,
  "cache_id": "a1b2c3d4e5f6",
  "cache_path": "/home/user/.cache/xpu/fft/a1b2c3d4e5f6",
  "size_bytes": 188000000
}
```

#### 4.2.3 Daemon缓存查询优化

```cpp
// Orchestrator在编排前查询缓存
class Orchestrator {
public:
    json execute(const SemanticRequest& request) {
        if (request.intent == Intent::Play) {
            std::string file = request.parameters["file"];

            // 查询FFT缓存
            auto fft_cache = cache_manager_->lookupCache(file, CacheType::FFT);

            Pipe pipe;
            pipe.addModule("xpuLoad");

            if (fft_cache) {
                // 缓存存在，跳过FFT计算
                std::cout << "复用FFT缓存: " << fft_cache->cache_id << std::endl;
                pipe.addModule("xpuIn2Wav --no-fft-cache");
                pipe.addModule("xpuFingerprint --fft-cache " + fft_cache->cache_id);
            } else {
                // 缓存不存在，计算FFT
                pipe.addModule("xpuIn2Wav --fft-cache");
                pipe.addModule("xpuFingerprint");
            }

            return executePipeline(pipe);
        }
    }
};
```

#### 4.2.4 缓存失效策略

```cpp
class CacheManager {
public:
    // 定期GC（由Daemon定时触发）
    void runGC() {
        auto now = std::chrono::system_clock::now();

        // 策略1：清理过期缓存
        cleanExpired(now - config_.max_age);

        // 策略2：清理超过大小限制
        while (getTotalSize() > config_.max_size) {
            evictLRU();
        }

        // 策略3：清理低频缓存
        cleanLowAccessCount(config_.min_access_count);
    }

    // 源文件修改时失效缓存
    void onFileModified(const std::string& file_path) {
        auto caches = index_->findBySource(file_path);
        for (const auto& cache : caches) {
            invalidateCache(cache.cache_id);
        }
    }

private:
    void evictLRU() {
        // 淘汰最久未使用的缓存
        auto cache_id = lru_list_.front();
        lru_list_.pop_front();

        // 删除缓存文件
        std::filesystem::remove_all(getCachePath(cache_id));

        // 更新索引
        index_->remove(cache_id);
        memory_cache_.erase(cache_id);
    }
};
```

#### 4.2.5 CLI工具集成

```bash
# xpuCache工具底层调用Daemon API
xpuCache list
# → GET /api/cache/list

xpuCache stat a1b2c3d4e5f6
# → GET /api/cache/a1b2c3d4e5f6

xpuCache gc --keep 7d
# → POST /api/cache/gc {"keep_days": 7}

# Daemon未运行时，xpuCache回退到直接读取SQLite
if ! xpuDaemon status >/dev/null 2>&1; then
    echo "Daemon未运行，直接读取缓存索引"
    sqlite3 ~/.cache/xpu/index.db "SELECT * FROM caches"
fi
```

### 4.3 统一语义层

#### 4.3.1 意图识别

```cpp
enum class Intent {
    // 播放控制
    Play,
    Pause,
    Resume,
    Stop,
    Seek,
    Next,
    Previous,

    // 队列管理
    QueueAdd,
    QueueRemove,
    QueueClear,
    QueueList,
    QueueShuffle,

    // 音量控制
    VolumeSet,
    VolumeUp,
    VolumeDown,
    Mute,
    Unmute,

    // 信息查询
    GetStatus,
    GetMetadata,
    GetQueue,
    GetDevices,

    // 设备管理
    DeviceSet,
    DeviceList,
    DeviceTest,

    // 搜索
    Search,
    Find
};

struct SemanticRequest {
    Intent intent;
    std::map<std::string, std::string> parameters;  // 规范化后的参数
    std::string original_query;                     // 原始查询（用于调试）
    std::string context;                            // 上下文信息
};
```

#### 4.3.2 参数规范化

```cpp
class ParameterNormalizer {
public:
    // 规范化时间表达
    std::optional<double> normalizeTime(const std::string& expr);

    // 规范化音量
    std::optional<float> normalizeVolume(const std::string& expr);

    // 规范化文件路径
    std::optional<std::string> normalizePath(const std::string& expr);

    // 规范化设备
    std::optional<std::string> normalizeDevice(const std::string& expr);

private:
    // "1:30" -> 90.0 秒
    // "90 seconds" -> 90.0 秒
    // "50%" -> 0.5
    // "~/Music" -> "/home/user/Music"
};
```

**示例：**

```cpp
// 输入参数规范化
{
    "time": "1:30",        // → 90.0 (秒)
    "volume": "80%",       // → 0.8
    "file": "~/Music/song.flac",  // → "/home/user/Music/song.flac"
    "device": "speaker",   // → "alsa_output.pci-0000_00_1f.3.analog-stereo"
    "sample_rate": "192k", // → 192000 (Hz)
    "bit_depth": "32"      // → 32 (bit)
}
```

#### 4.3.3 结果格式化

```cpp
class ResponseFormatter {
public:
    // 为不同客户端格式化响应
    json formatForMCP(const json& result);
    json formatForAPI(const json& result);
    json formatForAgent(const json& result);

    // 人类友好的描述
    std::string describe(const json& result);

private:
    // 生成自然语言描述
    std::string generateDescription(const json& result);
};
```

### 4.4 模块编排引擎

```cpp
class Orchestrator {
public:
    // 执行语义请求
    json execute(const SemanticRequest& request);

    // 构建管道
    Pipe buildPipeline(const std::vector<std::string>& modules);

    // 管理模块生命周期
    void startModule(const std::string& name);
    void stopModule(const std::string& name);

private:
    // 管道执行
    json executePipeline(Pipe& pipe);

    // 错误处理
    json handleError(const std::exception& e);
};
```

**编排示例：**

```cpp
// 语义请求: "播放 ~/Music/song.flac，音量80%，使用摇滚均衡器，输出为192kHz/32-bit"
SemanticRequest req{
    .intent = Intent::Play,
    .parameters = {
        {"file", "/home/user/Music/song.flac"},
        {"volume", "0.8"},
        {"eq", "rock"},
        {"sample_rate", "192000"},
        {"bit_depth", "32"}
    }
};

// Daemon 编排完整管道
Pipe pipe = orchestrator.buildPipeline({
    "xpuLoad",        // 解析音频文件
    "xpuIn2Wav",      // 重采样到目标格式（无损）
    "xpuProcess",     // DSP 处理（音量、EQ）
    "xpuOutWave",     // 输出格式转换
    "xpuPlay"         // 播放
});

// 等价于命令行：
// xpuLoad file.flac | xpuIn2Wav --rate 192000 --depth 32 | xpuProcess --volume 0.8 --eq rock | xpuOutWave --auto | xpuPlay

// 执行并返回结果
json result = orchestrator.executePipeline(pipe);
```

**编排示例（带分析）：**

```cpp
// 语义请求: "分析并播放 ~/Music/song.flac，告诉我这是什么类型的音乐"
SemanticRequest req{
    .intent = Intent::Play,
    .parameters = {
        {"file", "/home/user/Music/song.flac"},
        {"analyze", "true"},        // 启用分析
        {"dimensions", "all"}       // 分析所有维度
    }
};

// Daemon 编排完整管道（包含指纹、分类和可视化）
Pipe pipe = orchestrator.buildPipeline({
    "xpuLoad",         // 解析音频文件
    "xpuIn2Wav",       // 重采样到目标格式
    "xpuFingerprint",  // 生成音频指纹
    "xpuClassify",     // 音乐分类（流派、情绪、活动）
    "xpuVisualize",    // 生成可视化数据
    "xpuProcess",      // DSP 处理
    "xpuOutWave",      // 输出格式转换
    "xpuPlay"          // 播放
});

// 等价于命令行：
// xpuLoad file.flac | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all | xpuVisualize | xpuProcess | xpuOutWave --auto | xpuPlay

// 执行并返回结果（包含分类和可视化信息）
json result = orchestrator.executePipeline(pipe);
// result["classification"] 包含流派、情绪、活动等信息
// result["visualization"] 包含缓存 ID 和路径
```

### 4.5 轻量任务队列

为支持批量音频处理和并发操作，xpuDaemon 提供轻量级任务队列功能。设计原则是**简单实用**，避免复杂的调度算法。

#### 4.5.1 任务定义

```cpp
enum class TaskStatus {
    Pending,    // 等待执行
    Running,    // 执行中
    Completed,  // 已完成
    Failed,     // 失败
    Cancelled   // 已取消
};

enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2
};

struct Task {
    std::string id;                    // 任务ID（UUID）
    std::string name;                  // 任务名称
    std::string command;               // 完整命令行
    TaskStatus status;                 // 状态
    TaskPriority priority;             // 优先级
    double progress;                   // 进度（0.0-1.0）
    std::string error_message;         // 错误信息（如果失败）
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    json result;                       // 执行结果
    json metadata;                     // 元数据（用户自定义）
};
```

#### 4.5.2 任务队列管理

```cpp
class TaskQueue {
public:
    // 添加任务
    std::string addTask(const std::string& name,
                       const std::string& command,
                       TaskPriority priority = TaskPriority::Normal,
                       const json& metadata = {});

    // 批量添加任务
    std::vector<std::string> addTasks(const std::vector<Task>& tasks);

    // 获取任务状态
    Task getTask(const std::string& task_id);

    // 列出所有任务
    std::vector<Task> listTasks(TaskStatus filter = {});

    // 取消任务
    bool cancelTask(const std::string& task_id);

    // 删除任务
    bool removeTask(const std::string& task_id);

    // 清空已完成任务
    void clearCompleted();

    // 获取队列统计
    json getStats();

private:
    std::queue<Task> pending_tasks_;
    std::map<std::string, Task> running_tasks_;
    std::map<std::string, Task> completed_tasks_;
    std::mutex mutex_;
    size_t max_concurrent_ = 4;        // 最大并发数
};
```

#### 4.5.3 任务执行器

```cpp
class TaskExecutor {
public:
    // 启动执行器（创建工作线程）
    void start(size_t num_workers = 4);

    // 停止执行器
    void stop();

    // 设置最大并发数
    void setMaxConcurrent(size_t max);

    // 执行单个任务
    json execute(const Task& task);

private:
    // 工作线程
    void workerThread();

    // 更新任务进度
    void updateProgress(const std::string& task_id, double progress);

    std::vector<std::thread> workers_;
    TaskQueue* queue_;
    std::atomic<bool> running_;
};
```

#### 4.5.4 CLI 接口

```bash
# 添加单个任务
xpuQueue add --name "处理音乐库" \
    --command "xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify" \
    --priority normal

# 输出
{
  "task_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "status": "pending",
  "created_at": "2025-01-07T10:00:00Z"
}

# 批量添加任务（从文件）
xpuQueue add --batch tasks.json

# tasks.json 格式
{
  "tasks": [
    {
      "name": "处理专辑1",
      "command": "xpuLoad album1/song.flac | xpuIn2Wav | xpuFingerprint",
      "priority": "normal",
      "metadata": {"album": "album1", "artist": "Artist1"}
    },
    {
      "name": "处理专辑2",
      "command": "xpuLoad album2/song.flac | xpuIn2Wav | xpuFingerprint",
      "priority": "normal",
      "metadata": {"album": "album2", "artist": "Artist2"}
    }
  ]
}

# 查看任务状态
xpuQueue status <task_id>

# 输出
{
  "task_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "name": "处理音乐库",
  "status": "running",
  "progress": 0.35,
  "started_at": "2025-01-07T10:00:05Z",
  "estimated_remaining": 125.5
}

# 列出所有任务
xpuQueue list

# 输出
{
  "tasks": [
    {
      "task_id": "...",
      "name": "处理音乐库",
      "status": "running",
      "progress": 0.35
    },
    {
      "task_id": "...",
      "name": "处理专辑2",
      "status": "pending",
      "progress": 0.0
    }
  ],
  "summary": {
    "total": 10,
    "pending": 5,
    "running": 2,
    "completed": 2,
    "failed": 1
  }
}

# 按状态过滤
xpuQueue list --status running
xpuQueue list --status failed
xpuQueue list --status completed

# 取消任务
xpuQueue cancel <task_id>

# 删除任务
xpuQueue remove <task_id>

# 清空已完成任务
xpuQueue clear --completed

# 暂停队列（停止接受新任务）
xpuQueue pause

# 恢复队列
xpuQueue resume

# 获取队列统计
xpuQueue stats

# 输出
{
  "total_tasks": 100,
  "pending": 10,
  "running": 4,
  "completed": 80,
  "failed": 5,
  "cancelled": 1,
  "avg_duration": 45.2,
  "success_rate": 0.94
}
```

#### 4.5.5 使用示例

```bash
# === 批量处理音乐库 ===

# 方法1：使用xargs并行执行（外部并发）
find ~/Music -name "*.flac" | xargs -P 4 -I {} bash -c \
    'xpuLoad {} | xpuIn2Wav | xpuFingerprint | xpuClassify'

# 方法2：使用任务队列（Daemon内部并发）
xpuQueue add --batch music_library_tasks.json
xpuQueue list --watch  # 实时监控进度

# music_library_tasks.json 生成脚本
for file in ~/Music/**/*.flac; do
    cat >> tasks.json <<EOF
{
  "name": "处理 $(basename "$file")",
  "command": "xpuLoad '$file' | xpuIn2Wav | xpuFingerprint | xpuClassify",
  "priority": "normal",
  "metadata": {"file": "$file"}
}
EOF
done

# === 并发生成缓存 ===

# 批量生成FFT缓存
for file in ~/Music/*.flac; do
    xpuQueue add --name "FFT缓存: $(basename "$file")" \
        --command "xpuLoad '$file' | xpuIn2Wav --fft-cache > /dev/null"
done

# 等待所有任务完成
while xpuQueue list | jq -r '.summary.running > 0'; do
    sleep 5
    xpuQueue list | jq -r '.summary'
done

# === 任务依赖管理 ===

# 添加依赖任务（手动编排）
# 任务1：生成FFT缓存
xpuQueue add --name "生成FFT缓存" \
    --command "xpuLoad song.flac | xpuIn2Wav --fft-cache > /dev/null"

# 等待任务1完成
TASK_ID=$(xpuQueue add --name "生成FFT缓存" ...)
while [ "$(xpuQueue status $TASK_ID | jq -r '.status')" != "completed" ]; do
    sleep 1
done

# 获取cache_id
CACHE_ID=$(xpuQueue status $TASK_ID | jq -r '.result.cache_id')

# 任务2：使用缓存进行指纹识别
xpuQueue add --name "指纹识别" \
    --command "xpuFingerprint --fft-cache $CACHE_ID"
```

#### 4.5.6 配置

```toml
# ~/.config/xpu/queue.conf

[queue]
# 最大并发任务数
max_concurrent = 4

# 任务超时时间（秒）
timeout = 3600

# 自动清理已完成任务的时间
auto_cleanup_completed_after = "7d"

# 自动清理失败任务的时间
auto_cleanup_failed_after = "30d"

# 任务重试次数
max_retries = 3

# 任务结果保留时间
keep_results_for = "30d"

[worker]
# 工作线程数（0 = 自动检测CPU核心数）
num_workers = 0

# 每个工作线程的内存限制（字节）
memory_limit = 2147483648  # 2GB

# CPU亲和性（绑定到特定CPU核心）
cpu_affinity = []

[logging]
# 任务日志级别
log_level = "info"

# 任务日志目录
log_dir = "~/.local/log/xpu/tasks"

# 保存任务输出
save_output = true
```

### 4.6 统一错误处理标准

#### 4.6.1 错误码体系

```cpp
// 错误类别（高4位）
enum class ErrorCategory {
    System = 0x10,       // 0x1x: 系统错误（内存、资源不足）
    Parameter = 0x20,    // 0x2x: 参数错误（无效参数、格式错误）
    Resource = 0x30,     // 0x3x: 资源错误（文件不存在、权限拒绝）
    Process = 0x40,      // 0x4x: 处理错误（解码失败、格式不支持）
    Cache = 0x50,        // 0x5x: 缓存错误（缓存损坏、版本不匹配）
    Network = 0x60,      // 0x6x: 网络错误（连接失败、超时）
    Device = 0x70        // 0x7x: 设备错误（设备不可用、格式不支持）
};

// 具体错误码
enum class ErrorCode {
    // 系统错误 (0x10-0x1F)
    OutOfMemory = 0x10,
    ResourceExhausted = 0x11,
    InternalError = 0x1F,

    // 参数错误 (0x20-0x2F)
    InvalidArgument = 0x20,
    MissingRequiredArgument = 0x21,
    InvalidFormat = 0x22,
    InvalidRange = 0x23,

    // 资源错误 (0x30-0x3F)
    FileNotFound = 0x30,
    PermissionDenied = 0x31,
    DirectoryNotFound = 0x32,
    FileAlreadyExists = 0x33,

    // 处理错误 (0x40-0x4F)
    AudioDecodeError = 0x40,
    FormatNotSupported = 0x41,
    SampleRateNotSupported = 0x42,
    BitDepthNotSupported = 0x43,
    ChannelCountNotSupported = 0x44,
    ProcessingError = 0x45,

    // 缓存错误 (0x50-0x5F)
    CacheNotFound = 0x50,
    CacheCorrupted = 0x51,
    CacheVersionMismatch = 0x52,
    CacheWriteError = 0x53,

    // 网络错误 (0x60-0x6F)
    ConnectionFailed = 0x60,
    ConnectionTimeout = 0x61,
    InvalidURL = 0x62,
    AuthenticationFailed = 0x63,

    // 设备错误 (0x70-0x7F)
    DeviceNotFound = 0x70,
    DeviceUnavailable = 0x71,
    DeviceOpenFailed = 0x72
};
```

#### 4.6.2 错误信息格式

所有错误信息输出到 stderr，采用标准化 JSON 格式：

```json
{
  "error": {
    "code": 64,
    "category": "Process",
    "name": "AudioDecodeError",
    "message": "Failed to decode audio file",
    "module": "xpuIn2Wav",
    "context": {
      "input_file": "/path/to/song.flac",
      "file_size": 15728640,
      "detected_format": "FLAC",
      "position": "124.5s"
    },
    "suggestion": "Try re-encoding the file with ffmpeg: ffmpeg -i song.flac -c:a flac song_fixed.flac",
    "documentation_url": "https://xpu.music/docs/errors/0x40"
  }
}
```

#### 4.6.3 错误上下文传递

管道中的错误应该保留完整上下文：

```cpp
struct ErrorContext {
    std::string module;              // 发生错误的模块
    ErrorCode code;                  // 错误码
    std::string message;             // 错误消息
    std::map<std::string, std::string> details;  // 详细信息
    std::string suggestion;          // 恢复建议
    std::optional<std::exception_ptr> inner_exception;  // 内部异常
};

// 错误传播
class XpuException : public std::exception {
public:
    XpuException(ErrorCode code, const std::string& message)
        : code_(code), message_(message) {}

    XpuException withContext(const std::string& key, const std::string& value) {
        context_[key] = value;
        return *this;
    }

    XpuException withModule(const std::string& module) {
        module_ = module;
        return *this;
    }

    json toJson() const {
        return {
            {"error", {
                {"code", static_cast<int>(code_)},
                {"category", getCategoryName(code_)},
                {"name", getErrorName(code_)},
                {"message", message_},
                {"module", module_},
                {"context", context_},
                {"suggestion", getSuggestion(code_)}
            }}
        };
    }

private:
    ErrorCode code_;
    std::string message_;
    std::string module_;
    std::map<std::string, std::string> context_;
};
```

#### 4.6.4 管道错误处理

```bash
# 方法1：使用 set -e 确保管道失败时立即退出
set -e; xpuLoad song.flac | xpuIn2Wav | xpuPlay

# 方法2：使用 && 短路求值
xpuLoad song.flac && \
    xpuIn2Wav && \
    xpuFingerprint && \
    xpuPlay

# 方法3：捕获错误并处理
{ \
    xpuLoad song.flac | xpuIn2Wav | xpuPlay; \
} || { \
    ERROR=$(cat <&0); \
    echo "播放失败: $(echo "$ERROR" | jq -r '.error.message')" >&2; \
    SUGGESTION=$(echo "$ERROR" | jq -r '.error.suggestion'); \
    [ -n "$SUGGESTION" ] && echo "建议: $SUGGESTION" >&2; \
    exit 1; \
}

# 方法4：使用 PIPESTATUS 检查管道中的所有命令
xpuLoad song.flac | xpuIn2Wav | xpuPlay
for status in "${PIPESTATUS[@]}"; do
    if [ $status -ne 0 ]; then
        echo "管道中的命令失败，退出码: $status" >&2
        exit 1
    fi
done
```

#### 4.6.5 错误恢复建议

每个错误码都应该提供恢复建议：

```cpp
std::string getSuggestion(ErrorCode code) {
    switch (code) {
        case ErrorCode::AudioDecodeError:
            return "Try re-encoding the file with ffmpeg: ffmpeg -i input.flac -c:a flac output.flac";

        case ErrorCode::FormatNotSupported:
            return "Convert the file to a supported format: FLAC, WAV, ALAC, or DSD";

        case ErrorCode::SampleRateNotSupported:
            return "Use --rate option to convert to a supported sample rate (44100-768000 Hz)";

        case ErrorCode::CacheCorrupted:
            return "Clear the corrupted cache: xpuCache clear --cache-id <id> and regenerate";

        case ErrorCode::DeviceNotFound:
            return "List available devices: xpuDevice list, then specify with --device option";

        default:
            return "Check the documentation at https://xpu.music/docs/errors";
    }
}
```

#### 4.6.6 模块退出码约定

```bash
# 标准退出码
# 0: 成功
# 1: 通用错误（未分类）
# 2: 使用错误（命令行参数错误）
# 64-127: 特定错误（参考错误码体系）

# 示例
exit 0    # 成功
exit 1    # 通用错误
exit 64   # AudioDecodeError
exit 80   # CacheNotFound

# 模块应该输出错误到 stderr，退出码反映错误类别
xpuIn2Wav song.flac 2>error.json
EXIT_CODE=$?

if [ $EXIT_CODE -eq 64 ]; then
    echo "解码失败" >&2
    jq -r '.error.suggestion' error.json
fi
```

#### 4.6.7 CLI 错误处理工具

提供辅助工具处理错误：

```bash
# xpuError（错误处理辅助工具）
xpuError parse < error.json          # 解析错误信息
xpuError suggest <error.json>        # 获取恢复建议
xpuError explain 0x40                # 解释错误码
xpuError docs 0x40                   # 打开错误文档

# 使用示例
xpuLoad song.flac | xpuIn2Wav 2>error.txt || {
    xpuError explain $(cat error.txt | jq -r '.error.code')
    xpuError suggest < error.txt
}

# 输出示例
# 错误码 0x40 (AudioDecodeError): 音频解码失败
# 建议: Try re-encoding the file with ffmpeg...
```

#### 4.6.8 错误日志记录

```bash
# 错误日志存储位置
~/.local/log/xpu/errors/
├── 2025-01-07.log
├── 2025-01-06.log
└── errors.db          # SQLite 索引

# 日志格式
{
  "timestamp": "2025-01-07T10:30:00Z",
  "level": "ERROR",
  "module": "xpuIn2Wav",
  "error_code": 64,
  "error_name": "AudioDecodeError",
  "message": "Failed to decode audio file",
  "context": {...},
  "stack_trace": "...",
  "pid": 12345,
  "command": "xpuLoad song.flac | xpuIn2Wav"
}

# 查询错误日志
xpuError log --today                  # 今天的错误
xpuError log --category Process       # 特定类别
xpuError log --module xpuIn2Wav       # 特定模块
xpuError log --since 2025-01-01       # 时间范围

# 错误统计
xpuError stats --days 7

# 输出
{
  "total_errors": 152,
  "by_category": {
    "Process": 45,
    "Resource": 38,
    "Cache": 25,
    ...
  },
  "by_module": {
    "xpuIn2Wav": 32,
    "xpuPlay": 28,
    ...
  },
  "top_errors": [
    {"code": 64, "count": 23, "message": "AudioDecodeError"}
  ]
}
```

### 4.7 安全与访问控制

#### 4.7.1 设计目标

XPU 系统设计为单用户本地使用场景，但通过 REST API、MCP 和 Agent-to-Agent 协议暴露接口后，需要基础的安全机制：

- **本地优先**：默认仅监听 localhost，无网络暴露风险
- **可选认证**：远程访问时启用 API Key 或 JWT
- **路径安全**：防止路径遍历攻击
- **资源限制**：防止内存/CPU/磁盘耗尽

#### 4.7.2 认证机制

**默认模式（本地无认证）**：

```bash
# 默认配置：仅监听 localhost，无认证
xpuDaemon --bind 127.0.0.1 --port 8080

# REST API 无需认证即可访问
curl http://localhost:8080/api/v3/status
```

**远程访问模式（API Key 认证）**：

```bash
# 生成 API Key
xpuDaemon --generate-api-key
# 输出: XPU_API_KEY=xpu_sk_20250107_a1b2c3d4e5f6

# 启动时启用认证
export XPU_API_KEY=xpu_sk_20250107_a1b2c3d4e5f6
xpuDaemon --bind 0.0.0.0 --port 8080 --auth-mode apikey

# 客户端请求时携带 API Key
curl -H "X-API-Key: xpu_sk_20250107_a1b2c3d4e5f6" \
     http://remote.host:8080/api/v3/status
```

**JWT Token 认证**（可选，用于多用户场景）：

```bash
# 启用 JWT 认证
xpuDaemon --auth-mode jwt --jwt-secret /path/to/secret.key

# 获取 Token
curl -X POST http://localhost:8080/api/v3/auth/login \
     -H "Content-Type: application/json" \
     -d '{"username": "user", "password": "pass"}'
# 返回: {"token": "eyJhbGciOiJIUzI1NiIs..."}

# 使用 Token 访问
curl -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIs..." \
     http://localhost:8080/api/v3/status
```

#### 4.7.3 API Key 管理

```cpp
// Daemon 中的 API Key 管理器
class ApiKeyManager {
public:
    // 生成新 API Key
    std::string generateKey();

    // 验证 API Key
    bool validateKey(const std::string& key);

    // 撤销 API Key
    void revokeKey(const std::string& key);

    // 列出所有 Keys
    std::vector<ApiKeyInfo> listKeys();

private:
    std::string hashKey(const std::string& key);
    std::unordered_map<std::string, ApiKeyInfo> keys_;
};

// API Key 格式
struct ApiKeyInfo {
    std::string key_id;           // xpu_sk_20250107_a1b2c3
    std::string name;             // "Claude Code"
    std::string created_at;       // "2025-01-07T10:00:00Z"
    std::string last_used;        // "2025-01-07T15:30:00Z"
    std::vector<std::string> scopes;  // ["playback", "queue", "metadata"]
    bool is_revoked = false;
};
```

**API Key 细粒度权限**：

```bash
# 生成仅用于播放控制的 Key
xpuDaemon create-api-key \
    --name "Web Player" \
    --scopes playback,queue,status

# 生成完整权限的 Key（谨慎使用）
xpuDaemon create-api-key \
    --name "Admin" \
    --scopes "*"

# 可用权限范围：
# - playback: 播放控制
# - queue: 队列管理
# - metadata: 元数据读取
# - cache: 缓存管理
# - system: 系统信息
# - admin: 管理功能
```

#### 4.7.4 路径安全验证

**防止路径遍历攻击**：

```cpp
// 路径安全验证器
class PathValidator {
public:
    // 验证并规范化路径
    static std::string validate(const std::string& input_path) {
        // 1. 解析 ~ 为用户主目录
        std::string expanded = expandHome(input_path);

        // 2. 规范化路径（解析 .. 和 .）
        std::filesystem::path canonical_path =
            std::filesystem::weakly_canonical(expanded);

        // 3. 检查是否在允许的目录内
        if (!isAllowedPath(canonical_path)) {
            throw XpuException(ErrorCode::PermissionDenied,
                "Path access denied: " + canonical_path.string())
                .withContext("path", input_path)
                .withSuggestion("Ensure the path is within allowed directories");
        }

        // 4. 检查文件是否存在
        if (!std::filesystem::exists(canonical_path)) {
            throw XpuException(ErrorCode::FileNotFound,
                "File not found: " + canonical_path.string());
        }

        return canonical_path.string();
    }

private:
    static bool isAllowedPath(const std::filesystem::path& path) {
        // 检查配置的允许路径
        for (const auto& allowed_dir : getAllowedDirectories()) {
            // 检查 path 是否在 allowed_dir 或其子目录中
            auto relative = std::filesystem::relative(path, allowed_dir);
            if (!relative.empty() && relative.begin()->string() != "..") {
                return true;
            }
        }
        return false;
    }
};
```

**配置允许的目录**：

```bash
# 配置文件 ~/.config/xpu/allowed_paths.conf
# 允许访问的目录（一行一个）
~/Music
~/Downloads
/media/external/Audio

# 添加允许目录
xpuDaemon add-allowed-path ~/Music

# 列出允许目录
xpuDaemon list-allowed-paths

# 移除允许目录
xpuDaemon remove-allowed-path ~/Downloads
```

#### 4.7.5 请求速率限制

防止 API 滥用和 DoS 攻击：

```cpp
// 速率限制器
class RateLimiter {
public:
    // 检查请求是否允许
    bool checkLimit(const std::string& client_id,
                    const std::string& endpoint);

    // 配置限制规则
    void setLimit(const std::string& endpoint,
                  int max_requests,
                  int time_window_seconds);

private:
    // 滑动窗口算法
    struct RequestRecord {
        std::string client_id;
        std::string endpoint;
        std::chrono::system_clock::time_point timestamp;
    };

    std::deque<RequestRecord> request_history_;
    std::unordered_map<std::string, RateLimit> limits_;
};

// 默认限制规则
struct RateLimit {
    int max_requests;
    int time_window_seconds;
};

// 内置限制
RateLimits = {
    {"POST /api/v3/play", {10, 60}},      // 10次/分钟
    {"GET /api/v3/status", {60, 60}},     // 60次/分钟
    {"POST /api/v3/queue", {20, 60}},     // 20次/分钟
    {"*", {100, 60}}                      // 全局 100次/分钟
};
```

**速率限制响应**：

```json
// 超出限制时的响应
HTTP/1.1 429 Too Many Requests
X-RateLimit-Limit: 10
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1704556860
Retry-After: 30

{
  "error": {
    "code": 73,
    "category": "Network",
    "name": "RateLimitExceeded",
    "message": "Rate limit exceeded for endpoint POST /api/v3/play",
    "context": {
      "limit": 10,
      "window": 60,
      "retry_after": 30
    },
    "suggestion": "Reduce request frequency or increase rate limit"
  }
}
```

#### 4.7.6 安全头配置

```cpp
// HTTP 安全头
class SecurityHeaders {
public:
    static void setSecurityHeaders(http::response& response) {
        response.set("X-Content-Type-Options", "nosniff");
        response.set("X-Frame-Options", "DENY");
        response.set("X-XSS-Protection", "1; mode=block");
        response.set("Strict-Transport-Security",
                     "max-age=31536000; includeSubDomains");
        response.set("Content-Security-Policy",
                     "default-src 'self'; script-src 'self'");
        response.set("Permissions-Policy",
                     "geolocation=(), microphone=(), camera=()");
    }
};
```

#### 4.7.7 资源访问控制

```bash
# CLI 路径验证
xpuPlay ../../etc/passwd
# 错误: PermissionDenied: Path not in allowed directories

# API 请求验证
curl -X POST http://localhost:8080/api/v3/play \
     -H "Content-Type: application/json" \
     -d '{"file": "/etc/passwd"}'
# 错误 403: {"error": {"message": "Path access denied"}}

# 配置允许目录
xpu config set allowed.paths "~/Music,~/Downloads"

# 添加临时允许（仅当前会话）
xpuPlay --allow-path ~/temp/song.flac
```

#### 4.7.8 审计日志

```bash
# 审计日志位置
~/.local/log/xpu/audit/
├── 2025-01-07.log
└── audit.db  # SQLite 索引

# 日志格式
{
  "timestamp": "2025-01-07T15:30:00Z",
  "event": "api.request",
  "client_ip": "127.0.0.1",
  "api_key": "xpu_sk_***a1b2c3",
  "method": "POST",
  "endpoint": "/api/v3/play",
  "params": {"file": "~/Music/song.flac"},
  "status_code": 200,
  "user_agent": "Claude-Code/1.0"
}

# 查询审计日志
xpuDaemon audit --today
xpuDaemon audit --api-key xpu_sk_***a1b2c3
xpuDaemon audit --endpoint "/api/v3/play"

# 审计统计
xpuDaemon audit-stats --days 7
```

#### 4.7.9 TLS/SSL 支持（可选）

```bash
# 为远程访问启用 HTTPS
xpuDaemon --bind 0.0.0.0 --port 8443 \
          --tls-cert /etc/xpu/cert.pem \
          --tls-key /etc/xpu/key.pem

# 生成自签名证书（开发环境）
xpuDaemon --generate-cert

# 客户端请求
curl https://remote.host:8443/api/v3/status
```

#### 4.7.10 安全检查清单

部署前的安全检查：

```bash
# xpuDaemon security-check

✓ API Key 认证: 已启用
✓ 路径验证: 已配置 3 个允许目录
✓ TLS/SSL: 已启用（证书有效期内）
✓ 速率限制: 已启用
✓ 安全头: 已配置
⚠ 允许目录包含 Downloads (审查是否必要)
⚠ 日志文件权限: 644 (建议 600)

修复建议:
1. 移除不必要的允许目录
2. 限制日志文件权限: chmod 600 ~/.local/log/xpu/*
```

## 5. MCP 接口设计

### 5.1 MCP 服务器

MCP 使用 stdio JSON-RPC 2.0 协议，LLM 可以直接调用。

```cpp
class MCPServer {
public:
    // 处理 JSON-RPC 请求
    std::string handleRequest(const std::string& json_request);

private:
    // 工具定义
    json listTools();
    json callTool(const std::string& name, const json& args);

    // 资源定义
    json listResources();
    json readResource(const std::string& uri);

    // 提示词定义
    json listPrompts();
    json getPrompt(const std::string& name);
};
```

### 5.2 MCP Tools 定义

```json
{
  "tools": [
    {
      "name": "xpu_play",
      "description": "Play a music file. Only lossless formats supported: FLAC, WAV, ALAC, DSD (DSF/DSD). Supports up to 768kHz/32-bit.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "file": {
            "type": "string",
            "description": "Path to the audio file to play. Can be absolute path or relative to home directory (~)."
          },
          "volume": {
            "type": "number",
            "minimum": 0,
            "maximum": 1,
            "description": "Volume level (0.0 to 1.0). Default is current volume."
          },
          "device": {
            "type": "string",
            "description": "Output device name or ID. Default is system default."
          },
          "seek": {
            "type": "number",
            "description": "Start position in seconds."
          }
        },
        "required": ["file"]
      }
    },
    {
      "name": "xpu_pause",
      "description": "Pause the current playback.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_resume",
      "description": "Resume paused playback.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_stop",
      "description": "Stop playback and clear the buffer.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_seek",
      "description": "Seek to a specific position in the current track.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "position": {
            "type": "number",
            "description": "Position in seconds. Can also use format like '1:30' for 1 minute 30 seconds."
          }
        },
        "required": ["position"]
      }
    },
    {
      "name": "xpu_queue_add",
      "description": "Add one or more files to the playback queue.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "files": {
            "type": "array",
            "items": {"type": "string"},
            "description": "List of file paths to add."
          },
          "position": {
            "type": "number",
            "description": "Insert position (-1 for end, 0 for beginning). Default is -1."
          }
        },
        "required": ["files"]
      }
    },
    {
      "name": "xpu_queue_list",
      "description": "List all items in the playback queue.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_queue_clear",
      "description": "Clear the playback queue.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_queue_next",
      "description": "Skip to the next track in the queue.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_queue_previous",
      "description": "Go back to the previous track.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_volume_set",
      "description": "Set the playback volume.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "volume": {
            "type": "number",
            "minimum": 0,
            "maximum": 100,
            "description": "Volume level (0 to 100). Can also use percentage like '80%'."
          },
          "fade": {
            "type": "number",
            "description": "Fade duration in milliseconds."
          }
        },
        "required": ["volume"]
      }
    },
    {
      "name": "xpu_mute",
      "description": "Mute or unmute the audio.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "muted": {
            "type": "boolean",
            "description": "True to mute, false to unmute."
          }
        }
      }
    },
    {
      "name": "xpu_get_status",
      "description": "Get current playback status including position, duration, and state.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_get_metadata",
      "description": "Get metadata for a specific file.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "file": {
            "type": "string",
            "description": "Path to the audio file."
          }
        },
        "required": ["file"]
      }
    },
    {
      "name": "xpu_search",
      "description": "Search for music files by metadata.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "query": {
            "type": "string",
            "description": "Search query (e.g., 'artist:Coldplay', 'genre:rock')."
          },
          "in": {
            "type": "string",
            "description": "Directory to search in. Default is music library."
          },
          "limit": {
            "type": "number",
            "description": "Maximum number of results. Default is 20."
          }
        },
        "required": ["query"]
      }
    },
    {
      "name": "xpu_list_devices",
      "description": "List all available audio output devices.",
      "inputSchema": {
        "type": "object",
        "properties": {}
      }
    },
    {
      "name": "xpu_set_device",
      "description": "Set the audio output device.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "device": {
            "type": "string",
            "description": "Device name or ID."
          }
        },
        "required": ["device"]
      }
    },
    {
      "name": "xpu_set_eq",
      "description": "Set the audio equalizer preset or custom settings.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "preset": {
            "type": "string",
            "enum": ["flat", "rock", "pop", "jazz", "classical", "electronic", "bass", "vocal", "speech"],
            "description": "EQ preset name."
          },
          "custom": {
            "type": "string",
            "description": "Custom EQ settings (e.g., '60=+3,250=-2,1000=0,4000=+2,16000=+4')."
          }
        }
      }
    },
    {
      "name": "xpu_set_speed",
      "description": "Set playback speed and pitch.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "speed": {
            "type": "number",
            "minimum": 0.5,
            "maximum": 2.0,
            "description": "Playback speed multiplier (0.5x to 2.0x)."
          },
          "pitch_shift": {
            "type": "number",
            "minimum": -12,
            "maximum": 12,
            "description": "Pitch shift in semitones (-12 to +12)."
          },
          "preserve_pitch": {
            "type": "boolean",
            "description": "Preserve pitch when changing speed."
          }
        }
      }
    },
    {
      "name": "xpu_set_output_format",
      "description": "Set the output audio format for playback. Only lossless formats supported.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "sample_rate": {
            "type": "number",
            "enum": [44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000, 705600, 768000],
            "description": "Sample rate in Hz. Standard: 44.1/48kHz, High: 88.2/96kHz, Ultra: 176.4/192kHz, Extreme: 352.8/384kHz, Master: 705.6/768kHz"
          },
          "bit_depth": {
            "type": "number",
            "enum": [16, 24, 32],
            "description": "Bit depth. 16-bit (CD quality), 24-bit (high resolution), 32-bit (master quality)"
          },
          "auto": {
            "type": "boolean",
            "description": "Auto-detect best format for device."
          }
        }
      }
    },
    {
      "name": "xpu_system_info",
      "description": "Get system audio information including devices, cache status, and version.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "include_devices": {
            "type": "boolean",
            "description": "Include available audio devices in response."
          },
          "include_cache": {
            "type": "boolean",
            "description": "Include cache statistics in response."
          },
          "include_performance": {
            "type": "boolean",
            "description": "Include performance metrics (CPU, memory)."
          }
        }
      }
    },
    {
      "name": "xpu_cache_status",
      "description": "Get detailed cache usage statistics and information.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "type": {
            "type": "string",
            "enum": ["all", "fft", "visual", "fingerprint"],
            "description": "Filter by cache type."
          }
        }
      }
    },
    {
      "name": "xpu_playlist_import",
      "description": "Import a playlist file in various formats (M3U, PLS, XSPF, etc.).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "input": {
            "type": "string",
            "description": "Path or URL to the playlist file."
          },
          "format": {
            "type": "string",
            "enum": ["auto", "m3u", "m3u8", "pls", "xspf", "asx", "json"],
            "description": "Playlist format (auto-detect if not specified)."
          },
          "name": {
            "type": "string",
            "description": "Custom name for the imported playlist."
          }
        },
        "required": ["input"]
      }
    },
    {
      "name": "xpu_playlist_export",
      "description": "Export a playlist to various formats.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "input": {
            "type": "string",
            "description": "Path to the input playlist (JSON format)."
          },
          "output": {
            "type": "string",
            "description": "Path to the output playlist file."
          },
          "format": {
            "type": "string",
            "enum": ["m3u", "m3u8", "pls", "xspf", "asx", "json"],
            "description": "Output format (inferred from output extension if not specified)."
          }
        },
        "required": ["input", "output"]
      }
    }
  ]
}
```

### 5.3 MCP Resources 定义

```json
{
  "resources": [
    {
      "uri": "xpu://status",
      "name": "Playback Status",
      "description": "Current playback status",
      "mimeType": "application/json"
    },
    {
      "uri": "xpu://queue",
      "name": "Playback Queue",
      "description": "Current playback queue",
      "mimeType": "application/json"
    },
    {
      "uri": "xpu://library",
      "name": "Music Library",
      "description": "Complete music library index",
      "mimeType": "application/json"
    },
    {
      "uri": "xpu://devices",
      "name": "Audio Devices",
      "description": "Available audio devices",
      "mimeType": "application/json"
    },
    {
      "uri": "xpu://cache",
      "name": "Cache Status",
      "description": "Cache usage and statistics",
      "mimeType": "application/json"
    },
    {
      "uri": "xpu://system",
      "name": "System Information",
      "description": "System status and performance metrics",
      "mimeType": "application/json"
    }
  ]
}
```

### 5.4 MCP Prompts 定义

```json
{
  "prompts": [
    {
      "name": "dj_assistant",
      "description": "AI DJ assistant that helps manage music playback",
      "arguments": [
        {
          "name": "mood",
          "description": "The mood or atmosphere for the music",
          "required": false
        },
        {
          "name": "activity",
          "description": "The current activity (e.g., 'working', 'exercise')",
          "required": false
        }
      ]
    },
    {
      "name": "music_curator",
      "description": "Helps discover and organize music",
      "arguments": [
        {
          "name": "genre",
          "description": "Music genre to explore",
          "required": false
        },
        {
          "name": "era",
          "description": "Time period (e.g., '90s', '2000s')",
          "required": false
        }
      ]
    }
  ]
}
```

### 5.5 MCP 交互示例

```json
// LLM → MCP Server
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "xpu_play",
    "arguments": {
      "file": "~/Music/song.flac",
      "volume": 0.8
    }
  },
  "id": 1
}

// MCP Server → LLM
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "now_playing": {
      "file": "/home/user/Music/song.flac",
      "title": "Song Title",
      "artist": "Artist Name",
      "duration": 245.8
    },
    "message": "Now playing 'Song Title' by Artist Name"
  },
  "id": 1
}
```

## 6. REST API 设计

### 6.1 OpenAPI 规范

```yaml
openapi: 3.0.0
info:
  title: XPU Music Player API
  version: 3.1.0
  description: AI-Native music player system with local-first design

servers:
  - url: http://localhost:8080/api/v3
    description: Local development (default, no auth)
  - url: https://remote.example.com:8443/api/v3
    description: Remote production (requires API Key or JWT)

security:
  - ApiKeyAuth: []  # 可选：本地无认证，远程需要认证
  - BearerAuth: []  # JWT Token 认证（可选）

paths:
  /play:
    post:
      summary: Play a music file
      security:
        - ApiKeyAuth: []
          BearerAuth: []
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              properties:
                file:
                  type: string
                  description: Path to audio file
                volume:
                  type: number
                  minimum: 0
                  maximum: 1
                  description: Volume level (0.0-1.0)
      responses:
        '200':
          description: Successfully started playback
          headers:
            X-RateLimit-Limit:
              schema:
                type: integer
              description: Request limit per window
            X-RateLimit-Remaining:
              schema:
                type: integer
              description: Remaining requests
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/PlaybackStatus'
        '401':
          $ref: '#/components/responses/Unauthorized'
        '403':
          $ref: '#/components/responses/Forbidden'
        '429':
          $ref: '#/components/responses/RateLimited'

  /pause:
    post:
      summary: Pause playback
      security:
        - ApiKeyAuth: []
          BearerAuth: []
      responses:
        '200':
          description: Playback paused
        '401':
          $ref: '#/components/responses/Unauthorized'

  /resume:
    post:
      summary: Resume playback
      security:
        - ApiKeyAuth: []
          BearerAuth: []
      responses:
        '200':
          description: Playback resumed
        '401':
          $ref: '#/components/responses/Unauthorized'

  /status:
    get:
      summary: Get current status
      responses:
        '200':
          description: Current status
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/PlaybackStatus'

  /queue:
    get:
      summary: Get queue with pagination
      parameters:
        - name: cursor
          in: query
          schema:
            type: string
          description: Cursor for pagination (from next_cursor in previous response)
        - name: limit
          in: query
          schema:
            type: integer
            default: 50
            minimum: 1
            maximum: 500
          description: Number of items per page
      responses:
        '200':
          description: Queue contents (paginated)
          content:
            application/json:
              schema:
                type: object
                properties:
                  items:
                    type: array
                    items:
                      type: object
                      properties:
                        id:
                          type: string
                        file:
                          type: string
                        metadata:
                          type: object
                  total:
                    type: integer
                    description: Total number of items in queue
                  cursor:
                    type: string
                    description: Current cursor position
                  next_cursor:
                    type: string
                    nullable: true
                    description: Cursor for next page (null if last page)
                  has_more:
                    type: boolean
                    description: Whether more items exist
        '400':
          description: Invalid pagination parameters
    post:
      summary: Add to queue
      security:
        - ApiKeyAuth: []
          BearerAuth: []
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              properties:
                files:
                  type: array
                  items:
                    type: string
                position:
                  type: string
                  enum: [next, last]
                  default: next
      responses:
        '200':
          description: Items added to queue
        '401':
          $ref: '#/components/responses/Unauthorized'
        '429':
          $ref: '#/components/responses/RateLimited'

  /queue/{id}:
    delete:
      summary: Remove item from queue
      security:
        - ApiKeyAuth: []
          BearerAuth: []
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: string
      responses:
        '200':
          description: Item removed
        '404':
          description: Item not found

  /auth/login:
    post:
      description: Login and get JWT token (JWT auth mode only)
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              properties:
                username:
                  type: string
                password:
                  type: string
      responses:
        '200':
          description: Login successful
          content:
            application/json:
              schema:
                type: object
                properties:
                  token:
                    type: string
                  expires_in:
                    type: integer
        '401':
          description: Invalid credentials

components:
  securitySchemes:
    ApiKeyAuth:
      type: apiKey
      in: header
      name: X-API-Key
      description: API Key authentication (format: xpu_sk_YYYYMMDD_...)
    BearerAuth:
      type: http
      scheme: bearer
      bearerFormat: JWT
      description: JWT Token authentication

  responses:
    Unauthorized:
      description: Authentication required
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/Error'
          example:
            error:
              code: 70
              category: "Network"
              name: "AuthenticationRequired"
              message: "API Key or JWT token required"
    Forbidden:
      description: Access denied
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/Error'
          example:
            error:
              code: 71
              category: "Network"
              name: "AccessDenied"
              message: "Insufficient permissions for this operation"
              context:
                required_scope: "playback"
                provided_scopes: ["metadata"]
    RateLimited:
      description: Rate limit exceeded
      headers:
        Retry-After:
          schema:
            type: integer
          description: Seconds until retry is allowed
        X-RateLimit-Limit:
          schema:
            type: integer
        X-RateLimit-Remaining:
          schema:
            type: integer
        X-RateLimit-Reset:
          schema:
            type: integer
          description: Unix timestamp when limit resets
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/Error'

  schemas:
    PlaybackStatus:
      type: object
      properties:
        state:
          type: string
          enum: [playing, paused, stopped, idle]
        current_track:
          type: object
          properties:
            file:
              type: string
            title:
              type: string
            artist:
              type: string
            duration:
              type: number
        position:
          type: number
        volume:
          type: number
        queue_length:
          type: integer
    Error:
      type: object
      properties:
        error:
          type: object
          properties:
            code:
              type: integer
            category:
              type: string
            name:
              type: string
            message:
              type: string
            context:
              type: object
            suggestion:
              type: string
```

### 6.2 WebSocket 事件

```javascript
// 客户端连接
const ws = new WebSocket('ws://localhost:8080/api/v3/events');

// 服务器推送事件
{
  "event": "playback.started",
  "data": {
    "file": "/path/to/song.flac",
    "metadata": {...}
  },
  "timestamp": 1704556800123
}

{
  "event": "playback.progress",
  "data": {
    "position": 45.23,
    "duration": 245.8,
    "percent": 18.4
  },
  "timestamp": 1704556800123
}

{
  "event": "playback.ended",
  "data": {
    "file": "/path/to/song.flac"
  },
  "timestamp": 1704556800123
}
```

## 7. Agent-to-Agent 协议

### 7.1 WebSub/PubSub 事件

```yaml
# 主题定义
topics:
  xpu:playback:started:
    description: Emitted when playback starts
    payload:
      file: string
      metadata: object

  xpu:playback:progress:
    description: Emitted periodically during playback
    payload:
      position: number
      duration: number
      buffer: number

  xpu:playback:ended:
    description: Emitted when playback ends
    payload:
      file: string
      reason: string  # "completed", "stopped", "error"

  xpu:queue:changed:
    description: Emitted when queue changes
    payload:
      action: string  # "add", "remove", "clear"
      items: array
```

### 7.2 gRPC 服务定义

```protobuf
service XPUPlayer {
  // 播放控制
  rpc Play(PlayRequest) returns (PlayResponse);
  rpc Pause(PauseRequest) returns (PauseResponse);
  rpc Stop(StopRequest) returns (StopResponse);

  // 队列管理
  rpc AddToQueue(AddRequest) returns (AddResponse);
  rpc GetQueue(QueueRequest) returns (QueueResponse);

  // 信息查询
  rpc GetStatus(StatusRequest) returns (StatusResponse);
  rpc GetMetadata(MetadataRequest) returns (MetadataResponse);

  // 事件流
  rpc SubscribeEvents(SubscriptionRequest) returns (stream Event);
}

message PlayRequest {
  string file = 1;
  optional float volume = 2;
  optional string device = 3;
}

message PlayResponse {
  bool success = 1;
  PlaybackInfo info = 2;
}

message PlaybackInfo {
  string file = 1;
  string title = 2;
  string artist = 3;
  double duration = 4;
}
```

### 7.3 服务发现

```json
{
  "service": "xpu-daemon",
  "version": "3.0.0",
  "endpoints": {
    "mcp": "stdio",
    "http": "http://localhost:8080/api/v3",
    "ws": "ws://localhost:8080/api/v3/events",
    "grpc": "localhost:9090",
    "events": "http://localhost:8080/events"
  },
  "capabilities": [
    "playback",
    "queue",
    "metadata",
    "search"
  ]
}
```

## 8. 使用示例

### 8.1 Claude (MCP) 使用

```
User: 播放我的音乐文件夹中的摇滚音乐

Claude: 我来帮你播放摇滚音乐。

[调用 xpu_search with query="genre:rock" in="~/Music"]

[调用 xpu_queue_add with files from search results]

[调用 xpu_play]

已为您找到并添加 25 首摇滚歌曲到播放队列。正在播放第一首:
"Bohemian Rhapsody" by Queen
```

**更多 AI 交互示例：**

```
User: 我正在学习，帮我播放一些背景音乐，用平坦的均衡器

Claude: 我来为您播放适合学习的背景音乐。

[调用 xpu_search with query="genre:ambient OR genre:classical"]

[调用 xpu_queue_add with files]

[调用 xpu_play]

[调用 xpu_set_eq with preset="flat"]

[调用 xpu_set_output_format with auto=true]

已为您添加 30 首适合学习的背景音乐，使用平坦均衡器。
正在播放: "Gymnopédie No.1" by Erik Satie
```

```
User: 这首播客太慢了，用 1.5 倍速播放

Claude: 我来调整播放速度。

[调用 xpu_set_speed with speed=1.5, preserve_pitch=true]

已将播放速度调整为 1.5 倍，同时保持原始音调。
```

```
User: 我要用蓝牙耳机听音乐

Claude: 我来为蓝牙设备优化音频设置。

[查询设备支持格式]

[调用 xpu_set_device with device="bluetooth"]

[调用 xpu_set_output_format with sample_rate=44100, bit_depth=16]

[调用 xpu_set_eq with preset="vocal"]

已切换到蓝牙设备，使用 44.1kHz/16-bit 格式和人声增强均衡器。
```

### 8.2 Web UI (REST API) 使用

```javascript
// 获取状态
const status = await fetch('/api/v3/status')
  .then(r => r.json());

// 播放歌曲
await fetch('/api/v3/play', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({
    file: '~/Music/song.flac',
    volume: 0.8
  })
});

// 监听事件
const ws = new WebSocket('ws://localhost:8080/api/v3/events');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(`${data.event}:`, data.data);
};
```

### 8.3 Agent-to-Agent 协作

```javascript
// 音乐 Agent 接收智能家居 Agent 的请求
agent.on('xpu:play', async (request) => {
  const { mood, activity } = request;

  // 根据心情和活动选择音乐
  const playlist = await findPlaylist(mood, activity);

  // 播放播放列表
  for (const song of playlist) {
    await daemon.exec('xpu_queue_add', {files: [song]});
  }

  await daemon.exec('xpu_play');

  // 通知其他 Agent
  agent.publish('xpu:playback:started', {
    playlist: playlist.name,
    mood: mood
  });
});
```

### 8.4 直接 CLI 使用

```bash
# 人类直接使用
xpuLoad ~/Music/song.flac | xpuIn2Wav | xpuPlay

# 获取信息
xpuMeta read ~/Music/song.flac

# 搜索音乐
xpuMeta search "artist:Queen" --in ~/Music

# 管理队列
xpuQueue add ~/Music/*.flac
xpuQueue list
```

## 9. 部署

### 9.1 CLI 模块安装

```bash
# 编译安装
cmake -B build
cmake --build build
sudo cmake --install build

# 验证
xpuLoad --version
xpuIn2Wav --version
xpuPlay --version
```

### 9.2 Daemon 启动

```bash
# 启动 Daemon
xpuDaemon start

# 或者前台运行（调试）
xpuDaemon run --foreground

# 查看状态
xpuDaemon status

# 停止
xpuDaemon stop
```

### 9.3 MCP 配置

```json
// Claude Desktop 配置 (~/.claude/config.json)
{
  "mcpServers": {
    "xpu": {
      "command": "/usr/local/bin/xpuDaemon",
      "args": ["--mcp"]
    }
  }
}
```

### 9.4 Docker 部署

```dockerfile
FROM ubuntu:22.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    ffmpeg \
    portaudio19-dev \
    libsamplerate0-dev

# 复制二进制文件
COPY build/xpu* /usr/local/bin/

# 暴露端口
EXPOSE 8080 9090

# 启动 Daemon
CMD ["xpuDaemon", "run", "--api", "--grpc"]
```

## 10. 实际应用案例：Claude Code Skills

本案例展示如何使用 XPU 的 CLI 模块为 Claude Code 设计 Skills，让 Claude Code 能够直接操作和播放音乐文件。

### 10.1 案例概述

**目标：** 让 Claude Code 成为用户的音乐助手，能够：
- 查看和分析音乐文件
- 转换音乐格式
- 创建播放列表
- 播放音乐
- 批量处理音乐文件

**设计方案：**
- 每个 Skill 是一个 Python/Shell 脚本
- Skill 调用 XPU 的 CLI 模块
- 返回结构化的 JSON 结果
- 支持交互式确认

### 10.2 Skills 定义

#### 10.2.1 Skill 文件结构

```
.claude/
└── skills/
    └── music/
        ├── skill.yaml           # Skill 配置
        ├── analyze.py          # 分析音乐文件
        ├── convert.py          # 格式转换
        ├── playlist.py         # 播放列表管理
        ├── play.py             # 播放控制
        ├── batch.py            # 批量处理
        └── utils.py            # 工具函数
```

#### 10.2.2 skill.yaml 配置

```yaml
name: music
description: "Music file operations and playback using XPU (Lossless only)"
version: "1.0.0"
author: "XPU Team"

skills:
  - name: analyze_music
    description: "Analyze music file metadata and properties"
    parameters:
      - name: file
        type: string
        description: "Path to the music file"
        required: true

  - name: convert_music
    description: "Convert music file to different lossless format"
    parameters:
      - name: input
        type: string
        required: true
      - name: output
        type: string
        required: true
      - name: format
        type: string
        enum: ["wav", "flac", "dsd"]
        description: "Target format (lossless only)"
      - name: sample_rate
        type: integer
        description: "Target sample rate (44100-768000 Hz)"
      - name: bit_depth
        type: integer
        enum: [16, 24, 32]
        description: "Target bit depth"

  - name: create_playlist
    description: "Create a playlist from music files"
    parameters:
      - name: name
        type: string
        required: true
      - name: files
        type: array
        items:
          type: string
        required: true
      - name: output
        type: string
        description: "Output playlist file (m3u, m3u8, or json)"

  - name: play_music
    description: "Play music files with options"
    parameters:
      - name: files
        type: array
        items:
          type: string
        required: true
      - name: volume
        type: number
        minimum: 0
        maximum: 100
        description: "Volume level (0-100)"
      - name: eq
        type: string
        enum: ["flat", "rock", "pop", "jazz", "classical", "bass", "vocal"]
        description: "Equalizer preset"
      - name: sample_rate
        type: integer
        description: "Output sample rate (Hz)"
      - name: bit_depth
        type: integer
        enum: [16, 24, 32]
        description: "Output bit depth"
      - name: shuffle
        type: boolean
        description: "Shuffle playback order"

  - name: batch_convert
    description: "Batch convert multiple music files to lossless format"
    parameters:
      - name: input_dir
        type: string
        required: true
      - name: output_dir
        type: string
        required: true
      - name: format
        type: string
        required: true
      - name: pattern
        type: string
        description: "File pattern (e.g., '*.flac')"
      - name: parallel
        type: integer
        description: "Number of parallel conversions"

  - name: normalize_volume
    description: "Normalize volume across multiple files"
    parameters:
      - name: files
        type: array
        items:
          type: string
        required: true
      - name: target_db
        type: number
        description: "Target loudness in dB (-20 to -5)"
```

### 10.3 Skills 实现

#### 10.3.1 analyze.py - 分析音乐文件

```python
#!/usr/bin/env python3
"""
Analyze music file using XPU CLI modules
"""

import subprocess
import json
import sys
from pathlib import Path

def analyze_music(file_path: str) -> dict:
    """Analyze music file and return metadata"""

    # Check if file exists
    if not Path(file_path).exists():
        return {
            "success": False,
            "error": f"File not found: {file_path}"
        }

    # Use xpuMeta to read metadata
    try:
        result = subprocess.run(
            ["xpuMeta", "read", file_path],
            capture_output=True,
            text=True,
            check=True
        )
        metadata = json.loads(result.stdout)
    except subprocess.CalledProcessError as e:
        return {
            "success": False,
            "error": f"Failed to read metadata: {e.stderr}"
        }
    except json.JSONDecodeError:
        return {
            "success": False,
            "error": "Invalid JSON response from xpuMeta"
        }

    # Get audio properties using xpuLoad
    try:
        result = subprocess.run(
            ["xpuLoad", file_path, "--format", "json"],
            capture_output=True,
            text=True,
            check=True
        )
        audio_info = json.loads(result.stdout)
    except subprocess.CalledProcessError:
        audio_info = {}

    # Combine results
    return {
        "success": True,
        "file": file_path,
        "metadata": metadata.get("metadata", {}),
        "audio": {
            "sample_rate": audio_info.get("metadata", {}).get("sample_rate"),
            "bit_depth": audio_info.get("metadata", {}).get("bit_depth"),
            "channels": audio_info.get("metadata", {}).get("channels"),
            "duration": audio_info.get("metadata", {}).get("duration"),
            "codec": audio_info.get("metadata", {}).get("codec"),
            "bitrate": audio_info.get("metadata", {}).get("bitrate")
        },
        "file_size": Path(file_path).stat().st_size,
        "file_size_mb": round(Path(file_path).stat().st_size / 1024 / 1024, 2)
    }

def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: analyze.py <file_path>"
        }))
        sys.exit(1)

    file_path = sys.argv[1]
    result = analyze_music(file_path)
    print(json.dumps(result, indent=2))
    sys.exit(0 if result["success"] else 1)

if __name__ == "__main__":
    main()
```

#### 10.3.2 convert.py - 格式转换

```python
#!/usr/bin/env python3
"""
Convert music files using XPU CLI modules (Lossless only)
"""

import subprocess
import json
import sys
from pathlib import Path

def convert_music(input_file: str, output_file: str,
                  format: str = "wav", sample_rate: int = 96000,
                  bit_depth: int = 24) -> dict:
    """Convert music file to different lossless format"""

    if not Path(input_file).exists():
        return {
            "success": False,
            "error": f"Input file not found: {input_file}"
        }

    # Build the pipeline
    # xpuLoad | xpuIn2Wav --rate <rate> --depth <depth> | xpuOutWave --format <fmt>
    output_cmd = (
        f"xpuLoad \"{input_file}\" | "
        f"xpuIn2Wav --rate {sample_rate} --depth {bit_depth} | "
        f"xpuOutWave --format {format}"
    )

    # Add format-specific options
    if format == "flac":
        output_cmd += " --compression 0"  # Best compression

    # Execute conversion
    try:
        with open(output_file, 'wb') as f:
            result = subprocess.run(
                output_cmd,
                shell=True,
                stdout=f,
                stderr=subprocess.PIPE,
                check=True
            )

        # Get output file size
        output_size = Path(output_file).stat().st_size

        return {
            "success": True,
            "input": input_file,
            "output": output_file,
            "format": format,
            "sample_rate": sample_rate,
            "bit_depth": bit_depth,
            "output_size_mb": round(output_size / 1024 / 1024, 2)
        }

    except subprocess.CalledProcessError as e:
        return {
            "success": False,
            "error": f"Conversion failed: {e.stderr}"
        }
    except Exception as e:
        return {
            "success": False,
            "error": f"Unexpected error: {str(e)}"
        }

def main():
    # Parse arguments
    args = json.loads(sys.argv[1])

    result = convert_music(
        args["input"],
        args["output"],
        args.get("format", "wav"),
        args.get("sample_rate", 96000),
        args.get("bit_depth", 24)
    )

    print(json.dumps(result, indent=2))
    sys.exit(0 if result["success"] else 1)

if __name__ == "__main__":
    main()
```

#### 10.3.3 play.py - 播放控制

```python
#!/usr/bin/env python3
"""
Play music files using XPU with real-time control
"""

import subprocess
import json
import sys
import time
import threading
from pathlib import Path

class MusicPlayer:
    def __init__(self):
        self.process = None
        self.control_pipe = None
        self.playing = False

    def play(self, files: list, volume: float = 0.8,
             eq: str = "flat", shuffle: bool = False) -> dict:
        """Play music files with options"""

        # Validate files
        valid_files = []
        for f in files:
            if Path(f).exists():
                valid_files.append(f)

        if not valid_files:
            return {
                "success": False,
                "error": "No valid files found"
            }

        # Shuffle if requested
        if shuffle:
            import random
            random.shuffle(valid_files)

        # Build pipeline with DSP processing
        # xpuLoad | xpuIn2Wav | xpuProcess --volume <vol> --eq <eq> | xpuOutWave --auto | xpuPlay
        for file in valid_files:
            cmd = (
                f"xpuLoad \"{file}\" | "
                f"xpuIn2Wav | "
                f"xpuProcess --volume {volume} --eq {eq} | "
                f"xpuOutWave --auto | "
                f"xpuPlay"
            )

            try:
                print(f"Playing: {Path(file).name}")
                self.process = subprocess.Popen(
                    cmd,
                    shell=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                )

                self.playing = True
                self.process.wait()
                self.playing = False

            except KeyboardInterrupt:
                if self.process:
                    self.process.terminate()
                break

        return {
            "success": True,
            "files_played": len(valid_files),
            "message": f"Finished playing {len(valid_files)} files"
        }

    def pause(self) -> dict:
        """Pause playback"""
        result = subprocess.run(
            ["xpuPlayCtrl", "pause"],
            capture_output=True,
            text=True
        )
        return {"success": result.returncode == 0}

    def resume(self) -> dict:
        """Resume playback"""
        result = subprocess.run(
            ["xpuPlayCtrl", "resume"],
            capture_output=True,
            text=True
        )
        return {"success": result.returncode == 0}

    def stop(self) -> dict:
        """Stop playback"""
        result = subprocess.run(
            ["xpuPlayCtrl", "stop"],
            capture_output=True,
            text=True
        )
        if self.process:
            self.process.terminate()
        return {"success": result.returncode == 0}

    def status(self) -> dict:
        """Get playback status"""
        result = subprocess.run(
            ["xpuPlayCtrl", "status"],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            return json.loads(result.stdout)
        return {"error": "Failed to get status"}

def main():
    player = MusicPlayer()
    action = sys.argv[1]

    if action == "play":
        args = json.loads(sys.argv[2])
        result = player.play(
            args["files"],
            args.get("volume", 0.8),
            args.get("eq", "flat"),
            args.get("shuffle", False)
        )
    elif action == "pause":
        result = player.pause()
    elif action == "resume":
        result = player.resume()
    elif action == "stop":
        result = player.stop()
    elif action == "status":
        result = player.status()
    else:
        result = {"success": False, "error": f"Unknown action: {action}"}

    print(json.dumps(result, indent=2))
    sys.exit(0 if result.get("success", False) else 1)

if __name__ == "__main__":
    main()
```

#### 10.3.4 batch.py - 批量处理

```python
#!/usr/bin/env python3
"""
Batch process music files using XPU
"""

import subprocess
import json
import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

def batch_convert(input_dir: str, output_dir: str,
                  format: str, pattern: str = "*.flac",
                  parallel: int = 4) -> dict:
    """Batch convert music files"""

    input_path = Path(input_dir)
    output_path = Path(output_dir)

    if not input_path.exists():
        return {
            "success": False,
            "error": f"Input directory not found: {input_dir}"
        }

    # Create output directory
    output_path.mkdir(parents=True, exist_ok=True)

    # Find matching files
    files = list(input_path.glob(pattern))

    if not files:
        return {
            "success": False,
            "error": f"No files found matching pattern: {pattern}"
        }

    results = {
        "success": True,
        "total": len(files),
        "converted": 0,
        "failed": 0,
        "files": []
    }

    def convert_file(input_file):
        output_file = output_path / f"{input_file.stem}.{format}"

        cmd = f"xpuLoad \"{input_file}\" | xpuIn2Wav | xpuOutWave --format {format}"

        try:
            with open(output_file, 'wb') as f:
                subprocess.run(
                    cmd,
                    shell=True,
                    stdout=f,
                    stderr=subprocess.PIPE,
                    check=True
                )

            return {
                "input": str(input_file),
                "output": str(output_file),
                "success": True
            }

        except Exception as e:
            return {
                "input": str(input_file),
                "error": str(e),
                "success": False
            }

    # Convert files in parallel
    with ThreadPoolExecutor(max_workers=parallel) as executor:
        futures = {executor.submit(convert_file, f): f for f in files}

        for future in as_completed(futures):
            result = future.result()
            results["files"].append(result)

            if result["success"]:
                results["converted"] += 1
                print(f"✓ {Path(result['input']).name}")
            else:
                results["failed"] += 1
                print(f"✗ {Path(result['input']).name}: {result.get('error')}")

    return results

def main():
    args = json.loads(sys.argv[1])

    result = batch_convert(
        args["input_dir"],
        args["output_dir"],
        args["format"],
        args.get("pattern", "*.flac"),
        args.get("parallel", 4)
    )

    print(json.dumps(result, indent=2))
    sys.exit(0 if result["success"] else 1)

if __name__ == "__main__":
    main()
```

### 10.4 对话示例

#### 示例 1：分析音乐文件

```
User: 帮我分析一下这个音乐文件的信息

Claude: 我来帮你分析这个文件。

[调用 analyze_music with file="~/Music/song.flac"]

分析结果：
- 文件: song.flac
- 时长: 4:05 (245秒)
- 格式: FLAC (无损)
- 采样率: 44100 Hz
- 位深: 24-bit
- 声道: 立体声
- 比特率: 1024 kbps
- 文件大小: 18.5 MB

元数据：
- 标题: Bohemian Rhapsody
- 艺术家: Queen
- 专辑: A Night at the Opera
- 年份: 1975
- 流派: Rock

需要我对这个文件做什么操作吗？
```

#### 示例 2：批量转换格式

```
User: 把 ~/Music/flac 文件夹里所有的 FLAC 文件转换为高分辨率 FLAC (192kHz/24-bit)

Claude: 我来批量转换这些文件到高分辨率格式。

[扫描文件夹，找到 45 个 FLAC 文件]

[调用 batch_convert with
  input_dir="~/Music/flac",
  output_dir="~/Music/hr_flac",
  format="flac",
  sample_rate=192000,
  bit_depth=24,
  pattern="*.flac",
  parallel=4]

开始转换，使用 4 个并行进程，目标格式：192kHz/24-bit FLAC...

✓ Bohemian Rhapsody.flac → Bohemian Rhapsody.flac (192kHz/24-bit, 52.3 MB)
✓ Stairway to Heaven.flac → Stairway to Heaven.flac (192kHz/24-bit, 61.1 MB)
✓ Hotel California.flac → Hotel California.flac (192kHz/24-bit, 48.7 MB)
...

转换完成！
- 总数: 45 个文件
- 成功: 45 个
- 失败: 0 个
- 总输出大小: 2.34 GB
- 输出格式：192kHz/24-bit FLAC (无损)

所有文件已保存到 ~/Music/hr_flac/
```

#### 示例 3：创建并播放播放列表

```
User: 创建一个工作背景音乐播放列表，包含古典和氛围音乐，然后播放

Claude: 我来为你创建一个工作背景音乐播放列表。

[搜索古典和氛围音乐]

找到 12 首古典音乐和 8 首氛围音乐。

[调用 create_playlist with
  name="work_ambient",
  files=[...20首音乐],
  output="~/Music/playlists/work_ambient.m3u8"]

播放列表已创建: ~/Music/playlists/work_ambient.m3u8

现在开始播放，使用平坦均衡器以保持音质...

[调用 play_music with
  files=playlist,
  eq="flat",
  volume=60]

正在播放: Gymnopédie No.1 - Erik Satie
总时长: 2小时15分钟 (20首歌曲)

需要我调整音量或切换其他音乐吗？
```

#### 示例 4：智能推荐和处理

```
User: 我的音乐文件夹里有很多不同质量的文件，帮我检查一下音质，把低分辨率的升级到高分辨率

Claude: 我来检查并处理你的音乐文件。

[扫描 ~/Music 文件夹]

找到 120 个音乐文件，正在分析音质...

分析结果：
- 顶级音质 (≥352.8kHz/32-bit): 5 个
- 高端音质 (192kHz/24-bit): 18 个
- 标准音质 (96kHz/24-bit): 32 个
- CD音质 (44.1kHz/16-bit): 65 个

检测到 65 个 CD 音质文件。建议升级到高分辨率格式以获得更好的音质体验。
- 目标格式: 96kHz/24-bit FLAC
- 预计空间增加: 约 80%

请确认是否开始转换...

User: 确认，开始转换

[开始批量转换到高分辨率]

正在转换 CD 音质文件到高分辨率...

✓ song_001_16bit.wav → song_001_24bit.flac (45.2 MB → 82.1 MB, 96kHz/24-bit)
✓ song_002_16bit.wav → song_002_24bit.flac (38.7 MB → 69.4 MB, 96kHz/24-bit)
...

转换完成！
- 转换了 65 个文件
- 新增空间: 2.87 GB
- 音质提升: 从 44.1kHz/16-bit 到 96kHz/24-bit (无损)

需要我更新播放列表吗？
```

#### 示例 5：按心情播放

```
User: 我现在很兴奋，帮我播放一些激昂的摇滚音乐

Claude: 我来为你播放一些激昂的摇滚音乐！

[搜索摇滚音乐，按能量和节奏筛选]

找到 30 首高能量的摇滚歌曲。

[调用 play_music with
  files=[筛选后的摇滚歌曲],
  eq="rock",
  volume=80,
  shuffle=true]

正在播放，使用摇滚 EQ 预设和 80% 音量...

🎵 现在播放: "We Will Rock You" - Queen

已添加到播放队列：
1. We Will Rock You - Queen
2. Thunderstruck - AC/DC
3. Born to Run - Bruce Springsteen
4. You Give Love a Bad Name - Bon Jovi
... (共 30 首)

需要我调整音量或切换到其他风格吗？
```

### 10.5 Skills 优势

**XPU CLI 模块让 Claude Code Skills 变得简单：**

| 优势 | 说明 |
|------|------|
| **无需编程** | 直接调用 CLI 工具，无需理解复杂 API |
| **可测试** | 每个 CLI 可独立测试验证 |
| **可组合** | 管道组合实现复杂功能 |
| **可扩展** | 新功能 = 新 CLI 模块 |
| **可维护** | 模块独立，故障隔离 |
| **跨平台** | CLI 工具运行在任何平台 |

**与传统方案对比：**

```
传统方案：
Claude Code → Python 库 (pydub, librosa) → 复杂依赖，难以安装

XPU 方案：
Claude Code → CLI 工具 (xpuLoad, xpuIn2Wav, ...) → 简单，可靠，无损
```

**专业音频支持：**

```
支持的格式：     FLAC, WAV, ALAC, DSD (DSF/DSD)
支持的采样率：   44.1kHz - 768kHz
支持的位深：     16-bit, 24-bit, 32-bit
音频质量：       100% 无损，无任何有损转换
```

## 11. 技术栈

| 层级 | 技术 |
|------|------|
| CLI 模块 | C++17, FFmpeg, PortAudio, libsamplerate |
| Daemon | C++20, asio, nlohmann/json |
| REST API | Crow/Drogon, WebSocket |
| MCP | JSON-RPC 2.0, stdio |
| Agent Protocol | gRPC, WebSub |
| 音频格式 | FLAC, WAV, ALAC, DSD (DSF/DSD) - 仅无损 |
| 构建 | CMake, Conan |
| 测试 | Catch2 |

## 12. 许可证

MIT License
