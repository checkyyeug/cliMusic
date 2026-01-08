# XPU Architecture Documentation

## System Overview

XPU (Cross-Platform Professional audio playback system) is a modular, professional-grade audio playback system designed for audiophiles and audio professionals.

## Architecture Principles

1. **Modularity**: Each component is a separate executable
2. **Unix Philosophy**: Do one thing well, communicate via stdin/stdout
3. **Cross-Platform**: Support Windows, macOS, Linux
4. **Professional Quality**: Support high-resolution audio (up to 768kHz, 32-bit)
5. **Low Latency**: Target <50ms playback latency
6. **Extensible**: Plugin architecture for Phase 2-5 features

## System Architecture

### 4-Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Layer 1: CLI Interface                   │
│                    (xpuLoad, xpuPlay, etc.)                  │
└────────────────────────┬────────────────────────────────────┘
                         │ JSON over stdin/stdout
┌────────────────────────▼────────────────────────────────────┐
│                   Layer 2: REST API (Phase 2)                │
│                      (HTTP/JSON API)                         │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                  Layer 3: MCP Protocol (Phase 3)             │
│              (Model Context Protocol Integration)             │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                Layer 4: Agent Protocol (Phase 4)             │
│                   (AI Agent Integration)                     │
└─────────────────────────────────────────────────────────────┘
```

## Module Architecture

### Module 1: xpuLoad (Audio File Parser)

**Purpose**: Parse audio files and extract metadata

**Supported Formats**:
- Lossless: FLAC, WAV, ALAC, DSD
- Lossy: MP3, AAC, OGG, OPUS

**Key Components**:
- `AudioFileLoader`: FFmpeg integration
- `DSDDecoder`: DSD format support
- `MetadataExtractor`: Metadata parsing

**Output**: JSON metadata + PCM audio data

### Module 2: xpuIn2Wav (Format Converter + FFT Cache)

**Purpose**: Convert audio to standard format and compute FFT

**Key Features**:
- Format conversion to 48kHz stereo float
- FFT computation with FFTW3
- SHA256-based caching (10-100x speedup)

**Cache Structure**:
```
~/.cache/xpu/fft/
  └── {sha256_hash}/
      ├── meta.json
      ├── magnitude.bin
      ├── phase.bin
      └── config.json
```

**Key Components**:
- `FFTEngine`: FFT computation and caching
- `FormatConverter`: Audio format conversion
- `CacheManager`: Cache persistence

### Module 3: xpuPlay (Low-Latency Audio Output)

**Purpose**: Play audio with <50ms latency

**Platform Support**:
- Windows: WASAPI (exclusive mode)
- macOS: CoreAudio (HAL)
- Linux: ALSA (dmix)

**Key Components**:
- `AudioBackend`: Cross-platform abstraction
- `BufferManager`: Buffer management
- `DeviceManager`: Device enumeration

**Real-Time Status** (10Hz push to stdout):
```json
{
  "state": "playing",
  "position": 123456,
  "buffer_fill": 85,
  "latency_ms": 42.5
}
```

### Module 4: xpuQueue (Queue Management)

**Purpose**: Manage playback queue with persistence

**Playback Modes**:
- Sequential: Play in order
- Random: Shuffle playback
- LoopSingle: Loop single track
- LoopAll: Loop entire queue

**Persistence**: `~/.config/xpu/queue.json`

**Key Components**:
- `QueueManager`: Queue operations
- `QueueStorage`: JSON persistence
- `PlaybackController`: Queue navigation

### Module 5: xpuProcess (DSP Effects)

**Purpose**: Apply digital signal processing

**Effects**:
1. **Volume Control** (0-200%)
   - Linear gain adjustment
   - Soft clipping for volume >100%

2. **Fade Effects**
   - Fade-in: 0% → 100%
   - Fade-out: 100% → 0%
   - Configurable duration

3. **3-Band EQ**
   - Bass: <200 Hz
   - Mid: 200 Hz - 2 kHz
   - Treble: >2 kHz
   - Presets: Flat, Rock, Pop, Classical, Jazz, Electronic
   - Range: -20dB to +20dB per band

**Key Components**:
- `VolumeControl`: Volume adjustment
- `FadeEffects`: Fade in/out
- `Equalizer`: 3-band EQ
- `DSPPipeline`: Effect chaining

### Module 6: xpuDaemon (Background Daemon)

**Purpose**: Orchestrate all modules as background service

**Key Components**:
- `DaemonController`: Daemon lifecycle
- `ProcessManager`: Child process management
- `OrchestrationManager`: Pipeline orchestration
- `ConfigWatcher`: Configuration hot-reload
- `StatePersistence`: State management

**Pipeline**:
```
xpuLoad → xpuIn2Wav → xpuProcess → xpuPlay
```

**Signals**:
- SIGTERM: Graceful shutdown
- SIGINT: Interrupt
- SIGHUP: Reload configuration

## Shared Library (libxpu)

### Protocol Layer

**ErrorCode**: 60+ error codes with HTTP status mapping
- File errors (60-69)
- Audio errors (70-79)
- Cache errors (80-89)
- State errors (90-99)
- Resource errors (100-109)
- Network errors (110-119)

**ErrorResponse**: JSON error format
```cpp
struct ErrorResponse {
    int error_code;
    std::string message;
    std::string timestamp;
    int http_status;
};
```

**Protocol**: Data structures for inter-module communication
```cpp
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    double duration;
    int sample_rate;
    int channels;
    // ...
};

