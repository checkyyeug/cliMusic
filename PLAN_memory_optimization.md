# 内存管理优化计划

## 实施状态总结

**Phase 1: Buffer 预分配** - ✅ 已完成 (2026-01-15)
- xpuIn2Wav streaming 模式已优化
- xpuProcess 已优化
- 预期改进: 减少内存分配次数 90%+, 降低内存碎片

**下一步**: Phase 2 - 消除中间 Buffer

---

## 当前问题分析

### 1. xpuLoad - 大内存复制
**问题**: 将整个 PCM 数据复制到内存中（100MB+）
```cpp
std::vector<uint8_t> pcm_data_copy;  // Store a copy of PCM data
pcm_data_copy = dsd_decoder.getPCMData();  // 复制整个数据
```

**影响**:
- 内存占用: 100MB+ (取决于音频大小)
- 启动延迟: 需要等待整个文件解码完成
- 不必要的复制: getPCMData() 返回 const reference，但还是复制了

### 2. xpuIn2Wav - 频繁的 vector 重新分配
**问题**: 每个分块都重新分配 buffer
```cpp
std::vector<float> input_buffer;
std::vector<float> resampled_buffer;
std::vector<float> output_buffer;
std::vector<uint8_t> write_buffer;

// 每次循环都重新分配
input_buffer.resize(input_samples);
output_buffer = std::move(resampled_buffer);  // move 但还是会分配
remixed.resize(frames * channels);
write_buffer.resize(byte_count);
```

**影响**:
- 频繁的内存分配/释放
- 内存碎片
- 不必要的 memcpy

### 3. xpuProcess - Buffer 重复分配
**问题**: 每次处理都分配新的 buffer
```cpp
std::vector<float> audio_buffer(BUFFER_SIZE);
std::vector<float> processed_buffer(BUFFER_SIZE);

// 每次都重新分配
if (samples > audio_buffer.size()) {
    audio_buffer.resize(samples);
    processed_buffer.resize(samples);
}
```

## 优化方案

### 阶段 1: 预分配和复用 Buffer（简单）

#### xpuIn2Wav 优化
```cpp
// 优化前
std::vector<float> input_buffer;
std::vector<float> resampled_buffer;
std::vector<float> output_buffer;
std::vector<uint8_t> write_buffer;

// 每次循环重新分配
input_buffer.resize(input_samples);
remixed.resize(frames * channels);
write_buffer.resize(byte_count);

// 优化后 - 预分配最大可能需要的 buffer
constexpr size_t MAX_CHUNK_SIZE = 256 * 1024;  // 256KB
constexpr size_t MAX_SAMPLES = MAX_CHUNK_SIZE / sizeof(float);
constexpr size_t MAX_CHANNELS = 8;  // 支持最多 8 声道

std::vector<float> input_buffer;
std::vector<float> resampled_buffer;
std::vector<float> output_buffer;
std::vector<float> remixed_buffer;
std::vector<uint8_t> write_buffer;

// 预分配
input_buffer.reserve(MAX_SAMPLES);
resampled_buffer.reserve(MAX_SAMPLES * 2);  // 重采样可能增加
output_buffer.reserve(MAX_SAMPLES * 2);
remixed_buffer.reserve(MAX_SAMPLES * MAX_CHANNELS);
write_buffer.reserve(MAX_CHUNK_SIZE * 2);  // 32-bit 转 8-bit 可能增加

// 在循环中使用
input_buffer.resize(input_samples);  // 如果小于 reserve，不会重新分配
```

**预期改进**:
- 减少内存分配次数 90%+
- 降低内存碎片
- 减少 20-30% CPU 使用

### 阶段 2: 消除不必要的数据复制（中等）

#### xpuIn2Wav 优化
```cpp
// 优化前 - 多次 memcpy
std::memcpy(&chunk_input_size, size_buffer, 8);
std::vector<uint8_t> pcm_data(data_size);  // 分配
std::memcpy(input_buffer.data(), pcm_data.data(), chunk_input_size);  // 复制

// 优化后 - 直接读取到目标 buffer
input_buffer.resize(input_samples);
if (!std::cin.read(reinterpret_cast<char*>(input_buffer.data()), chunk_input_size)) {
    // 错误处理
}
```

