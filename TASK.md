# XPU Phase 1 Implementation Tasks
# 基础播放管道 + 4层架构 - 4周实施计划

**Based on**: PLAN.md Phase 1
**Duration**: 4 weeks (28 days)
**Goal**: 实现 DESIGN.md 定义的完整4层架构基础，专业级音频播放

---

## 📋 Task Summary

- **Total Tasks**: 169 tasks (optimized based on gap analysis)
- **Total Time**: 4 weeks
- **Modules**: 6 core modules (xpuLoad, xpuIn2Wav, xpuPlay, xpuQueue, xpuProcess, xpuDaemon)
- **Key Metrics**: <50ms latency, 768kHz support, 10-100x FFT speedup
- **PLAN.md Coverage**: 95%+ (improved from 85% by addressing gaps)

---

## 🔧 Gap Analysis Fixes (from TASK_vs_PLAN_Phase1_Gaps.md)

This TASK.md has been optimized to address all identified gaps:

### ✅ Critical Fixes (影响Phase 2依赖)

1. **扩展接口预留完整性** (Extension Interface Completeness)
   - **Before**: 60% coverage, incomplete method signatures
   - **After**: 100% coverage with complete C++ interface definitions
   - **Added**:
     - Complete IAudioFingerprint interface with 5 methods
     - Complete IAudioClassifier interface with 4 methods
     - Complete IAudioVisualizer interface with 4 methods
     - Complete IAdvancedDSP interface with 6 methods (Reverb, Chorus, TubeAmp, Phaser, Flanger)
     - Complete IMetadataProvider interface with 3 methods
     - Complete IAudioStreamer interface with 5 methods
     - Complete IDistributedCache interface with 4 methods
     - Complete INetworkAudio interface with 4 methods
     - All interfaces include `isAvailable()` method returning false in Phase 1
     - All methods return ErrorCode::NotImplemented in Phase 1
   - **Impact**: Ensures Phase 2-5 modules can integrate without breaking changes

2. **状态持久化范围增强** (State Persistence Enhancement)
   - **Before**: Basic state persistence with limited scope
   - **After**: Comprehensive state persistence with 6 subtasks
   - **Added**:
     - Playback state persistence (current track, position, mode)
     - Configuration state persistence (device selection, custom settings)
     - State versioning and migration support
     - Cross-platform path handling (Linux/macOS/Windows)
     - Atomic writes with compression
     - Corruption recovery with backup restoration
   - **Impact**: Enables Phase 2 AI integration to access full playback history

### ✅ Important Enhancements (影响用户体验)

3. **实时状态推送机制完善** (Real-Time Status Push Mechanism)
   - **Before**: Basic status output (3 subtasks)
   - **After**: Complete push mechanism (5 subtasks)
   - **Added**:
     - Non-blocking JSON writes to stdout
     - Flush control for real-time delivery
     - Event-driven updates (play, pause, stop, error)
     - Push frequency control (default: 10Hz, configurable)
     - JSON schema validation before output
     - Timestamp synchronization
   - **Impact**: Meets real-time monitoring requirements for Phase 2 API

4. **设备管理完整性** (Device Management Completeness)
   - **Before**: Basic device listing and selection (3 subtasks)
   - **After**: Complete device management (6 subtasks)
   - **Added**:
     - Device capability detection (sample rates, bit depths, channels)
     - Auto-select best format based on capabilities
     - Hot-plug support with state preservation
     - Device fallback mechanism with priority system
     - Configuration-based device ordering
     - Exclusive mode preference
   - **Impact**: Supports professional-grade audio requirements (768kHz, 32-bit)

5. **配置热重载机制** (Configuration Hot-Reload)
   - **Before**: Basic hot-reload (1 subtask)
   - **After**: Complete hot-reload with safety (4 subtasks)
   - **Added**:
     - SIGHUP handler (Unix/Linux/macOS)
     - File watcher for configuration changes
     - Validation before apply with rollback on failure
     - Notify all modules of configuration changes
     - Configuration persistence with backup
   - **Impact**: Enables runtime configuration changes without restart

6. **跨平台路径处理** (Cross-Platform Path Handling)
   - **Before**: Generic cross-platform paths
   - **After**: Explicit paths for each platform
   - **Added**:
     - Queue persistence paths (Linux: ~/.config/xpu/queue.json, etc.)
     - State persistence paths (Linux: ~/.config/xpu/state.json, etc.)
     - Atomic writes (write to temp, then rename)
     - Error handling and retry logic
   - **Impact**: Ensures consistent behavior across all platforms

7. **接口兼容性测试** (Interface Compatibility Tests)
   - **Before**: Basic interface tests (3 subtasks)
   - **After**: Comprehensive compatibility testing (4 subtasks)
   - **Added**:
     - Verify all interfaces return ErrorCode::NotImplemented in Phase 1
     - Verify isAvailable() returns false for Phase 2-5 interfaces
     - Test compilation compatibility with future Phase definitions
     - Test graceful fallback when interfaces are not available
   - **Impact**: Prevents breaking changes when implementing Phase 2-5 features

### 📊 Coverage Improvement Summary

| Area | Before | After | Improvement |
|------|--------|-------|-------------|
| Extension Interfaces | 60% | 100% | +40% |
| State Persistence | 70% | 95% | +25% |
| Real-Time Status | 75% | 95% | +20% |
| Device Management | 70% | 95% | +25% |
| Configuration Hot-Reload | 60% | 95% | +35% |
| Cross-Platform Paths | 70% | 95% | +25% |
| **Overall PLAN.md Coverage** | **85%** | **95%+** | **+10%** |

### 🎯 Task Count Changes

| Week | Before | After | Change | Reason |
|------|--------|-------|--------|--------|
| Week 1 | 68 tasks | 70 tasks | +2 | Enhanced xpuQueue persistence |
| Week 2 | 55 tasks | 60 tasks | +5 | Enhanced xpuPlay device management |
| Week 3 | 47 tasks | 62 tasks | +15 | Complete interface definitions |
| Week 4 | 46 tasks | 46 tasks | 0 | Unchanged |
| **Total** | **156 tasks** | **169 tasks** | **+13** | Gap analysis improvements |

---

## Week 1: Foundation & Core Modules (第1周：基础设施与核心模块)

### Day 1-2: Project Setup & Infrastructure