struct PlaybackStatus {
    int state;
    uint64_t position;
    int buffer_fill;
    double latency_ms;
};
```

### Utils Layer

**Logger**: spdlog integration
- Console and file logging
- JSON structured logging
- Log rotation

**PlatformUtils**: Cross-platform paths
- Linux: `~/.config/xpu/`, `~/.cache/xpu/`
- macOS: `~/Library/Application Support/xpu/`
- Windows: `%APPDATA%\xpu\`

**ConfigLoader**: TOML configuration
- Atomic writes (temp → rename)
- Type-safe value access

**ConfigValidator**: Configuration validation
- Range checking
- Type validation

### Audio Layer

**AudioFormat**: Format detection
- Supports: FLAC, WAV, ALAC, DSD, MP3, AAC, OGG, OPUS
- Format-specific properties

**AudioMetadata**: Metadata parsing
- FFmpeg integration
- DSD metadata support

**AudioProperties**: Audio properties
- Bitrate calculation
- Quality detection

### Extension Interfaces (Phase 2-5)

All interfaces use stub pattern:
```cpp
class IAudioFingerprint {
public:
    virtual ErrorCode computeFingerprint(...) = 0;
    virtual bool isAvailable() const = 0;  // Returns false in Phase 1
};

class AudioFingerprintStub : public IAudioFingerprint {
public:
    ErrorCode computeFingerprint(...) override {
        return ErrorCode::NotImplemented;
    }
    bool isAvailable() const override { return false; }
};
```

**Interfaces Defined**:
1. `IAudioFingerprint` (Phase 3)
2. `IAudioClassifier` (Phase 3)
3. `IAudioVisualizer` (Phase 3)
4. `IAdvancedDSP` (Phase 3)
5. `IMetadataProvider` (Phase 3)
6. `IAudioStreamer` (Phase 4)
7. `IDistributedCache` (Phase 4)
8. `INetworkAudio` (Phase 4)

## Data Flow

### Typical Playback Flow

```
1. User: xpuQueue add music.flac
   └─> QueueManager.addTrack()

2. User: xpuQueue play
   └─> OrchestrationManager.startPipeline()

3. Pipeline: xpuLoad music.flac
   └─> AudioFileLoader.load()
       ├─> FFmpeg: Decode audio
       ├─> Extract metadata
       └─> Output: JSON metadata + PCM data

4. Pipeline: xpuIn2Wav
   └─> FFTEngine.computeFFTWithCache()
       ├─> Check cache
       ├─> Compute FFT if needed
       ├─> Save to cache
       └─> Output: FFT data

5. Pipeline: xpuProcess
   └─> DSPPipeline.process()
       ├─> VolumeControl.process()
       ├─> FadeEffects.process()
       └─> Equalizer.process()

6. Pipeline: xpuPlay
   └─> AudioBackend.write()
       ├─> WASAPI/CoreAudio/ALSA
       └─> Output: Audio

7. Status: 10Hz push
   └─> PlaybackStatus to stdout
