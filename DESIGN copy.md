# XPU AI-Native 音乐播放系统 设计文档 v3.0

## 1. 项目概述

XPU 是一款专为 AI 时代设计的模块化音乐播放系统。每个功能模块都是独立的 CLI 工具，可以被人类直接使用，也可以被 LLM/AI Agent 通过 MCP、API 或 Agent-to-Agent 协议调用。

### 核心特性

- **CLI-First 设计**：每个模块都是独立的可执行文件，可单独使用
- **AI-Native**：内置 MCP (Model Context Protocol) 支持，LLM 可直接调用
- **多层封装**：CLI → REST API → MCP → Agent，逐层抽象
- **完全模块化**：Unix 哲学，一个程序只做一件事
- **跨平台**：Windows, macOS, Linux
- **专业音质**：只支持无损格式（FLAC/WAV/ALAC/DSD），最高支持 768kHz
- **高性能**：C/C++ 实现，极低延迟

### 设计理念

```
人类用户 ←─ 命令行 ──→ CLI 模块
                        ↓
AI Agent ←─ MCP/API ──→ xpuDaemon (语义层)
                        ↓
                   CLI 模块编排
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
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ xpuLoad  │→│ xpuIn2Wav│→│xpuFingerp│→│xpuClassi│→│xpuProcess│→│xpuOutWave│→│ xpuPlay  │ │
│  │ (解析)   │ │ (转WAV)  │ │ (指纹)   │ │ (分类)   │ │ (DSP)    │ │ (输出)   │ │ (播放)   │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                         │
│  │ xpuQueue │ │ xpuMeta  │ │ xpuCache │                         │
│  │ (队列)   │ │ (元数据) │ │ (缓存)   │                         │
│  └──────────┘ └──────────┘ └──────────┘                         │
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

#### 3.2.2 xpuIn2Wav (统一转换为 WAV)

**核心功能：将无损音频文件转换为标准 WAV 格式**

这是音频管道的"统一化"模块，确保后续所有模块只需处理 WAV 一种格式。只支持无损格式：FLAC、WAV、ALAC、DSD。

```bash
# 基本用法（使用配置文件中的参数）
xpuIn2Wav

# 指定输出参数
xpuIn2Wav --rate 96000 --depth 32 --channels 2

# 最高质量模式
xpuIn2Wav --high-quality

# 快速模式（较低质量）
xpuIn2Wav --fast

# 输入 (stdin)
# 来自 xpuLoad 的二进制格式

# 输出 (stdout)
# 标准 WAV 格式 PCM 数据（44字节头部 + 数据）
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
│  5. 输出标准 WAV 格式                                        │
│     ├── 44字节 WAV 头部                                     │
│     └── PCM 音频数据                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
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
```

**设计理念：**

```
输入格式多样性                统一的 WAV 格式
┌──────────────┐              ┌──────────────┐
│   FLAC      │              │              │
│   WAV       │    ┌─────┐    │              │
│   ALAC      │───→│  WAV │───→│   xpuProcess │
│   DSD       │    └─────┘    │  (标准化)    │
│  (DSF/DSD)  │              │              │
└──────────────┘              └──────────────┘
                                  │
                            ↓ xpuIn2Wav 的核心价值
                        "让后续模块只处理 WAV"
```

**为什么这样设计：**

1. **简化后续模块**：xpuProcess、xpuOutWave 只需处理 WAV
2. **保证音质**：统一的无损高质量转换起点
3. **易于缓存**：WAV 格式便于缓存和重用
4. **符合 WAV 规范**：标准格式，兼容性好

#### 3.2.3 xpuFingerprint (音频指纹)

生成音频的唯一指纹标识，用于重复检测、版权识别和音乐匹配。

```bash
# 基本用法
xpuFingerprint [选项]

# 生成指纹（输出到 stdout）
xpuFingerprint

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
# WAV 格式 PCM 数据

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

# 基于分类自动创建播放列表
xpuIn2Wav | xpuFingerprint | xpuClassify | \
    xpuPlaylist --auto-organize
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

#### 3.2.5 xpuProcess (DSP 处理器)

数字信号处理模块，对音频进行实时效果处理。

