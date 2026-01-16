# xpuIn2Wav 串流重采样实现计划

## 概述

为 xpuIn2Wav 模块添加串流重采样功能，使大文件可以一边从 stdin 读取，一边进行重采样转换，并立即输出到 stdout，显著降低内存占用并提高响应速度。

**分支**: `feature-xpuIn2Wav-streaming-resample`

**当前状态**: 设计完成，待实现

---

## 实现优先级

### Phase 1: 核心功能（MVP）

**目标**: 实现基本的串流重采样功能

#### 任务 1.1: 实现 FormatConverter::convertStdinToStdoutStreaming()

**文件**: `xpu/src/xpuIn2Wav/FormatConverter.cpp`

**步骤**:
1. 在 FormatConverter 类中添加新方法声明（FormatConverter.h）
2. 实现串流处理逻辑：
   - 解析 JSON 元数据（一次性）
   - 读取 8 字节大小头
   - 初始化 StreamingResampler
   - 循环读取 chunk、处理、输出
   - 刷新 resampler 剩余数据

**伪代码**:
```cpp
ErrorCode FormatConverter::convertStdinToStdoutStreaming(
    int sample_rate,
    int bit_depth,
    int channels,
    const char* quality,
    int chunk_size,
    size_t buffer_size
) {
    // 1. 解析 JSON 元数据
    // 2. 读取大小头
    // 3. 初始化 StreamingResampler
    // 4. 循环处理 chunks
    //    while (还有数据) {
    //        读取 chunk_size 帧
    //        StreamingResampler::process()
    //        位深度转换
    //        写入 stdout
    //    }
    // 5. StreamingResampler::flush()
    // 6. 写入最终 JSON 元数据
}
```

#### 任务 1.2: 添加命令行参数解析

**文件**: `xpu/src/xpuIn2Wav/xpuIn2Wav.cpp`

**步骤**:
1. 在 printUsage() 中添加新选项说明
2. 在 main() 中添加参数解析：
   - `--streaming` / `-S`
   - `--chunk-size`

**代码**:
```cpp
bool streaming = false;
int chunk_size = 4096;

// 在参数解析循环中
} else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--streaming") == 0) {
    streaming = true;
} else if (strcmp(argv[i], "--chunk-size") == 0) {
    if (i + 1 < argc) {
        chunk_size = std::atoi(argv[++i]);
    }
}
```

#### 任务 1.3: 调用串流方法

**文件**: `xpu/src/xpuIn2Wav/xpuIn2Wav.cpp`

**修改**: 在管道模式逻辑中根据 streaming 参数选择方法

```cpp
if (output_to_stdout) {
    if (streaming) {
        LOG_INFO("Output mode: stdout (streaming mode)");
        ret = in2wav::FormatConverter::convertStdinToStdoutStreaming(
            output_sample_rate,
            output_bit_depth,
            output_channels,
            quality,
            chunk_size
        );
    } else {
        // 现有批量模式
        ret = in2wav::FormatConverter::convertStdinToStdout(...);
    }
}
```

#### 任务 1.4: 基本测试

**文件**: 创建 `xpu/tests/integration/test_StreamingResample.cpp`

**测试用例**:
1. 基本串流转换（44.1k -> 48k）
2. 无需重采样的串流（44.1k -> 44.1k）
3. 位深度转换
4. 声道转换
5. 错误处理（无效输入、中断）

---

### Phase 2: 优化和完善

#### 任务 2.1: 可配置分块大小

- 支持命令行 `--chunk-size` 参数
- 验证不同分块大小的性能
- 添加分块大小合理性检查

#### 任务 2.2: 缓冲区优化

- 支持自定义输出缓冲区大小 `--buffer-size`
- 实现缓冲区满时自动刷新
- 优化内存使用

#### 任务 2.3: 性能基准测试

**测试指标**:
- 内存占用（批量 vs 串流）
- 首字节延迟
- 总处理时间
- CPU 使用率
- 不同分块大小的性能曲线

**测试文件**: 创建 `xpu/tests/performance/test_StreamingPerformance.cpp`

#### 任务 2.4: 文档完善

- 更新用户手册
- 添加使用示例
- 更新 CLAUDE.md（如有必要）
- 添加性能对比数据

---

### Phase 3: 高级特性（可选）

#### 任务 3.1: 自适应分块大小

根据系统资源动态调整分块大小：
- 检测可用内存
- 检测 CPU 负载
- 自动选择最优分块大小

#### 任务 3.2: 进度报告

输出处理进度信息：
- 已处理帧数 / 总帧数
- 已处理时间 / 预计剩余时间
- 可选：进度条

#### 任务 3.3: 配置文件支持

从 `xpuSetting.conf` 读取默认值：