**预期改进**:
- 消除中间 buffer
- 减少 memcpy 次数
- 降低内存占用 50%

### 阶段 3: 流式解码（复杂）

#### xpuLoad 优化
```cpp
// 优化前 - 全部解码到内存
load::AudioFileLoader loader;
loader.load(input_file);
std::vector<uint8_t> pcm_data_copy = loader.getPCMData();  // 100MB+

// 优化后 - 流式解码和输出
class StreamingAudioFileLoader {
public:
    // 分块解码接口
    bool decodeNextChunk(std::vector<uint8_t>& output, size_t max_size);

    // 或者迭代器接口
    class ChunkIterator {
        std::vector<uint8_t> operator*();
        ChunkIterator& operator++();
    };
};

// 使用
StreamingAudioFileLoader loader;
loader.load(input_file);

std::vector<uint8_t> chunk;
while (loader.decodeNextChunk(chunk, CHUNK_SIZE)) {
    // 直接输出 chunk
    std::cout.write(...);
}
```

**预期改进**:
- 内存占用: 100MB → <1MB (降低 99%)
- 启动延迟: 解码第一个 chunk 后立即开始输出
- 更好的缓存局部性

## 实施优先级

### 优先级 1: 预分配 Buffer（立即实施）
- 风险: 低
- 收益: 中等
- 工作量: 小

### 优先级 2: 消除中间 Buffer（短期）
- 风险: 中
- 收益: 中等
- 工作量: 中

### 优先级 3: 流式解码（长期）
- 风险: 高
- 收益: 高
- 工作量: 大
- 需要: 重构 AudioFileLoader 架构

## 性能目标

| 指标 | 当前 | 目标 | 改进 |
|------|------|------|------|
| xpuLoad 内存占用 | ~100MB | <1MB | 99% |
| xpuIn2Wav 内存分配次数 | ~1000次/秒 | ~10次/秒 | 99% |
| 总内存占用 | ~110MB | ~2MB | 98% |
| CPU 使用率 | 5-10% | 3-5% | 50% |

## 测试方法

### 内存测试
```bash
# Windows
powershell "Get-Process xpuLoad | Select-Object WorkingSet64"

# Linux
valgrind --tool=massif xpuLoad song.flac | xpuIn2Wav

# 简单测试
/usr/bin/time -v xpuLoad song.flac | xpuIn2Wav > /dev/null
```

### 性能测试
```bash
# 测试分块处理速度
time xpuLoad large.flac -V | xpuIn2Wav -V > /dev/null

# 测试内存分配
valgrind --tool=massif --massif-out-file=memory.profile xpuLoad song.flac
ms_print memory.profile
```

## 实施计划

### Phase 1: Buffer 预分配（已完成）
1. ✅ 修改 xpuIn2Wav streaming 模式
   - 在 convertStdinToStdoutStreaming() 中添加预分配常量
   - 预分配 input_buffer, resampled_buffer, output_buffer, remixed_buffer, write_buffer
2. ✅ 修改 xpuProcess
   - 预分配 audio_buffer 和 processed_buffer
   - 使用 reserve() 避免频繁重新分配
3. ✅ 测试和验证 - 构建成功

### Phase 2: 消除中间 Buffer（已完成）
1. ✅ xpuIn2Wav 优化
   - 消除 size_buffer，直接读取到 uint64_t（避免 memcpy）
   - 使用 swap() 替代 move() 保留 buffer 容量
2. ✅ xpuProcess 优化
   - 移除不必要的 processed_buffer
   - 直接在 audio_buffer 上进行 DSP 处理
   - 消除每个分块的 memcpy
3. ✅ 测试和验证 - 构建成功

**预期改进**:
- 消除中间 buffer
- 每个 chunk 减少 1-2 次 memcpy
- 降低内存占用 50%

### Phase 3: 流式解码（待实施）
1. 设计 StreamingAudioFileLoader 接口
2. 实现 FFmpeg 流式解码
3. 迁移 xpuLoad 使用新接口
4. 全面测试

## 注意事项

1. **兼容性**: 确保不破坏现有管道协议
2. **错误处理**: 流式模式下错误恢复更复杂
3. **测试**: 需要测试各种音频格式和大小
4. **向后兼容**: 保持批量模式作为备选