#### 1.1 Project Structure Setup
- [ ] **1.1.1** Create directory structure for xpu/src/{xpuLoad, xpuIn2Wav, xpuPlay, xpuQueue, xpuProcess, xpuDaemon}
- [ ] **1.1.2** Create xpu/src/lib/{protocol, utils, audio, interfaces}
- [ ] **1.1.3** Create xpu/tests/{unit, integration, contract, benchmark}
- [ ] **1.1.4** Create xpu/{configs, scripts, docs}
- [ ] **1.1.5** Create xpu/.gitignore for C++ projects
- [ ] **1.1.6** Create xpu/README.md with project overview

#### 1.2 Build System Configuration
- [ ] **1.2.1** Create root CMakeLists.txt with cross-platform support
  - Windows (WASAPI), macOS (CoreAudio), Linux (ALSA)
  - C++17 standard
  - Dependency detection (FFmpeg, PortAudio, FFTW3, etc.)
- [ ] **1.2.2** Create xpu/src/lib/CMakeLists.txt for shared library
- [ ] **1.2.3** Create CMakeLists.txt for each module (6 modules)
- [ ] **1.2.4** Create xpu/tests/CMakeLists.txt
- [ ] **1.2.5** Verify CMake configuration on all platforms

#### 1.3 Code Quality Tools Setup
- [ ] **1.3.1** Create .clang-tidy configuration
  - Google C++ Style Guide naming conventions
  - Modern C++ checks (C++17)
- [ ] **1.3.2** Create .clang-format configuration
  - 4-space indentation
  - Google C++ Style
- [ ] **1.3.3** Create .cppcheck configuration
  - C++17 standard
  - Enable all checks
- [ ] **1.3.4** Configure pre-commit hooks for code quality

#### 1.4 Configuration Files
- [ ] **1.4.1** Create xpu/configs/xpuSetting.conf (TOML format)
  - [playback] section with device, sample_rate, channels, buffer_size, latency_ms
  - [fft_cache] section with enabled, cache_dir, max_size_mb, fft_size
  - [queue] section with persistent, queue_file, max_items
  - [logging] section with level, file, rotation
  - [audio_processing] section with resample_quality
- [ ] **1.4.2** Create configuration loader utility
- [ ] **1.4.3** Create configuration validator

#### 1.5 Installation Scripts
- [ ] **1.5.1** Create xpu/scripts/install.sh for Linux/macOS
  - Support apt (Ubuntu/Debian)
  - Support dnf (Fedora)
  - Support pacman (Arch)
  - Support Homebrew (macOS)
- [ ] **1.5.2** Create xpu/scripts/install.ps1 for Windows
  - Support vcpkg
  - Support Conan
- [ ] **1.5.3** Test installation scripts on all platforms
- [ ] **1.5.4** Create xpu/requirements.txt for Python dependencies

#### 1.6 Error Handling Framework
- [ ] **1.6.1** Define ErrorCode enum (30+ error codes)
  - File errors (60-69)
  - Audio errors (70-79)
  - Network errors (72-79)
  - Cache errors (80-89)
  - State errors (90-99)
  - Resource errors (100-109)
- [ ] **1.6.2** Define ErrorResponse struct (JSON format)
- [ ] **1.6.3** Implement error serialization to JSON
- [ ] **1.6.4** Implement error logging system
- [ ] **1.6.5** Create error handling utilities

---

### Day 3-5: xpuLoad Module (音频文件解析器)

#### 2.1 Core Functionality
- [ ] **2.1.1** Implement basic audio file loading
  - Support FLAC format
  - Support WAV format
  - Support ALAC format
- [ ] **2.1.2** Implement FFmpeg integration for decoding
  - FFmpeg AVFormatContext
  - FFmpeg AVCodecContext
  - Audio stream extraction
- [ ] **2.1.3** Implement metadata extraction
  - Title, Artist, Album, Year, Genre
  - Track number, Duration
  - Sample rate, Bit depth, Channels
- [ ] **2.1.4** Implement JSON metadata output to stdout
- [ ] **2.1.5** Implement binary audio data output to stdout
- [ ] **2.1.6** Add support for high-resolution audio (up to 768kHz)

#### 2.2 DSD Format Support
- [ ] **2.2.1** Implement DSD file detection (.dsf, .dsd)
- [ ] **2.2.2** Implement DSD format parsing
  - DSF format (1-bit, 705.6/768kHz)
  - DSD format parsing
- [ ] **2.2.3** Implement DSD to PCM conversion
- [ ] **2.2.4** Add DSD metadata extraction

#### 2.3 CLI Interface
- [ ] **2.3.1** Implement command-line argument parsing
  - Input file path
  - Verbose mode
  - Help message
- [ ] **2.3.2** Implement --help command
- [ ] **2.3.3** Implement --version command
- [ ] **2.3.4** Add input validation
- [ ] **2.3.5** Add progress indicators

#### 2.4 Testing
- [ ] **2.4.1** Unit tests for FLAC loading
- [ ] **2.4.2** Unit tests for WAV loading
- [ ] **2.4.3** Unit tests for ALAC loading
- [ ] **2.4.4** Unit tests for DSD loading
- [ ] **2.4.5** Unit tests for metadata extraction
- [ ] **2.4.6** Integration tests for full pipeline
- [ ] **2.4.7** Performance tests (load time benchmarks)

---

### Day 5-7: xpuIn2Wav Module (格式转换器 + FFT缓存)

#### 3.1 Core Conversion Functionality
- [ ] **3.1.1** Implement WAV format conversion
  - PCM data processing
  - WAV header generation (44 bytes)
  - Multi-channel support
- [ ] **3.1.2** Implement resampling support
  - 44.1kHz, 48kHz, 96kHz, 192kHz, 384kHz, 768kHz
  - libsamplerate integration
  - Quality settings (sinc_best, sinc_medium, sinc_fastest)
- [ ] **3.1.3** Implement bit depth conversion
  - 16-bit, 24-bit, 32-bit
  - Dithering support
- [ ] **3.1.4** Implement channel configuration
  - Mono, Stereo, Multi-channel
- [ ] **3.1.5** Optimize conversion pipeline for performance

#### 3.2 FFT Cache Implementation (Core Performance Module)
- [ ] **3.2.1** Design FFT cache file structure
  - meta.json (metadata)
  - magnitude.bin (amplitude spectrum)
  - phase.bin (phase spectrum)
  - config.json (FFT parameters)
