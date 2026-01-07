---

description: "Task list for XPU AI-Ready Music Playback System MVP implementation"
---

# Tasks: XPU AI-Ready Music Playback System MVP

**Input**: Design documents from `/specs/001-xpu-mvp-system/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Per XPU Constitution (TDD NON-NEGOTIABLE), tests MUST be included for all user stories. Tests MUST be written first and MUST fail before implementation begins.

**Cross-Platform**: All tasks MUST support Windows 10+, macOS 10.14+, and Linux (Ubuntu 20.04+). Use CMake for cross-platform detection and avoid platform-specific code in core business logic.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

**Implementation Order** (per user request):
1. Pipeline modules (xpuLoad, xpuIn2Wav, xpuPlay, xpuQueue, xpuProcess)
2. API and MCP layer (REST API, MCP protocol)
3. xpuDaemon orchestration and Claude Skills integration

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4, US5)
- Include exact file paths in descriptions

## Path Conventions

- **Source root**: `xpu/src/`
- **Tests root**: `xpu/tests/`
- **Shared library**: `xpu/src/lib/`

## Platform-Specific Paths

All paths MUST use platform detection via CMake preprocessor macros:

| Platform | Config | Cache | Logs |
|----------|--------|-------|------|
| **Linux** | `~/.config/xpu/` | `~/.cache/xpu/fft/` | `~/.local/state/xpu/` |
| **macOS** | `~/Library/Application Support/xpu/` | `~/Library/Caches/xpu/fft/` | `~/Library/Logs/xpu/` |
| **Windows** | `%APPDATA%\xpu\` | `%LOCALAPPDATA%\xpu\cache\fft\` | `%LOCALAPPDATA%\xpu\logs\` |

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure with cross-platform support

### Project Structure

- [ ] T001 Create project structure: xpu/src/xpuLoad/, xpu/src/xpuIn2Wav/, xpu/src/xpuPlay/, xpu/src/xpuQueue/, xpu/src/xpuProcess/, xpu/src/xpuDaemon/, xpu/src/lib/, xpu/tests/, xpu/configs/, xpu/scripts/

### CMake Build System (Cross-Platform)

- [ ] T002 Create root CMakeLists.txt with:
  - C++17 standard requirement
  - Cross-platform detection (WIN32, APPLE, UNIX)
  - Find dependencies: FFmpeg, PortAudio, FFTW3, libsamplerate, nlohmann/json, spdlog, GoogleTest
  - Platform-specific linking (winmm for Windows, CoreAudio for macOS, asound for Linux)
- [ ] T003 [P] Create xpu/src/lib/CMakeLists.txt for shared library (libxpu) with all dependencies
- [ ] T004 [P] Create xpu/src/xpuLoad/CMakeLists.txt linking to libxpu
- [ ] T005 [P] Create xpu/src/xpuIn2Wav/CMakeLists.txt linking to libxpu, FFmpeg, FFTW3, libsamplerate
- [ ] T006 [P] Create xpu/src/xpuPlay/CMakeLists.txt linking to libxpu, PortAudio
- [ ] T007 [P] Create xpu/src/xpuQueue/CMakeLists.txt linking to libxpu, nlohmann/json
- [ ] T008 [P] Create xpu/src/xpuProcess/CMakeLists.txt linking to libxpu, FFTW3
- [ ] T009 [P] Create xpu/src/xpuDaemon/CMakeLists.txt linking to libxpu, cpp-httplib
- [ ] T010 [P] Create xpu/tests/CMakeLists.txt with GoogleTest and pytest support

### Code Quality Tools

- [ ] T011 [P] Configure .clang-tidy for static analysis (cross-platform checks)
- [ ] T012 [P] Configure clang-format for C++ code (Google C++ Style Guide)
- [ ] T013 [P] Configure cppcheck for static analysis with platform-specific rules

### Configuration

- [ ] T014 Create example xpuSetting.conf TOML configuration file in xpu/configs/ with:
  - [playback] section: target_sample_rate, target_bit_depth, channels, default_device
  - [fft_cache] section: enabled, cache_dir (platform-specific), fft_size, hop_size, window
  - [queue] section: max_tracks, auto_save
  - [rest_api] section: host, port, enabled
  - [mcp] section: enabled, transport
  - [logging] section: level, file (platform-specific), max_size_mb

### Installation Scripts

- [ ] T015 [P] Create xpu/scripts/install.sh for Linux/macOS dependency installation
- [ ] T016 [P] Create xpu/scripts/install.ps1 for Windows dependency installation (vcpkg/Conan)
- [ ] T017 [P] Create xpu/scripts/install.sh for Homebrew (macOS) specific packages
- [ ] T018 [P] Create xpu/scripts/install.sh for apt (Ubuntu/Debian) specific packages

**Checkpoint**: Build system works on all three platforms, dependencies can be installed

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Shared Library Infrastructure (Cross-Platform)

- [ ] T019 Create xpu/src/lib/utils/platform.cpp for platform-specific path resolution (Windows/macOS/Linux)
- [ ] T020 Create xpu/src/lib/protocol/json_serializer.cpp for JSON request/response handling
- [ ] T021 Create xpu/src/lib/protocol/cli_protocol.cpp for stdin/stdout protocol implementation
- [ ] T022 Create xpu/src/lib/utils/logger.cpp using spdlog for JSON stderr logging with platform-specific log paths
- [ ] T023 Create xpu/src/lib/utils/config.cpp for TOML configuration file parsing with platform-specific config paths
- [ ] T024 Create xpu/src/lib/audio/ffmpeg_decoder.cpp wrapper for FFmpeg audio decoding (cross-platform)
- [ ] T025 Create xpu/src/lib/audio/portaudio_output.cpp wrapper for PortAudio playback (auto-selects WASAPI/CoreAudio/ALSA)
- [ ] T026 Create xpu/src/lib/audio/libsamplerate_wrapper.cpp for resampling (cross-platform)
- [ ] T027 Create xpu/src/lib/audio/fftw_wrapper.cpp for FFT computation (cross-platform)
- [ ] T028 [P] Create xpu/src/lib/include/protocol/protocol.h header with JSON message structs
- [ ] T029 [P] Create xpu/src/lib/include/audio/audio_types.h header with AudioTrack, PlaybackState structs
- [ ] T030 [P] Create xpu/src/lib/include/utils/platform.h header with platform detection macros and path resolution
- [ ] T031 [P] Create xpu/src/lib/include/utils/error_codes.h with standard error code definitions

### Test Infrastructure (Cross-Platform)

- [ ] T032 Create xpu/tests/contract/test_cli_protocol.py for CLI protocol contract tests
- [ ] T033 Create xpu/tests/integration/test_pipeline.py for end-to-end pipeline tests
- [ ] T034 Create xpu/tests/benchmark/test_performance.py for performance benchmark tests
- [ ] T035 [P] Create cross-platform test fixtures in xpu/tests/fixtures/platform.py for path resolution testing

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Local Audio Playback (Priority: P1) 🎯 MVP

**Goal**: Enable users to play lossless audio files (FLAC, WAV) using CLI pipeline: xpuLoad | xpuIn2Wav | xpuPlay

**Independent Test**: Run `xpuLoad song.flac | xpuIn2Wav | xpuPlay` and verify audio output through default device

**Cross-Platform**: Audio playback must work on Windows (WASAPI), macOS (CoreAudio), and Linux (ALSA)

### Tests for User Story 1 (MANDATORY per TDD Constitution) ⚠️

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [ ] T036 [P] [US1] Contract test for xpuLoad JSON protocol in xpu/tests/contract/test_xpuload_protocol.py
- [ ] T037 [P] [US1] Contract test for xpuIn2Wav JSON protocol in xpu/tests/contract/test_xpuin2wav_protocol.py
- [ ] T038 [P] [US1] Contract test for xpuPlay JSON protocol in xpu/tests/contract/test_xpuplay_protocol.py
- [ ] T039 [P] [US1] Integration test for FLAC playback pipeline in xpu/tests/integration/test_us1_playback.py
- [ ] T040 [P] [US1] Unit test for AudioTrack data model in xpu/tests/unit/test_audio_track.cpp
- [ ] T041 [P] [US1] Performance test for < 100ms latency requirement in xpu/tests/benchmark/test_latency.py
- [ ] T042 [P] [US1] Cross-platform test for device enumeration in xpu/tests/integration/test_us1_cross_platform.py

### Implementation for User Story 1

#### xpuLoad Module

- [ ] T043 [P] [US1] Create xpu/src/xpuLoad/main.cpp with command-line argument parsing
- [ ] T044 [P] [US1] Create xpu/src/xpuLoad/audio_parser.cpp for FLAC/WAV file validation (cross-platform)
- [ ] T045 [P] [US1] Create xpu/src/xpuLoad/metadata_extractor.cpp for reading Vorbis/ID3 tags
- [ ] T046 [US1] Implement stdin JSON request handling in xpu/src/xpuLoad/main.cpp (depends on T043-T045)
- [ ] T047 [US1] Implement stdout JSON response with AudioTrack in xpu/src/xpuLoad/main.cpp
- [ ] T048 [US1] Add stderr JSON logging for errors in xpu/src/xpuLoad/main.cpp

#### xpuIn2Wav Module

- [ ] T049 [P] [US1] Create xpu/src/xpuIn2Wav/decoder.cpp using FFmpeg for audio decoding (cross-platform)
- [ ] T050 [P] [US1] Create xpu/src/xpuIn2Wav/resampler.cpp using libsamplerate for sample rate conversion
- [ ] T051 [P] [US1] Create xpu/src/xpuIn2Wav/fft_computer.cpp using FFTW3 for FFT computation
- [ ] T052 [P] [US1] Create xpu/src/xpuIn2Wav/cache_manager.cpp for FFT cache file I/O with platform-specific cache paths
- [ ] T053 [US1] Implement WAV format output with 44-byte header in xpu/src/xpuIn2Wav/decoder.cpp
- [ ] T054 [US1] Implement FFT cache metadata JSON generation in xpu/src/xpuIn2Wav/cache_manager.cpp
- [ ] T055 [US1] Implement stdin/stdout JSON protocol in xpu/src/xpuIn2Wav/main.cpp (depends on T049-T052)
- [ ] T056 [US1] Add command-line args: --rate, --depth, --fft-cache, --no-fft-cache in xpu/src/xpuIn2Wav/main.cpp

#### xpuPlay Module

- [ ] T057 [P] [US1] Create xpu/src/xpuPlay/device_manager.cpp for PortAudio device enumeration (auto-selects backend)
- [ ] T058 [P] [US1] Create xpu/src/xpuPlay/audio_output.cpp for stream playback using PortAudio (cross-platform)
- [ ] T059 [US1] Implement WAV header parsing and PCM stream processing in xpu/src/xpuPlay/audio_output.cpp
- [ ] T060 [US1] Implement buffer management for < 500MB memory constraint in xpu/src/xpuPlay/audio_output.cpp
- [ ] T061 [US1] Implement stdin/stdout JSON protocol in xpu/src/xpuPlay/main.cpp (depends on T057-T058)
- [ ] T062 [US1] Add --list-devices and --device command-line options in xpu/src/xpuPlay/main.cpp
- [ ] T063 [US1] Add error handling for device disconnection in xpu/src/xpuPlay/audio_output.cpp (platform-specific)

**Checkpoint**: At this point, User Story 1 should be fully functional - can play FLAC/WAV via CLI pipeline on all platforms

---

## Phase 4: User Story 2 - Queue Management (Priority: P2)

**Goal**: Enable users to manage playlist with add, list, next, previous, pause, resume, clear operations

**Independent Test**: Add songs to queue, use navigation commands, verify correct song plays at each step

**Cross-Platform**: Queue state persistence must use platform-specific paths

### Tests for User Story 2 (MANDATORY per TDD Constitution) ⚠️

- [ ] T064 [P] [US2] Contract test for xpuQueue JSON protocol in xpu/tests/contract/test_xpuqueue_protocol.py
- [ ] T065 [P] [US2] Integration test for queue navigation in xpu/tests/integration/test_us2_queue.py
- [ ] T066 [P] [US2] Unit test for Queue data model in xpu/tests/unit/test_queue.cpp
- [ ] T067 [P] [US2] Unit test for queue state persistence in xpu/tests/unit/test_queue_persistence.cpp
- [ ] T068 [P] [US2] Cross-platform test for queue state file paths in xpu/tests/integration/test_us2_cross_platform.py

### Implementation for User Story 2

- [ ] T069 [P] [US2] Create xpu/src/xpuQueue/queue_manager.cpp for queue operations (add, remove, list, clear)
- [ ] T070 [P] [US2] Create xpu/src/xpuQueue/navigation.cpp for next/previous/index operations
- [ ] T071 [P] [US2] Create xpu/src/xpuQueue/state_persistence.cpp for JSON file save/load with platform-specific paths
- [ ] T072 [P] [US2] Create xpu/src/xpuQueue/playback_control.cpp for pause/resume state
- [ ] T073 [US2] Implement stdin/stdout JSON protocol in xpu/src/xpuQueue/main.cpp (depends on T069-T072)
- [ ] T074 [US2] Add queue state auto-save on modification in xpu/src/xpuQueue/state_persistence.cpp
- [ ] T075 [US2] Implement max 1000 tracks validation in xpu/src/xpuQueue/queue_manager.cpp
- [ ] T076 [US2] Add --file, --position, --index command-line arguments in xpu/src/xpuQueue/main.cpp

**Checkpoint**: At this point, User Stories 1 AND 2 should both work independently

---

## Phase 5: User Story 3 - Audio Processing (Priority: P2)

**Goal**: Enable users to adjust audio quality with EQ presets, volume control, and fade effects

**Independent Test**: Play audio with different EQ presets and volume levels, verify output changes

**Cross-Platform**: DSP processing must be consistent across all platforms

### Tests for User Story 3 (MANDATORY per TDD Constitution) ⚠️

- [ ] T077 [P] [US3] Contract test for xpuProcess JSON protocol in xpu/tests/contract/test_xpuprocess_protocol.py
- [ ] T078 [P] [US3] Integration test for DSP effects in xpu/tests/integration/test_us3_processing.py
- [ ] T079 [P] [US3] Unit test for EQ preset calculations in xpu/tests/unit/test_equalizer.cpp
- [ ] T080 [P] [US3] Unit test for volume control in xpu/tests/unit/test_volume.cpp
- [ ] T081 [P] [US3] Cross-platform test for FFT cache I/O in xpu/tests/integration/test_us3_cross_platform.py

### Implementation for User Story 3

- [ ] T082 [P] [US3] Create xpu/src/xpuProcess/equalizer.cpp with 5 presets (flat, rock, pop, classical, jazz)
- [ ] T083 [P] [US3] Create xpu/src/xpuProcess/volume_control.cpp for 0.0-1.0 volume scaling
- [ ] T084 [P] [US3] Create xpu/src/xpuProcess/fade.cpp for fade-in/fade-out envelope generation
- [ ] T085 [US3] Implement PCM stream processing pipeline in xpu/src/xpuProcess/main.cpp (depends on T082-T084)
- [ ] T086 [US3] Implement stdin/stdout JSON protocol in xpu/src/xpuProcess/main.cpp
- [ ] T087 [US3] Add --eq, --volume, --fade-in, --fade-out command-line arguments in xpu/src/xpuProcess/main.cpp
- [ ] T088 [US3] Use FFT cache from xpuIn2Wav for frequency-domain EQ in xpu/src/xpuProcess/equalizer.cpp

**Checkpoint**: All user stories should now be independently functional

---

## Phase 6: User Story 4 - AI Agent Integration via MCP (Priority: P3)

**Goal**: Enable Claude AI to control playback via MCP protocol (stdio JSON-RPC)

**Independent Test**: Start xpuDaemon with MCP, send tool calls, verify correct responses and CLI control

**Cross-Platform**: MCP server works via stdio on all platforms

### Tests for User Story 4 (MANDATORY per TDD Constitution) ⚠️

- [ ] T089 [P] [US4] Contract test for MCP JSON-RPC protocol in xpu/tests/contract/test_mcp_protocol.py
- [ ] T090 [P] [US4] Integration test for MCP tool calls in xpu/tests/integration/test_us4_mcp.py
- [ ] T091 [P] [US4] Unit test for MCP server in xpu/tests/unit/test_mcp_server.cpp

### Implementation for User Story 4

#### MCP Server

- [ ] T092 [P] [US4] Create xpu/src/xpuDaemon/mcp_server.cpp for JSON-RPC 2.0 over stdio (cross-platform)
- [ ] T093 [P] [US4] Create xpu/src/xpuDaemon/mcp_tool_registry.cpp for tool registration and dispatch
- [ ] T094 [US4] Implement xpu_play tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T095 [US4] Implement xpu_pause tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T096 [US4] Implement xpu_resume tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T097 [US4] Implement xpu_stop tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T098 [US4] Implement xpu_seek tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T099 [US4] Implement xpu_queue_add tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T100 [US4] Implement xpu_queue_list tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T101 [US4] Implement xpu_queue_remove tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T102 [US4] Implement xpu_queue_clear tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T103 [US4] Implement xpu_queue_next tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T104 [US4] Implement xpu_queue_previous tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T105 [US4] Implement xpu_meta_get tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T106 [US4] Implement xpu_meta_search tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T107 [US4] Implement xpu_process_volume tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T108 [US4] Implement xpu_process_eq tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp
- [ ] T109 [US4] Implement xpu_process_mute tool in xpu/src/xpuDaemon/mcp_tool_registry.cpp

#### xpuDaemon Core

- [ ] T110 [US4] Create xpu/src/xpuDaemon/orchestrator.cpp for spawning CLI module processes (cross-platform)
- [ ] T111 [US4] Create xpu/src/xpuDaemon/process_manager.cpp for monitoring module lifecycles
- [ ] T112 [US4] Implement daemon main with --mcp --stdio flags in xpu/src/xpuDaemon/main.cpp
- [ ] T113 [US4] Connect MCP tools to CLI module invocations in xpu/src/xpuDaemon/orchestrator.cpp

**Checkpoint**: AI agents can control playback via MCP protocol on all platforms

---

## Phase 7: User Story 5 - REST API Control (Priority: P3)

**Goal**: Enable third-party integrations via REST API (HTTP endpoints)

**Independent Test**: Make HTTP requests to endpoints, verify correct responses and audio behavior

**Cross-Platform**: HTTP server must work on Windows (Winsock2), macOS (BSD sockets), Linux (POSIX)

### Tests for User Story 5 (MANDATORY per TDD Constitution) ⚠️

- [ ] T114 [P] [US5] Contract test for REST API endpoints in xpu/tests/contract/test_rest_api.py
- [ ] T115 [P] [US5] Integration test for REST playback control in xpu/tests/integration/test_us5_rest.py
- [ ] T116 [P] [US5] Cross-platform test for HTTP server socket handling in xpu/tests/integration/test_us5_cross_platform.py

### Implementation for User Story 5

#### REST API Server

- [ ] T117 [P] [US5] Create xpu/src/xpuDaemon/rest_server.cpp using cpp-httplib (cross-platform sockets)
- [ ] T118 [P] [US5] Implement POST /api/play endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T119 [P] [US5] Implement POST /api/pause endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T120 [P] [US5] Implement POST /api/resume endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T121 [P] [US5] Implement POST /api/stop endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T122 [P] [US5] Implement POST /api/seek endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T123 [P] [US5] Implement GET /api/queue endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T124 [P] [US5] Implement POST /api/queue endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T125 [P] [US5] Implement DELETE /api/queue endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T126 [P] [US5] Implement DELETE /api/queue/{index} endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T127 [P] [US5] Implement POST /api/queue/next endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T128 [P] [US5] Implement POST /api/queue/previous endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T129 [P] [US5] Implement GET /api/volume endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T130 [P] [US5] Implement PUT /api/volume endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T131 [P] [US5] Implement GET /api/eq endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T132 [P] [US5] Implement PUT /api/eq endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T133 [P] [US5] Implement GET /api/status endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T134 [P] [US5] Implement POST /api/track/metadata endpoint in xpu/src/xpuDaemon/rest_server.cpp
- [ ] T135 [US5] Connect REST endpoints to orchestrator and CLI modules in xpu/src/xpuDaemon/rest_server.cpp

**Checkpoint**: All user stories complete - full CLI, MCP, and REST functionality on all platforms

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories and ensure cross-platform compatibility

- [ ] T136 [P] Add man pages for all CLI tools in xpu/docs/man/
- [ ] T137 [P] Create README.md for each module in xpu/src/xpu*/README.md
- [ ] T138 Run static analysis: clang-tidy and cppcheck on all C++ code (all platforms)
- [ ] T139 Run address sanitizer in debug builds to check for memory errors (all platforms)
- [ ] T140 Run all tests and ensure 80%+ code coverage on all three platforms
- [ ] T141 Run performance benchmarks: verify < 100ms latency, < 30s FFT for 5-min song (all platforms)
- [ ] T142 Validate quickstart.md instructions work end-to-end on Windows, macOS, and Linux
- [ ] T143 Create example configuration files in xpu/configs/examples/
- [ ] T144 Add input validation tests for edge cases (corrupted files, unsupported formats)
- [ ] T145 Add error message tests for all error codes in contracts/cli-protocol.md
- [ ] T146 Validate installation scripts work on all platforms (install.sh, install.ps1)
- [ ] T147 Create cross-platform CI/CD configuration (GitHub Actions matrix: ubuntu-latest, macos-latest, windows-latest)
- [ ] T148 Create Claude Skills integration example in xpu/examples/claude-skills/
- [ ] T149 Create platform-specific troubleshooting guides in xpu/docs/troubleshooting/

**Checkpoint**: Production-ready MVP with full cross-platform support

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-7)**: All depend on Foundational phase completion
  - US1 (Local Playback), US2 (Queue), US3 (Processing) can proceed in parallel after Foundational
  - US4 (MCP), US5 (REST) depend on US1-US3 being complete (need CLI modules to control)
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1) - Local Playback**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2) - Queue**: Can start after Foundational (Phase 2) - Integrates with US1 but independently testable
- **User Story 3 (P2) - Audio Processing**: Can start after Foundational (Phase 2) - Integrates with US1 pipeline but independently testable
- **User Story 4 (P3) - MCP Integration**: Depends on US1, US2, US3 (needs CLI modules to control)
- **User Story 5 (P3) - REST API**: Depends on US1, US2, US3 (needs CLI modules to control)

### Within Each User Story

- Tests MUST be written and FAIL before implementation (TDD Constitution)
- Tests can run in parallel if marked [P]
- Models/components can run in parallel if marked [P]
- Integration tasks depend on parallel components completing
- Story complete before moving to next priority

### Parallel Opportunities

**Setup Phase (Phase 1)**:
- All CMakeLists.txt files (T003-T010) can be created in parallel
- All static analysis configs (T011-T013) can be created in parallel
- All installation scripts (T015-T018) can be created in parallel

**Foundational Phase (Phase 2)**:
- All header files (T028-T031) can be created in parallel
- All library implementations (T019-T027) can be done in parallel (mostly independent)

**User Story 1 (Phase 3)**:
- All tests (T036-T042) can be written in parallel
- All xpuLoad components (T043-T045) can be done in parallel
- All xpuIn2Wav components (T049-T052) can be done in parallel
- All xpuPlay components (T057-T058) can be done in parallel

**User Stories 2 & 3**:
- Can proceed in parallel with US1 after Foundational phase
- Within each story, tests and components have parallel opportunities

**User Stories 4 & 5**:
- Can proceed in parallel with each other once US1-US3 are complete
- MCP tools (T094-T109) can be implemented in parallel
- REST endpoints (T118-T134) can be implemented in parallel

---

## Cross-Platform Development Guidelines

### Code Organization

**Core Business Logic**: MUST be platform-independent
- Use standard C++17 features (std::filesystem, std::thread, etc.)
- Avoid platform-specific preprocessor macros in core logic
- Delegate platform differences to wrapper classes in lib/

**Platform-Specific Code**: Isolate to dedicated modules
- `xpu/src/lib/utils/platform.cpp` - Path resolution, environment variables
- `xpu/src/lib/audio/portaudio_output.cpp` - Audio backend abstraction
- CMakeLists.txt - Library discovery and linking

### Path Handling

```cpp
// ✅ CORRECT: Use platform wrapper
std::string cache_dir = Platform::GetCacheDirectory();
std::string config_path = Platform::JoinPath(cache_dir, "xpu", "config.json");