```bash
# 基本用法
xpuProcess [选项]

# 音量控制
xpuProcess --volume 0.8           # 设置音量 0.0-1.0
xpuProcess --volume +10           # 增加 10dB
xpuProcess --volume -5            # 减少 5dB

# 淡入淡出
xpuProcess --fade-in 2000         # 2秒淡入
xpuProcess --fade-out 3000        # 3秒淡出

# 均衡器
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

# 输入 (stdin)
# WAV 格式 PCM 数据

# 输出 (stdout)
# 处理后的 WAV 格式 PCM 数据
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

# 跨文件渐变
xpuProcess song1.wav --fade-out 2000 > out1.wav
xpuProcess song2.wav --fade-in 2000 > out2.wav
```

#### 3.2.6 xpuOutWave (输出转换器)

将音频转换为输出设备最佳格式的最终转换器。只支持无损格式输出。

```bash
# 基本用法
xpuOutWave [选项]

# 自动检测设备最佳格式
xpuOutWave --auto

# 指定输出采样率和位深
xpuOutWave --rate 192000 --depth 32 --channels 2

# 支持的无损输出格式
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

#### 3.2.7 xpuPlay (音频播放)

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

#### 3.2.8 xpuQueue (队列管理)

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

#### 3.2.9 xpuMeta (元数据管理)

```bash
# 读取元数据
xpuMeta read <file>

# 编辑元数据
xpuMeta edit <file> --set "artist=New Artist"

# 搜索
xpuMeta search <query> [--in <dir>]

# 输出
{
  "results": [
    {"file": "/path/to/song.flac", "match_score": 0.95}
  ]
}
```

#### 3.2.10 xpuDevice (设备管理)

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

### 3.3 组合使用示例

```bash
# 简单播放（完整管道）
xpuLoad song.flac | xpuIn2Wav | xpuFingerprint | xpuClassify | xpuProcess | xpuOutWave | xpuPlay

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

# 重复检测（比较两个文件）
xpuLoad song1.flac | xpuIn2Wav | xpuFingerprint > fp1.json
xpuLoad song2.flac | xpuIn2Wav | xpuFingerprint > fp2.json
xpuMatch fp1.json fp2.json

# 批量分析音乐库
for file in ~/Music/**/*.flac; do
    xpuLoad "$file" | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all
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

    // 模块注册表
    ModuleRegistry module_registry_;
};
```

### 4.2 统一语义层

#### 4.2.1 意图识别

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

#### 4.2.2 参数规范化

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

#### 4.2.3 结果格式化

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

### 4.3 模块编排引擎

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

// Daemon 编排完整管道（包含指纹和分类）
Pipe pipe = orchestrator.buildPipeline({
    "xpuLoad",         // 解析音频文件
    "xpuIn2Wav",       // 重采样到目标格式
    "xpuFingerprint",  // 生成音频指纹
    "xpuClassify",     // 音乐分类（流派、情绪、活动）
    "xpuProcess",      // DSP 处理
    "xpuOutWave",      // 输出格式转换
    "xpuPlay"          // 播放
});

// 等价于命令行：
// xpuLoad file.flac | xpuIn2Wav | xpuFingerprint | xpuClassify --dimension all | xpuProcess | xpuOutWave --auto | xpuPlay

// 执行并返回结果（包含分类信息）
json result = orchestrator.executePipeline(pipe);
// result["classification"] 包含流派、情绪、活动等信息
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
  version: 3.0.0

servers:
  - url: http://localhost:8080/api/v3

paths:
  /play:
    post:
      summary: Play a music file
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              properties:
                file:
                  type: string
                volume:
                  type: number
                  minimum: 0
                  maximum: 1
      responses:
        '200':
          description: Successfully started playback
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/PlaybackStatus'

  /pause:
    post:
      summary: Pause playback
      responses:
        '200':
          description: Playback paused

  /resume:
    post:
      summary: Resume playback
      responses:
        '200':
          description: Playback resumed

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
      summary: Get queue
      responses:
        '200':
          description: Queue contents
    post:
      summary: Add to queue
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                files:
                  type: array
                  items:
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