- [ ] **3.2.2** Implement FFT computation using FFTW3
  - FFT size configuration (2048 default)
  - Overlap ratio (0.5 default)
  - Window function (Hann/Hamming)
- [ ] **3.2.3** Implement magnitude spectrum calculation
  - dB conversion
  - Frequency range: 20Hz-20kHz
- [ ] **3.2.4** Implement phase spectrum calculation
  - Phase unwrapping
  - Phase coherence
- [ ] **3.2.5** Implement cache file writing
  - Binary format for efficiency
  - Endianness handling
- [ ] **3.2.6** Implement cache file reading
  - Validation and integrity checks
- [ ] **3.2.7** Implement cache key generation
  - Based on file content hash
  - Include parameters in key

#### 3.3 Cache Management
- [ ] **3.3.1** Implement cache existence check
  - Fast lookup before computation
- [ ] **3.3.2** Implement cache validation
  - Version compatibility check
  - Parameter match check
- [ ] **3.3.3** Implement cache reuse mechanism
  - Skip FFT if cache exists and is valid
  - Performance target: 10-100x speedup
- [ ] **3.3.4** Implement cache statistics tracking
  - Hit rate monitoring
  - Computation time logging
- [ ] **3.3.5** Add cache directory management
  - Create directories if needed
  - Cross-platform paths (Linux/macOS/Windows)

#### 3.4 Performance Optimization
- [ ] **3.4.1** Optimize FFT computation for performance
  - Target: <30s for 5-minute song on first run
  - Target: <3s on cache hit (10x speedup minimum, 10-100x target)
- [ ] **3.4.2** Implement multi-threading for FFT computation
  - Parallel frame processing
  - Thread pool management
- [ ] **3.4.3** Memory optimization
  - Streaming processing for large files
  - Memory usage monitoring
- [ ] **3.4.4** Profile and optimize hotspots
- [ ] **3.4.5** SIMD optimizations (if applicable)

#### 3.5 CLI Interface
- [ ] **3.5.1** Command-line argument parsing
  - Input from stdin
  - Output format options
  - FFT parameters
- [ ] **3.5.2** Implement --cache-dir option
- [ ] **3.5.3** Implement --fft-size option
- [ ] **3.5.4** Implement --rate option (sample rate)
- [ ] **3.5.5** Implement --quality option (resampling quality)
- [ ] **3.5.6** Implement --force option (bypass cache)
- [ ] **3.5.7** Add progress indicators for long operations

#### 3.6 Testing
- [ ] **3.6.1** Unit tests for format conversion
- [ ] **3.6.2** Unit tests for resampling
- [ ] **3.6.3** Unit tests for FFT computation
- [ ] **3.6.4** Unit tests for cache operations
- [ ] **3.6.5** Unit tests for cache reuse mechanism
- [ ] **3.6.6** Integration tests for full pipeline
- [ ] **3.6.7** Performance benchmarks
  - Measure FFT computation time
  - Measure cache hit/miss times
  - Verify 10-100x speedup target
- [ ] **3.6.8** Cross-platform tests (Windows/macOS/Linux)

---

## Week 2: Audio Output & Queue Management (第2周：音频输出与队列管理)

### Day 8-10: xpuPlay Module (音频输出模块)

#### 4.1 Audio Backend Implementation
- [ ] **4.1.1** Windows: WASAPI implementation
  - Exclusive mode for low latency
  - Device enumeration
  - Format negotiation (768kHz, 32-bit)
- [ ] **4.1.2** macOS: CoreAudio implementation
  - Hog mode for low latency
  - AudioDeviceIO
  - Format negotiation
- [ ] **4.1.3** Linux: ALSA implementation
  - Direct hw:0,0 access
  - PCM configuration
  - Format negotiation
- [ ] **4.1.4** Cross-platform abstraction layer
  - Unified interface
  - Platform-specific implementations

#### 4.2 Low-Latency Playback
- [ ] **4.2.1** Implement buffer management
  - Double buffering
  - Triple buffering option
  - Buffer size: 2048 samples default
- [ ] **4.2.2** Optimize for <50ms latency target
  - Reduce buffer sizes
  - Minimize processing overhead
  - Profile and optimize
- [ ] **4.2.3** Implement underrun detection
  - Recover from underruns
  - Graceful degradation
- [ ] **4.2.4** Latency measurement
  - Implement --latency-test option
  - Measure end-to-end latency
  - Verify <50ms target

#### 4.3 High-Resolution Audio Support
- [ ] **4.3.1** Implement 768kHz support
  - Buffer sizing for high sample rates
  - Memory optimization
- [ ] **4.3.2** Implement 32-bit support
  - Data type handling
  - Precision preservation
- [ ] **4.3.3** Implement device capability detection
  - Query supported formats
  - Auto-select best format
  - Fallback to lower quality if needed

#### 4.4 Real-Time Status Output
- [ ] **4.4.1** Implement JSON status output to stdout
  - Current position
  - Playback state
  - Buffer status
  - CPU usage
  - Sample rate, bit depth
- [ ] **4.4.2** Implement heartbeat mechanism
  - Regular status updates (configurable interval)
  - Watchdog monitoring
  - Timeout detection
- [ ] **4.4.3** Implement real-time push mechanism
  - Non-blocking JSON writes to stdout
  - Flush control for real-time delivery
  - Buffer status synchronization
  - Event-driven updates (play, pause, stop, error)
  - Push frequency control (default: 10Hz, configurable)
- [ ] **4.4.4** Add timing information
  - Playback time (current position in seconds)
  - Buffer fill level (percentage)
  - Estimated time remaining
- [ ] **4.4.5** Implement status data validation
  - JSON schema validation before output
  - Field completeness checks
  - Timestamp synchronization

#### 4.5 Device Management
- [ ] **4.5.1** Implement device listing
  - --list-devices option
  - Device capabilities (sample rates, bit depths, channels)
  - Device names and IDs
- [ ] **4.5.2** Implement device selection
  - --device option
  - Device validation
  - Default device selection
- [ ] **4.5.3** Implement device capability detection
  - Query supported sample rates (44.1kHz - 768kHz)
  - Query supported bit depths (16/24/32-bit)
  - Query supported channel configurations
  - Auto-select best format based on capabilities
- [ ] **4.5.4** Implement device hot-plug support
  - Detect device connection/disconnection
  - Graceful switching on hot-plug
  - State preservation across device changes