```toml
[streaming]
enabled = false
chunk_size = 4096
buffer_size = 65536
```

---

## 详细实现设计

### FormatConverter::convertStdinToStdoutStreaming() 详细流程

```cpp
ErrorCode FormatConverter::convertStdinToStdoutStreaming(
    int sample_rate,
    int bit_depth,
    int channels,
    const char* quality,
    int chunk_size,
    size_t buffer_size
) {
    LOG_INFO("Converting stdin to stdout (streaming mode)");
    LOG_INFO("  Target sample rate: {}", sample_rate);
    LOG_INFO("  Target bit depth: {}", bit_depth);
    LOG_INFO("  Target channels: {}", channels);
    LOG_INFO("  Quality: {}", quality);
    LOG_INFO("  Chunk size: {} frames", chunk_size);

    // 设置二进制模式
    #ifdef PLATFORM_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
    #endif

    // ===== 阶段 1: 解析元数据 =====
    std::string json_str;
    // ... 解析 JSON 元数据（复用现有代码） ...

    // 解析输入格式
    int input_sample_rate = 48000;
    int input_channels = 2;
    // ... 从 JSON 解析 ...

    LOG_INFO("Input format: {} Hz, {} channels", input_sample_rate, input_channels);

    // 读取大小头
    uint64_t data_size = 0;
    char size_buffer[8];
    if (!std::cin.read(size_buffer, 8)) {
        LOG_ERROR("Failed to read size header");
        return ErrorCode::InvalidOperation;
    }
    std::memcpy(&data_size, size_buffer, 8);

    LOG_INFO("PCM data size: {} bytes", data_size);

    // 确定输出参数
    int output_sample_rate = (sample_rate > 0) ? sample_rate : input_sample_rate;
    int output_channels = (channels > 0) ? channels : input_channels;

    // ===== 阶段 2: 初始化重采样器 =====
    StreamingResampler resampler;
    if (output_sample_rate != input_sample_rate) {
        ErrorCode ret = resampler.init(
            input_sample_rate,
            output_sample_rate,
            input_channels,
            quality
        );
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Failed to initialize resampler");
            return ret;
        }
        LOG_INFO("Streaming resampler initialized: {} Hz -> {} Hz (ratio={})",
                 input_sample_rate, output_sample_rate, resampler.getRatio());
    }

    // 计算总帧数
    size_t total_input_frames = data_size / (sizeof(float) * input_channels);
    size_t processed_frames = 0;

    // 输入缓冲区（读取 chunk）
    std::vector<float> input_buffer;
    input_buffer.resize(chunk_size * input_channels);

    // 输出缓冲区
    std::vector<float> output_buffer;
    std::vector<uint8_t> write_buffer;

    // ===== 阶段 3: 循环处理 =====
    int chunk_count = 0;
    while (processed_frames < total_input_frames) {
        // 计算本次读取的帧数
        size_t frames_to_read = std::min(chunk_size, total_input_frames - processed_frames);

        // 读取 PCM 数据
        size_t bytes_to_read = frames_to_read * input_channels * sizeof(float);
        if (!std::cin.read(reinterpret_cast<char*>(input_buffer.data()), bytes_to_read)) {
            LOG_ERROR("Failed to read PCM data at frame {}", processed_frames);
            return ErrorCode::FileReadError;
        }

        chunk_count++;

        // 重采样
        if (resampler.isActive()) {
            ErrorCode ret = resampler.process(input_buffer.data(), frames_to_read, output_buffer);
            if (ret != ErrorCode::Success) {
                LOG_ERROR("Resampling failed at chunk {}", chunk_count);
                return ret;
            }

            if (verbose || chunk_count <= 2) {
                LOG_INFO("Processing chunk {}: {} frames -> {} frames",
                         chunk_count, frames_to_read,
                         output_buffer.size() / output_channels);
            }
        } else {
            // 无需重采样，直接复制
            output_buffer.assign(input_buffer.begin(), input_buffer.begin() + frames_to_read * input_channels);
        }

        // 声道转换（如果需要）
        if (output_channels != input_channels) {
            std::vector<float> remixed;
            // ... 声道转换逻辑 ...
            output_buffer = std::move(remixed);
        }

        // 位深度转换
        if (bit_depth != 32) {
            ErrorCode ret = convertBitDepth(output_buffer, 32, bit_depth, write_buffer);
            if (ret != ErrorCode::Success) {
                LOG_ERROR("Bit depth conversion failed");
                return ret;
            }
        } else {
            // 保持 32-bit float
            size_t byte_count = output_buffer.size() * sizeof(float);
            write_buffer.resize(byte_count);
            std::memcpy(write_buffer.data(), output_buffer.data(), byte_count);
        }

        // 写入 stdout
        std::cout.write(reinterpret_cast<const char*>(write_buffer.data()), write_buffer.size());
        std::cout.flush();

        if (!std::cout) {
            LOG_ERROR("Failed to write to stdout");
            return ErrorCode::FileWriteError;
        }

        processed_frames += frames_to_read;
    }

    // ===== 阶段 4: 刷新剩余数据 =====
    if (resampler.isActive()) {
        ErrorCode ret = resampler.flush(output_buffer);
        if (ret != ErrorCode::Success) {
            LOG_ERROR("Resampler flush failed");
            return ret;
        }

        if (!output_buffer.empty()) {
            LOG_INFO("Flushing resampler: {} frames remaining",
                     output_buffer.size() / output_channels);

            // 位深度转换并输出
            if (bit_depth != 32) {
                convertBitDepth(output_buffer, 32, bit_depth, write_buffer);
            } else {
                size_t byte_count = output_buffer.size() * sizeof(float);
                write_buffer.resize(byte_count);
                std::memcpy(write_buffer.data(), output_buffer.data(), byte_count);
            }

            std::cout.write(reinterpret_cast<const char*>(write_buffer.data()), write_buffer.size());
            std::cout.flush();
        }
    }

    // ===== 阶段 5: 输出最终元数据 =====
    // 注意：由于是串流模式，元数据在开始时已经输出（或者不输出）
    // 这里可以选择在 stderr 输出统计信息

    LOG_INFO("Streaming conversion complete:");
    LOG_INFO("  Total input frames: {}", processed_frames);
    LOG_INFO("  Total chunks processed: {}", chunk_count);
    LOG_INFO("  Chunk size: {} frames", chunk_size);

    return ErrorCode::Success;
}
```