```

## Performance Characteristics

### FFT Cache Performance

**Target**: 10-100x speedup

- First run: ~30s for 5-minute song
- Cached run: <3s
- Achieved: Framework complete, optimization pending

### Playback Latency

**Target**: <50ms

- Windows (WASAPI): ~42ms with 2048 sample buffer
- macOS (CoreAudio): ~40ms with 2048 sample buffer
- Linux (ALSA): ~45ms with 2048 sample buffer

### Memory Usage

**Target**: <100MB typical

- FFmpeg decoder: ~20MB
- FFT cache: Variable (user-configured)
- Audio buffers: ~10MB
- Queue metadata: <1MB

### CPU Usage

**Target**: <5% for audio playback

- Event-driven backends
- Efficient FFT caching
- Optimized DSP filters

## Cross-Platform Implementation

### Platform Detection

```cpp
#ifdef PLATFORM_WINDOWS
    // Windows-specific code
#elif defined(PLATFORM_MACOS)
    // macOS-specific code
#elif defined(PLATFORM_LINUX)
    // Linux-specific code
#endif
```

### Audio Backend Abstraction

```cpp
// Platform factory
std::unique_ptr<AudioBackend> AudioBackend::create() {
#ifdef PLATFORM_WINDOWS
    return std::make_unique<AudioBackendWASAPI>();
#elif defined(PLATFORM_MACOS)
    return std::make_unique<AudioBackendCoreAudio>();
#elif defined(PLATFORM_LINUX)
    return std::make_unique<AudioBackendALSA>();
#endif
}
```

## Configuration System

### File Locations

- **Linux**: `~/.config/xpu/xpuSetting.conf`
- **macOS**: `~/Library/Application Support/xpu/xpuSetting.conf`
- **Windows**: `%APPDATA%\xpu\xpuSetting.conf`

### Hot Reload

- SIGHUP handler (Unix/Linux/macOS)
- File watcher for configuration changes
- Validation before apply
- Rollback on validation failure
- Module notification system

## Error Handling

### Error Code Design

- 60+ error codes
- HTTP status code mapping
- Structured error responses
- Propagation through pipeline

### Error Response Format

```json
{
  "error_code": 60,
  "message": "File not found",
  "timestamp": "2026-01-08T12:34:56Z",
  "http_status": 404
}
```

## Extensibility

### Feature Status Markers

```cpp
enum class FeatureStatus {
    CORE_V1,        // Phase 1
    API_V1,         // Phase 2
    EXTENDED_V1,    // Phase 3
    DISTRIBUTED_V1, // Phase 4
    ADVANCED_V2,    // Phase 5
    EXPERIMENTAL    // Future
};
```

### Interface Compatibility

- All Phase 2-5 interfaces defined in Phase 1
- Stub implementation (return NotImplemented)
- Zero breaking changes for future phases
- FeatureStatus markers track phase ownership

## Testing Strategy

### Unit Tests

- Test individual components
- Mock dependencies
- Fast execution

### Integration Tests

- Test module interactions
- Test data flow
- Test error propagation

### Contract Tests

- Test interface compliance
- Test error code contracts
- Test data format contracts

### Performance Tests

- Latency benchmarks
- FFT cache benchmarks
- Memory usage tests
- CPU usage tests

## Security Considerations

### File Operations

- Atomic writes (temp → rename)
- Path validation
- Permission checks

### Process Management

- Signal handling
- Privilege separation
- Resource limits

### Data Validation

- Input validation
- Output validation
- Error message sanitization

## Future Enhancements (Phase 2-5)

### Phase 2: REST API
- HTTP/JSON API
- Web interface
- Remote control

### Phase 3: Extended Features
- Audio fingerprinting
- Genre classification
- Visualization
- Advanced DSP
- Metadata enrichment

### Phase 4: Distribution
- Audio streaming
- Distributed cache
- Network audio (DLNA, AirPlay)

### Phase 5: Advanced AI
- Agent protocol
- AI integration
- Smart recommendations

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Development setup
- Code style guide
- Pull request process
- Testing guidelines

## References

- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [PortAudio Documentation](http://www.portaudio.com/docs/)
- [FFTW3 Documentation](https://www.fftw.org/fftw3_doc/)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