- [ ] **4.5.5** Implement device fallback mechanism
  - Priority-based device selection
  - Automatic fallback to next available device
  - User notification on device switch
- [ ] **4.5.6** Implement device priority system
  - Configuration-based device ordering
  - Exclusive mode preference
  - Capability-based ranking

#### 4.6 CLI Interface
- [ ] **4.6.1** Command-line argument parsing
  - Input from stdin
  - Device selection
  - Buffer size
- [ ] **4.6.2** Implement --device option
- [ ] **4.6.3** Implement --buffer-size option
- [ ] **4.6.4** Implement --latency-test option
- [ ] **4.6.5** Add volume control options

#### 4.7 Testing
- [ ] **4.7.1** Unit tests for WASAPI backend
- [ ] **4.7.2** Unit tests for CoreAudio backend
- [ ] **4.7.3** Unit tests for ALSA backend
- [ ] **4.7.4** Integration tests for full playback
- [ ] **4.7.5** Latency tests (verify <50ms)
- [ ] **4.7.6** High-resolution audio tests (768kHz, 32-bit)
- [ ] **4.7.7** Device management tests
- [ ] **4.7.8** Cross-platform tests

---

### Day 11-12: xpuQueue Module (队列管理)

#### 5.1 Queue Data Structure
- [ ] **5.1.1** Design queue data structure
  - Song metadata
  - Position tracking
  - Playback history
- [ ] **5.1.2** Implement queue storage
  - In-memory queue
  - Thread-safe operations
  - Mutex protection
- [ ] **5.1.3** Implement queue operations
  - Add song
  - Remove song (by index)
  - Clear queue
  - Get queue size
- [ ] **5.1.4** Implement queue navigation
  - Next song
  - Previous song
  - Jump to index
  - Current position

#### 5.2 Queue Persistence
- [ ] **5.2.1** Design persistence format (JSON)
  - Queue metadata
  - Song list
  - Playback state
  - Queue version
- [ ] **5.2.2** Implement queue save to disk
  - Auto-save on changes
  - Atomic writes (write to temp, then rename)
  - Error handling and retry logic
- [ ] **5.2.3** Implement queue load from disk
  - Validation on load
  - Migration from old formats
  - Error recovery
- [ ] **5.2.4** Add configuration for queue file location
  - Cross-platform paths:
    - Linux: ~/.config/xpu/queue.json
    - macOS: ~/Library/Application Support/xpu/queue.json
    - Windows: %APPDATA%\xpu\queue.json
  - Default locations
  - Custom path configuration

#### 5.3 Playback Modes
- [ ] **5.3.1** Implement sequential playback
  - Play in order
  - Auto-advance
- [ ] **5.3.2** Implement random playback
  - Shuffle algorithm
  - Seed for reproducibility
- [ ] **5.3.3** Implement loop modes
  - Loop single track
  - Loop entire queue
  - Loop disabled

#### 5.4 CLI Interface
- [ ] **5.4.1** Implement xpuQueue command
  - Subcommands: add, remove, list, clear, next, previous
- [ ] **5.4.2** Implement add command
  - Single file
  - Multiple files (glob pattern)
  - Batch add
- [ ] **5.4.3** Implement remove command
  - Remove by index
  - Remove by song title
- [ ] **5.4.4** Implement list command
  - JSON output
  - Formatted table output
  - Current playing indicator
- [ ] **5.4.5** Implement clear command
- [ ] **5.4.6** Implement next/previous commands
- [ ] **5.4.7** Implement play/pause/stop commands
- [ ] **5.4.8** Implement shuffle/loop commands

#### 5.5 Testing
- [ ] **5.5.1** Unit tests for queue operations
- [ ] **5.5.2** Unit tests for persistence
- [ ] **5.5.3** Unit tests for playback modes
- [ ] **5.5.4** Integration tests with xpuPlay
- [ ] **5.5.5** Performance tests (large queues)

---

### Day 13-14: xpuProcess Module (基础DSP处理)

#### 6.1 Volume Control
- [ ] **6.1.1** Implement volume adjustment (0-200%)
  - Linear amplitude scaling
  - Prevent clipping
- [ ] **6.1.2** Implement --volume option
  - Percentage input
  - Normalized values (0.0-2.0)
- [ ] **6.1.3** Add volume validation
  - Range checking
  - Error messages

#### 6.2 Fade Effects
- [ ] **6.2.1** Implement fade-in effect
  - Linear fade
  - Configurable duration (ms)
  - --fade-in option
- [ ] **6.2.2** Implement fade-out effect
  - Linear fade
  - Configurable duration
  - --fade-out option
- [ ] **6.2.3** Implement cross-fade
  - For future use in queue transitions

#### 6.3 Basic Equalizer
- [ ] **6.3.1** Implement 3-band EQ
  - Bass (low frequencies)
  - Mid (mid frequencies)
  - Treble (high frequencies)
- [ ] **6.3.2** Implement gain control for each band
  - Range: -20dB to +20dB
  - Default: 0dB (flat)
- [ ] **6.3.3** Implement --eq option
  - Preset names (rock, pop, classical, jazz, flat)
  - Custom gain values
- [ ] **6.3.4** Implement EQ presets
  - Define common presets
  - Save/load custom presets

#### 6.4 DSP Pipeline
- [ ] **6.4.1** Design DSP processing chain
  - Volume → EQ → Fade
  - Modular architecture
- [ ] **6.4.2** Implement streaming processing
  - Process in chunks
  - Low memory footprint
- [ ] **6.4.3** Implement effect chaining
  - Multiple effects in sequence
  - Order matters

#### 6.5 CLI Interface
- [ ] **6.5.1** Implement xpuProcess command
  - Input from stdin
  - Output to stdout
- [ ] **6.5.2** Implement --volume option
- [ ] **6.5.3** Implement --fade-in option
- [ ] **6.5.4** Implement --fade-out option
- [ ] **6.5.5** Implement --eq option
- [ ] **6.5.6** Add help and version commands

#### 6.6 Testing
- [ ] **6.6.1** Unit tests for volume control
- [ ] **6.6.2** Unit tests for fade effects
- [ ] **6.6.3** Unit tests for EQ
- [ ] **6.6.4** Unit tests for DSP pipeline
- [ ] **6.6.5** Integration tests with xpuPlay
- [ ] **6.6.6** Quality tests (audio output quality)

---

## Week 3: Daemon & Integration (第3周：守护进程与集成)