// ❌ WRONG: Hard-coded platform paths
std::string cache_dir = "/home/user/.cache/xpu";  // Linux only
```

### Audio Backend Selection

```cpp
// PortAudio automatically selects the appropriate backend:
// - Windows: WASAPI (or DirectSound, MME as fallback)
// - macOS: CoreAudio
// - Linux: ALSA (or OSS, JACK, PulseAudio as fallback)
// No platform-specific code required in business logic!
```

### Build Verification

Before committing code:
1. **Linux**: `mkdir build && cd build && cmake .. && make && ctest`
2. **macOS**: `mkdir build && cd build && cmake .. && make && ctest`
3. **Windows**: `mkdir build && cd build && cmake .. -G "Visual Studio 16 2019" && cmake --build . && ctest -C Release`

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T018)
2. Complete Phase 2: Foundational (T019-T035) - CRITICAL, blocks all stories
3. Complete Phase 3: User Story 1 (T036-T063)
4. **STOP and VALIDATE**: Test `xpuLoad song.flac | xpuIn2Wav | xpuPlay` works independently on all platforms
5. Deploy/demo MVP

**MVP Deliverable**: Can play local FLAC files via CLI pipeline on Windows, macOS, and Linux

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test independently on all platforms → Deploy/Demo (MVP!)
3. Add User Story 2 → Test independently → Deploy/Demo
4. Add User Story 3 → Test independently → Deploy/Demo
5. Add User Story 4 → Test independently → Deploy/Demo
6. Add User Story 5 → Test independently → Deploy/Demo
7. Complete Polish → Final release with cross-platform support

Each story adds value without breaking previous stories.

### Parallel Team Strategy

With multiple developers:

**Phase 1-2**: All developers work together on Setup and Foundational

**Phase 3** (after Foundational):
- Developer A: User Story 1 (xpuLoad, xpuIn2Wav, xpuPlay)
- Developer B: User Story 2 (xpuQueue)
- Developer C: User Story 3 (xpuProcess)

**Phase 4** (after US1-US3 complete):
- Developer A: User Story 4 (MCP server + tools)
- Developer B: User Story 5 (REST API server + endpoints)

Stories complete and integrate independently.

---

## Task Count Summary

| Phase | Tasks | Description |
|-------|-------|-------------|
| Phase 1: Setup | 18 tasks | Project structure, CMake, tooling, installation scripts |
| Phase 2: Foundational | 17 tasks | Shared library, test infrastructure, platform utilities |
| Phase 3: US1 - Playback | 28 tasks | xpuLoad, xpuIn2Wav, xpuPlay modules with cross-platform tests |
| Phase 4: US2 - Queue | 13 tasks | xpuQueue module with cross-platform state persistence |
| Phase 5: US3 - Processing | 12 tasks | xpuProcess module with cross-platform FFT cache |
| Phase 6: US4 - MCP | 25 tasks | MCP server, 18 tools, daemon core |
| Phase 7: US5 - REST | 22 tasks | REST API, 18 endpoints with cross-platform socket tests |
| Phase 8: Polish | 14 tasks | Documentation, validation, CI/CD, cross-platform testing |
| **Total** | **149 tasks** | Complete MVP implementation with full cross-platform support |

---

## Notes

- [P] tasks = different files, no dependencies, can run in parallel
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Per constitution: Tests MUST be written first and FAIL before implementation (TDD)
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- 80%+ test coverage required per constitution
- Key audio paths (load -> convert -> play) must have 100% test coverage
- **Cross-platform**: All code must work on Windows 10+, macOS 10.14+, and Linux (Ubuntu 20.04+) without modifications
- **CI/CD**: GitHub Actions matrix builds must pass for all three platforms before merging