---

## 测试计划

### 单元测试

**文件**: `xpu/tests/unit/test_StreamingResampler.cpp`

```cpp
TEST(StreamingResampler, Init) {
    StreamingResampler resampler;
    ASSERT_EQ(resampler.init(44100, 48000, 2, "medium"), ErrorCode::Success);
    EXPECT_TRUE(resampler.isActive());
    EXPECT_NEAR(resampler.getRatio(), 1.088435, 0.0001);
}

TEST(StreamingResampler, ProcessChunk) {
    StreamingResampler resampler;
    resampler.init(44100, 48000, 2, "medium");

    std::vector<float> input(4096 * 2);  // 4096 frames, stereo
    std::vector<float> output;

    ErrorCode ret = resampler.process(input.data(), 4096, output);
    EXPECT_EQ(ret, ErrorCode::Success);
    EXPECT_GT(output.size(), 4096 * 2);  // Output should be larger
}

TEST(StreamingResampler, NoResampling) {
    StreamingResampler resampler;
    resampler.init(48000, 48000, 2, "medium");
    EXPECT_FALSE(resampler.isActive());

    std::vector<float> input(4096 * 2);
    std::vector<float> output;

    resampler.process(input.data(), 4096, output);
    EXPECT_EQ(output.size(), input.size());  // Same size
}

TEST(StreamingResampler, Flush) {
    StreamingResampler resampler;
    resampler.init(44100, 48000, 2, "medium");

    std::vector<float> output;
    ErrorCode ret = resampler.flush(output);
    EXPECT_EQ(ret, ErrorCode::Success);
    EXPECT_GE(output.size(), 0);
}
```

### 集成测试

**文件**: `xpu/tests/integration/test_StreamingResample.cpp`

```cpp
TEST(StreamingResample, BasicPipeline) {
    // 生成测试音频
    system("xpuLoad test_data/sample_44100.flac | xpuIn2Wav - -S -r 48000 > output.raw");

    // 验证输出
    EXPECT_EQ(file_exists("output.raw"), true);
    EXPECT_GE(file_size("output.raw"), 0);
}

TEST(StreamingResample, MemoryUsage) {
    // 测试大文件的内存占用
    size_t mem_before = get_current_memory_usage();
    system("xpuLoad test_data/large_file.flac | xpuIn2Wav - -S > /dev/null");
    size_t mem_after = get_current_memory_usage();

    EXPECT_LT(mem_after - mem_before, 10 * 1024 * 1024);  // < 10MB
}

TEST(StreamingResample, QualityComparison) {
    // 对比串流和批量模式的输出质量
    system("xpuLoad test.flac | xpuIn2Wav - -r 48000 > batch.raw");
    system("xpuLoad test.flac | xpuIn2Wav - -S -r 48000 > stream.raw");

    // 计算两个文件的相关性
    double correlation = compare_audio_files("batch.raw", "stream.raw");
    EXPECT_GT(correlation, 0.999);  // 应该几乎完全相同
}
```

### 性能测试

**文件**: `xpu/tests/performance/test_StreamingPerformance.cpp`