### Day 15-17: xpuDaemon Module (基础守护进程)

#### 7.1 Process Lifecycle Management
- [ ] **7.1.1** Implement daemon initialization
  - PID file creation
  - Signal handling (SIGTERM, SIGINT, SIGHUP)
  - Detach from terminal
- [ ] **7.1.2** Implement graceful shutdown
  - Signal handlers
  - Cleanup on exit
  - Resource release
- [ ] **7.1.3** Implement child process spawning
  - Fork/exec modules
  - Process tracking
  - PID management
- [ ] **7.1.4** Implement process monitoring
  - Watch child processes
  - Detect crashes
  - Auto-restart (optional)

#### 7.2 Module Orchestration
- [ ] **7.2.1** Design pipeline orchestration
  - xpuLoad → xpuIn2Wav → xpuPlay
  - Pipe creation
  - Process chaining
- [ ] **7.2.2** Implement basic orchestrator
  - Execute pipeline
  - Monitor pipeline health
  - Handle failures
- [ ] **7.2.3** Implement module communication
  - stdin/stdout pipe
  - Error propagation
  - Exit code handling
- [ ] **7.2.4** Implement cleanup mechanisms
  - Pipe cleanup
  - Process termination
  - Resource release

#### 7.3 Configuration Management
- [ ] **7.3.1** Load configuration from file
  - Read xpuSetting.conf
  - Validate configuration
  - Apply defaults
- [ ] **7.3.2** Implement configuration hot-reload
  - SIGHUP handler (Unix/Linux/macOS)
  - File watcher for configuration changes
  - Reload on change detection
  - Validate before apply
  - Rollback on validation failure
  - Notify all modules of configuration changes
- [ ] **7.3.3** Implement configuration validation
  - Check required fields
  - Validate ranges (sample rates, buffer sizes, etc.)
  - Validate device names
  - Validate paths
  - Error messages with specific field information
- [ ] **7.3.4** Implement configuration persistence
  - Save configuration changes
  - Atomic write operations
  - Backup previous configuration

#### 7.4 Logging System
- [ ] **7.4.1** Implement logging infrastructure
  - spdlog integration
  - Log levels (trace, debug, info, warning, error, critical)
- [ ] **7.4.2** Implement JSON format logging
  - Structured logs
  - Machine-readable
- [ ] **7.4.3** Implement log rotation
  - Size-based rotation
  - Time-based rotation
  - Compress old logs
- [ ] **7.4.4** Configure log outputs
  - Console output (stderr)
  - File output
  - Log location configuration

#### 7.5 State Persistence
- [ ] **7.5.1** Design state file format
  - Current playback state (playing/paused/stopped, position, current track)
  - Queue state (current queue index, playlist)
  - Configuration state (last used settings)
  - State version for migration
  - Cross-platform paths:
    - Linux: ~/.config/xpu/state.json
    - macOS: ~/Library/Application Support/xpu/state.json
    - Windows: %APPDATA%\xpu\state.json
- [ ] **7.5.2** Implement state save
  - Auto-save on state change (play, pause, stop, skip)
  - Atomic writes (write to temp, then rename)
  - Compression for large states
  - Error handling and recovery
- [ ] **7.5.3** Implement state load
  - Load on daemon startup
  - Validate state (schema validation)
  - Recovery from corruption (backup restoration)
  - Migration from older state versions
  - Apply last known playback state
- [ ] **7.5.4** Add state versioning
  - Version field in state
  - Migration support between versions
  - Backward compatibility checks
- [ ] **7.5.5** Implement playback state persistence
  - Save current track and position
  - Save playback mode (shuffle/loop)
  - Save volume and EQ settings
  - Restore on startup
- [ ] **7.5.6** Implement configuration state persistence
  - Save last configuration changes
  - Save device selection
  - Save custom settings
  - Restore on startup

#### 7.6 CLI Interface
- [ ] **7.6.1** Implement xpuDaemon command
  - --daemon option (run as daemon)
  - --foreground option (run in foreground)
  - --config option
  - --verbose option
- [ ] **7.6.2** Implement --status command
- [ ] **7.6.3** Implement --stop command
- [ ] **7.6.4** Implement --restart command
- [ ] **7.6.5** Implement --reload command

#### 7.7 Testing
- [ ] **7.7.1** Unit tests for daemon lifecycle
- [ ] **7.7.2** Unit tests for orchestration
- [ ] **7.7.3** Unit tests for configuration
- [ ] **7.7.4** Unit tests for logging
- [ ] **7.7.5** Unit tests for state persistence
- [ ] **7.7.6** Integration tests for full daemon
- [ ] **7.7.7** Crash recovery tests
- [ ] **7.7.8** Signal handling tests

---

### Day 18-19: Shared Library & Protocol

#### 8.1 Shared Library (libxpu)
- [ ] **8.1.1** Implement protocol utilities
  - JSON serialization
  - JSON deserialization
  - Error serialization
- [ ] **8.1.2** Implement utilities
  - Logging wrapper
  - Configuration loader
  - Platform abstraction
- [ ] **8.1.3** Implement audio wrappers
  - Audio format detection
  - Audio metadata
  - Audio properties
- [ ] **8.1.4** Implement extension interface stubs (critical for Phase 2-5 compatibility)
  - IAudioFingerprint interface stub (Phase 3)
  - IAudioClassifier interface stub (Phase 3)
  - IAudioVisualizer interface stub (Phase 3)
  - IAudioStreamer interface stub (Phase 4)
  - IAdvancedDSP interface stub (Phase 3)
  - IDistributedCache interface stub (Phase 4)
  - INetworkAudio interface stub (Phase 4)
  - IMetadataProvider interface stub (Phase 3)
  - All interfaces return ErrorCode::NotImplemented in Phase 1

#### 8.2 Cross-Platform Abstraction
- [ ] **8.2.1** Implement platform detection
  - Compile-time detection
  - Runtime detection
- [ ] **8.2.2** Implement path abstraction
  - Linux: ~/.config/xpu/
  - macOS: ~/Library/Application Support/xpu/
  - Windows: %APPDATA%\xpu\
- [ ] **8.2.3** Implement audio backend abstraction
  - Unified interface
  - Platform-specific implementations

#### 8.3 Protocol Definition
- [ ] **8.3.1** Define CLI protocol
  - JSON metadata format
  - Binary audio format
  - Error format
- [ ] **8.3.2** Document inter-module communication
  - Pipe protocol
  - Error propagation
  - Exit codes
- [ ] **8.3.3** Create protocol documentation
  - Developer guide
  - Examples

#### 8.4 Testing
- [ ] **8.4.1** Unit tests for protocol utilities
- [ ] **8.4.2** Unit tests for platform abstraction
- [ ] **8.4.3** Unit tests for audio wrappers
- [ ] **8.4.4** Cross-platform tests

---

### Day 20-21: Extension Interfaces

#### 9.1 Phase 3 Extension Interfaces (预留接口)
- [ ] **9.1.1** Define IAudioFingerprint interface (complete definition)
  ```cpp
  class IAudioFingerprint {
      virtual ErrorCode computeFingerprint(const std::string& audio_file, FingerprintData& result) = 0;
      virtual ErrorCode fingerprintFromCache(const std::string& cache_id, FingerprintData& result) = 0;
      virtual ErrorCode compareFingerprints(const FingerprintData& fp1, const FingerprintData& fp2, float& similarity) = 0;
      virtual ErrorCode queryOnlineDatabase(const FingerprintData& fp, Metadata& metadata) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.1.2** Define IAudioClassifier interface (complete definition)
  ```cpp
  class IAudioClassifier {
      virtual ErrorCode classify(const std::string& audio_file, ClassificationResult& result) = 0;
      virtual ErrorCode classifyFromFingerprint(const std::string& cache_id, ClassificationResult& result) = 0;
      virtual ErrorCode batchClassify(const std::vector<std::string>& files, std::vector<ClassificationResult>& results) = 0;
      virtual std::vector<std::string> getSupportedGenres() const = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.1.3** Define IAudioVisualizer interface (complete definition)
  ```cpp
  class IAudioVisualizer {
      virtual ErrorCode getSpectrumData(const std::string& cache_id, SpectrumData& result, size_t resolution = 1024) = 0;
      virtual ErrorCode getWaveformData(const std::string& cache_id, WaveformData& result, size_t resolution = 1024) = 0;
      virtual ErrorCode getEnvelopeData(const std::string& cache_id, EnvelopeData& result) = 0;
      virtual ErrorCode generateVisualization(const std::string& cache_id, VisualizationType type, ImageData& result) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.1.4** Define IAdvancedDSP interface (complete definition)
  ```cpp
  class IAdvancedDSP {
      virtual ErrorCode applyReverb(const AudioBuffer& input, AudioBuffer& output, const ReverbParams& params) = 0;
      virtual ErrorCode applyChorus(const AudioBuffer& input, AudioBuffer& output, const ChorusParams& params) = 0;
      virtual ErrorCode applyTubeAmp(const AudioBuffer& input, AudioBuffer& output, const TubeParams& params) = 0;
      virtual ErrorCode applyPhaser(const AudioBuffer& input, AudioBuffer& output, const PhaserParams& params) = 0;
      virtual ErrorCode applyFlanger(const AudioBuffer& input, AudioBuffer& output, const FlangerParams& params) = 0;
      virtual std::vector<TubeModel> getSupportedTubeModels() const = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.1.5** Define IMetadataProvider interface (complete definition)
  ```cpp
  class IMetadataProvider {
      virtual ErrorCode queryMusicBrainz(const std::string& fingerprint, MusicBrainzMetadata& result) = 0;
      virtual ErrorCode queryAcoustid(const std::string& fingerprint, AcoustidMetadata& result) = 0;
      virtual ErrorCode enrichMetadata(const std::string& audio_file, Metadata& metadata) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```

#### 9.2 Phase 4 Extension Interfaces (预留接口)
- [ ] **9.2.1** Define IAudioStreamer interface (complete definition)
  ```cpp
  class IAudioStreamer {
      virtual ErrorCode createStreamServer(int port, StreamHandle& handle) = 0;
      virtual ErrorCode startStream(const StreamHandle& handle) = 0;
      virtual ErrorCode stopStream(const StreamHandle& handle) = 0;
      virtual ErrorCode broadcastMulticast(const StreamHandle& handle, const std::string& multicast_address) = 0;
      virtual ErrorCode getStreamStatus(const StreamHandle& handle, StreamStatus& status) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.2.2** Define IDistributedCache interface (complete definition)
  ```cpp
  class IDistributedCache {
      virtual ErrorCode syncCache(const std::string& cache_id, const std::vector<std::string>& nodes) = 0;
      virtual ErrorCode replicateCache(const std::string& cache_id, const std::string& target_node) = 0;
      virtual ErrorCode getCacheNodes(std::vector<CacheNode>& nodes) = 0;
      virtual ErrorCode discoverNodes(std::vector<CacheNode>& nodes) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```
- [ ] **9.2.3** Define INetworkAudio interface (complete definition)
  ```cpp
  class INetworkAudio {
      virtual ErrorCode startDLNAServer(const DLNAConfig& config) = 0;
      virtual ErrorCode startAirPlayServer(const AirPlayConfig& config) = 0;
      virtual ErrorCode discoverDevices(std::vector<NetworkDevice>& devices) = 0;
      virtual ErrorCode pushToDevice(const NetworkDevice& device, const AudioStream& stream) = 0;
      virtual bool isAvailable() const = 0;  // Returns false in Phase 1
  };
  ```

#### 9.3 Feature Status Markers
- [ ] **9.3.1** Define FeatureStatus enum
  - CORE_V1
  - API_V1
  - EXTENDED_V1
  - DISTRIBUTED_V1
  - ADVANCED_V2
  - EXPERIMENTAL
- [ ] **9.3.2** Add feature status markers to all classes
  - xpuLoad: CORE_V1
  - xpuIn2Wav: CORE_V1
  - xpuPlay: CORE_V1
  - xpuQueue: CORE_V1
  - xpuProcess: CORE_V1
  - xpuDaemon: CORE_V1
- [ ] **9.3.3** Document feature status system
  - Developer guide
  - Migration guide

#### 9.4 Testing
- [ ] **9.4.1** Unit tests for all interfaces
  - Verify all interface methods exist
  - Verify all interfaces return ErrorCode::NotImplemented in Phase 1
  - Verify isAvailable() returns false for all Phase 2-5 interfaces
- [ ] **9.4.2** Interface contract tests
  - Test interface method signatures match specification
  - Test parameter validation
  - Test error code returns
- [ ] **9.4.3** Interface compatibility tests
  - Verify interfaces are compatible with future Phase implementations
  - Test that Phase 1 code compiles with Phase 2-5 interface definitions
  - Test that no breaking changes are introduced when interfaces are implemented
- [ ] **9.4.4** Interface integration tests
  - Test that libxpu properly exports all interfaces
  - Test that modules can query interface availability
  - Test graceful fallback when interfaces are not available

---

## Week 4: Integration, Testing & Documentation (第4周：集成、测试与文档)

### Day 22-23: Integration Testing

#### 10.1 Pipeline Integration Tests
- [ ] **10.1.1** Test complete pipeline: xpuLoad → xpuIn2Wav → xpuPlay
  - FLAC file playback
  - WAV file playback
  - ALAC file playback
  - DSD file playback
- [ ] **10.1.2** Test queue integration
  - Add multiple songs
  - Play through queue
  - Next/Previous navigation
  - Random playback
  - Loop modes
- [ ] **10.1.3** Test DSP integration
  - Volume control
  - Fade effects
  - EQ presets
- [ ] **10.1.4** Test daemon orchestration
  - Start/stop playback
  - Module spawning
  - Process monitoring
  - Error recovery

#### 10.2 Performance Testing
- [ ] **10.2.1** Measure playback latency
  - Verify <50ms target
  - Different buffer sizes
  - Different sample rates
- [ ] **10.2.2** Measure FFT cache performance
  - First run: ~30s for 5-minute song
  - Cached run: <3s (10x minimum, 10-100x target)
  - Calculate speedup ratio
- [ ] **10.2.3** Measure memory usage
  - Monitor during playback
  - Check for memory leaks
  - Optimize if needed
- [ ] **10.2.4** Measure CPU usage
  - Different sample rates
  - Different buffer sizes
  - Optimize if needed
- [ ] **10.2.5** Create performance benchmarks
  - Baseline measurements
  - Regression tests

#### 10.3 Error Handling Tests
- [ ] **10.3.1** Test file not found errors
  - Verify error code 60
  - Verify JSON error format
- [ ] **10.3.2** Test unsupported format errors
  - Verify error code 62
  - Verify helpful error messages
- [ ] **10.3.3** Test device unavailable errors
  - Verify error code 70
  - Verify recovery mechanisms
- [ ] **10.3.4** Test cache miss scenarios
  - Verify error code 80
  - Verify fallback behavior
- [ ] **10.3.5** Test resource errors
  - Verify error code 100-102
  - Verify graceful degradation

#### 10.4 Cross-Platform Tests
- [ ] **10.4.1** Windows tests
  - WASAPI backend
  - Path handling
  - Service installation
- [ ] **10.4.2** macOS tests
  - CoreAudio backend
  - Path handling
  - Bundle creation
- [ ] **10.4.3** Linux tests
  - ALSA backend
  - Path handling
  - Service installation

---

### Day 24-25: Documentation

#### 11.1 User Documentation
- [ ] **11.1.1** Create QUICKSTART.md
  - Installation instructions
  - Basic usage examples
  - Common commands
  - Troubleshooting
- [ ] **11.1.2** Create USER_GUIDE.md
  - Detailed module documentation
  - Command reference
  - Configuration guide
  - Examples and tutorials
- [ ] **11.1.3** Create FAQ.md
  - Common questions
  - Known issues
  - Workarounds
- [ ] **11.1.4** Create CHANGELOG.md
  - Version history
  - New features
  - Breaking changes
  - Migration guide

#### 11.2 Developer Documentation
- [ ] **11.2.1** Create ARCHITECTURE.md
  - System architecture
  - Module interactions
  - Data flow diagrams
  - Extension points
- [ ] **11.2.2** Create API.md
  - REST API specification
  - MCP protocol specification
  - CLI protocol specification
  - Error code reference
- [ ] **11.2.3** Create CONTRIBUTING.md
  - Development setup
  - Code style guide
  - Pull request process
  - Testing guidelines
- [ ] **11.2.4** Create MODULE_DEV_GUIDE.md
  - How to add a new module
  - Interface guidelines
  - Best practices
  - Examples

#### 11.3 Build Documentation
- [ ] **11.3.1** Create BUILD.md
  - Build instructions
  - Dependencies
  - Platform-specific notes
- [ ] **11.3.2** Create INSTALL.md
  - Installation guide
  - Platform-specific instructions
  - Verification steps

---

### Day 26-27: Comprehensive Testing

#### 12.1 Unit Tests
- [ ] **12.1.1** xpuLoad unit tests
  - File loading tests
  - Metadata extraction tests
  - Format support tests
  - Error handling tests
- [ ] **12.1.2** xpuIn2Wav unit tests
  - Conversion tests
  - FFT computation tests
  - Cache operation tests
  - Performance tests
- [ ] **12.1.3** xpuPlay unit tests
  - Backend tests
  - Device management tests
  - Latency tests
  - State output tests
- [ ] **12.1.4** xpuQueue unit tests
  - Queue operation tests
  - Persistence tests
  - Playback mode tests
- [ ] **12.1.5** xpuProcess unit tests
  - DSP effect tests
  - Pipeline tests
  - Quality tests
- [ ] **12.1.6** xpuDaemon unit tests
  - Lifecycle tests
  - Orchestration tests
  - Configuration tests
  - Logging tests
- [ ] **12.1.7** Shared library tests
  - Protocol tests
  - Utility tests
  - Interface tests

#### 12.2 Integration Tests
- [ ] **12.2.1** Pipeline integration tests
  - Full pipeline tests
  - Error propagation tests
- [ ] **12.2.2** Queue integration tests
  - Queue + Play tests
  - Queue + Process tests
- [ ] **12.2.3** Daemon integration tests
  - Daemon + all modules tests
  - Orchestration tests
- [ ] **12.2.4** Cross-module tests
  - Module communication tests
  - Data flow tests

#### 12.3 Contract Tests
- [ ] **12.3.1** CLI protocol contract tests
  - Input format tests
  - Output format tests
  - Error format tests
- [ ] **12.3.2** Error code contract tests
  - All error codes
  - Error message format
  - Error propagation
- [ ] **12.3.3** Interface contract tests
  - All interfaces
  - Method signatures
  - Behavior contracts

#### 12.4 Performance Tests
- [ ] **12.4.1** Latency benchmarks
  - Measure playback latency
  - Verify <50ms target
- [ ] **12.4.2** FFT cache benchmarks
  - Measure first run time
  - Measure cached run time
  - Verify 10-100x speedup
- [ ] **12.4.3** Memory benchmarks
  - Measure memory usage
  - Check for leaks
  - Optimize if needed
- [ ] **12.4.4** CPU benchmarks
  - Measure CPU usage
  - Different scenarios
- [ ] **12.4.5** Throughput benchmarks
  - Large file handling
  - Queue operations
  - Batch operations

#### 12.5 End-to-End Tests
- [ ] **12.5.1** Complete workflow test
  - Load music library
  - Create queue
  - Play through queue
  - Apply effects
  - Stop and restart
- [ ] **12.5.2** Error recovery tests
  - Simulate crashes
  - Verify recovery
  - Verify state persistence
- [ ] **12.5.3** Stress tests
  - Large queues (1000+ songs)
  - Long playback sessions
  - Rapid operations

---

### Day 28: Final Validation & Release

#### 13.1 Success Criteria Validation
- [ ] **13.1.1** Validate all success criteria from PLAN.md
  - Core pipeline works: ✅
  - Professional audio quality: ✅
  - DSD support: ✅
  - Queue functionality: ✅
  - FFT cache performance: ✅
  - Latency <50ms: ✅
  - Error handling: ✅
- [ ] **13.1.2** Run all test suites
  - Unit tests: 100% pass
  - Integration tests: 100% pass
  - Contract tests: 100% pass
  - Performance tests: All targets met
- [ ] **13.1.3** Code quality checks
  - clang-tidy: No warnings
  - cppcheck: No errors
  - clang-format: All files formatted
- [ ] **13.1.4** Documentation completeness
  - All docs written
  - All examples tested
  - All API documented

#### 13.2 Release Preparation
- [ ] **13.2.1** Create release tag
- [ ] **13.2.2** Create release notes
  - New features
  - Known issues
  - Upcoming features
- [ ] **13.2.3** Create release binaries
  - Windows build
  - macOS build
  - Linux builds (Ubuntu, Fedora, Arch)
- [ ] **13.2.4** Test release installation
  - Verify install scripts
  - Verify all platforms
- [ ] **13.2.5] Create checksums for releases

#### 13.3 Final Review
- [ ] **13.3.1** Review all code changes
  - Code review
  - Architecture review
  - Security review
- [ ] **13.3.2** Update documentation
  - Ensure all docs are up-to-date
  - Verify all examples work
- [ ] **13.3.3** Plan Phase 2 preparation
  - Identify Phase 2 dependencies
  - Ensure interfaces are ready
  - Document Phase 2 requirements

---

## 📊 Task Statistics

### By Category
- **Project Setup**: 26 tasks (15.4%)
- **xpuLoad Module**: 19 tasks (11.2%)
- **xpuIn2Wav Module**: 25 tasks (14.8%)
- **xpuPlay Module**: 29 tasks (17.2%)
- **xpuQueue Module**: 15 tasks (8.9%)
- **xpuProcess Module**: 16 tasks (9.5%)
- **xpuDaemon Module**: 29 tasks (17.2%)
- **Shared Library**: 13 tasks (7.7%)
- **Extension Interfaces**: 20 tasks (11.8%)
- **Integration Testing**: 18 tasks (10.7%)
- **Documentation**: 15 tasks (8.9%)
- **Final Testing**: 20 tasks (11.8%)
- **Release**: 11 tasks (6.5%)

### By Module
- **xpuLoad**: 19 tasks
- **xpuIn2Wav**: 25 tasks
- **xpuPlay**: 29 tasks (enhanced with detailed device management)
- **xpuQueue**: 15 tasks
- **xpuProcess**: 16 tasks
- **xpuDaemon**: 29 tasks (enhanced with comprehensive state management)
- **libxpu (Shared)**: 13 tasks (enhanced with interface stubs)
- **Extension Interfaces**: 20 tasks (complete C++ interface definitions)
- **Testing**: 38 tasks
- **Documentation**: 15 tasks
- **Infrastructure**: 26 tasks

### By Week
- **Week 1**: 70 tasks (infrastructure + xpuLoad + xpuIn2Wav)
- **Week 2**: 60 tasks (xpuPlay + xpuQueue + xpuProcess)
- **Week 3**: 62 tasks (xpuDaemon + shared library + interfaces)
- **Week 4**: 46 tasks (testing + documentation + release)

### Key Improvements from Gap Analysis
- ✅ **Complete extension interface definitions** (was 60%, now 100%)
- ✅ **Enhanced state persistence** (playback + configuration state)
- ✅ **Real-time status push mechanism** (detailed implementation)
- ✅ **Device management completeness** (capability detection, hot-plug, fallback)
- ✅ **Configuration hot-reload** (complete implementation with rollback)
- ✅ **Cross-platform path handling** (explicit paths for all platforms)
- ✅ **Interface compatibility tests** (ensure Phase 2-5 compatibility)

---

## ✅ Completion Criteria

Phase 1 is complete when:
- [x] All 156 tasks are done
- [x] All unit tests pass (100%)
- [x] All integration tests pass (100%)
- [x] All contract tests pass (100%)
- [x] Performance targets met:
  - [x] Playback latency < 50ms
  - [x] FFT cache speedup 10-100x
  - [x] Memory usage acceptable
- [x] Code quality checks pass:
  - [x] clang-tidy: 0 warnings
  - [x] cppcheck: 0 errors
  - [x] clang-format: 100% compliant
- [x] Documentation complete:
  - [x] README.md
  - [x] QUICKSTART.md
  - [x] API.md
  - [x] ARCHITECTURE.md
- [x] Cross-platform support verified:
  - [x] Windows (WASAPI)
  - [x] macOS (CoreAudio)
  - [x] Linux (ALSA)
- [x] All success criteria validated

---

## 🎯 Next Steps

After Phase 1 completion:
1. **Phase 2 begins**: AI-Native Integration (4 weeks)
   - REST API implementation
   - MCP Server implementation
   - Agent Protocol implementation
   - 30+ MCP tools
2. **Continue with**: Extended Modules (Phase 3)
   - xpuFingerprint
   - xpuClassify
   - xpuVisualize
   - And more...

---

**Total Phase 1 Tasks**: 169 (increased from 156 based on gap analysis)
**Estimated Duration**: 4 weeks (28 days)
**Success Rate Target**: 100%
**PLAN.md Phase 1 Coverage**: 95%+ (improved from 85%)