```cpp
TEST(StreamingPerformance, MemoryComparison) {
    struct Stats {
        size_t max_memory;
        double first_byte_latency;
        double total_time;
    };

    Stats batch = measure_performance("xpuLoad test.flac | xpuIn2Wav - -r 48000");
    Stats stream = measure_performance("xpuLoad test.flac | xpuIn2Wav - -S -r 48000");

    printf("批量模式: 内存=%zu MB, 延迟=%.2f ms, 时间=%.2f s\n",
           batch.max_memory / 1024 / 1024,
           batch.first_byte_latency,
           batch.total_time);
    printf("串流模式: 内存=%zu MB, 延迟=%.2f ms, 时间=%.2f s\n",
           stream.max_memory / 1024 / 1024,
           stream.first_byte_latency,
           stream.total_time);

    EXPECT_LT(stream.max_memory, batch.max_memory / 10);  // 内存降低 10x+
    EXPECT_LT(stream.first_byte_latency, batch.first_byte_latency / 10);
}
```

---

## 开发检查清单

### 代码实现

- [ ] FormatConverter::convertStdinToStdoutStreaming() 实现
- [ ] 命令行参数解析（`--streaming`/`-S`, `--chunk-size`）
- [ ] 调用串流方法的逻辑
- [ ] 错误处理和日志记录
- [ ] 帮助文档更新

### 测试

- [ ] 单元测试：StreamingResampler
- [ ] 单元测试：各种分块大小
- [ ] 集成测试：完整管道
- [ ] 集成测试：大文件内存占用
- [ ] 集成测试：错误处理
- [ ] 性能测试：内存对比
- [ ] 性能测试：延迟对比
- [ ] 质量测试：批量 vs 串流

### 文档

- [ ] 更新 DESIGN.md（已完成）
- [ ] 更新 printUsage()
- [ ] 添加使用示例
- [ ] 添加性能基准数据
- [ ] 更新 CLAUDE.md（如需要）

### 代码质量

- [ ] 代码审查
- [ ] 格式化（clang-format）
- [ ] 静态分析（clang-tidy）
- [ ] 内存泄漏检查
- [ ] 跨平台测试（Windows/macOS/Linux）

---

## 预期成果

### 功能目标

1. ✅ 支持串流重采样模式（`--streaming` / `-S`）
2. ✅ 可配置分块大小（`--chunk-size`）
3. ✅ 内存占用降低 95%+（大文件场景）
4. ✅ 首字节延迟降低 98%+
5. ✅ 输出质量与批量模式一致

### 性能目标

| 指标 | 批量模式 | 串流模式 | 改进 |
|------|---------|---------|------|
| 内存占用（5分钟歌曲） | ~50MB | ~256KB | 降低 99.5% |
| 首字节延迟 | ~5-10秒 | <100ms | 降低 98%+ |
| 总处理时间 | 基准 | 基准+5% | 轻微增加 |

### 兼容性目标

- ✅ 默认行为不变（向后兼容）
- ✅ 现有命令和脚本无需修改
- ✅ 跨平台支持（Windows/macOS/Linux）
- ✅ 与现有模块（xpuLoad, xpuPlay）兼容

---

## 风险和缓解

### 风险 1: 输出质量差异

**风险**: 串流处理可能导致输出与批量模式不一致

**缓解**:
- StreamingResampler 使用与批量模式相同的 libsamplerate
- 添加质量对比测试
- 验证输出文件相关性 > 0.999

### 风险 2: 性能下降

**风险**: 小文件可能因额外开销变慢

**缓解**:
- 保持批量模式为默认
- 只在处理大文件时建议使用串流模式
- 性能测试覆盖各种文件大小

### 风险 3: 兼容性问题

**风险**: 某些管道组合可能不兼容

**缓解**:
- 充分的集成测试
- 保守的错误处理
- 详细的日志记录

---

## 时间估算（参考）

| 阶段 | 任务 | 预计时间 |
|------|------|---------|
| Phase 1 | 核心功能实现 | 2-3 天 |
| Phase 1 | 基本测试 | 1 天 |
| Phase 2 | 优化和完善 | 2-3 天 |
| Phase 2 | 性能测试 | 1 天 |
| Phase 3 | 高级特性 | 可选 |
| **总计** | **MVP（Phase 1-2）** | **5-8 天** |

---

## 下一步行动

1. ✅ 完成设计文档（DESIGN.md 已更新）
2. ✅ 创建实现计划（本文档）
3. ⬜ 开始实现 Phase 1 任务
4. ⬜ 提交 PR 进行代码审查
5. ⬜ 合并到主分支

---

**文档版本**: 1.0
**创建日期**: 2026-01-15
**分支**: `feature-xpuIn2Wav-streaming-resample`
**状态**: 设计完成，待实现
