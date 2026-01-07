# Feature Specification: XPU AI-Ready Music Playback System MVP

**Feature Branch**: `001-xpu-mvp-system`
**Created**: 2026-01-07
**Status**: Draft
**Input**: User description: "Implement XPU AI-Ready Music Playback System MVP per DESIGN.md specifications"

---

## Overview

XPU is a modular music playback system designed for the AI era. Each functional module is an independent CLI tool that can be used directly by humans or exposed via MCP (Model Context Protocol) for AI Agents like Claude Skills.

**Design Philosophy**:
- **CLI-First**: Every module is an independent executable following Unix philosophy
- **AI-Ready**: Native MCP protocol support for seamless Claude/GPT-4 integration
- **Semantic Externalization**: AI understands intent, XPU focuses on execution
- **Fully Modular**: One program does one thing, composable via pipelines
- **FFT-Caching Optimized**: Pre-compute frequency spectrum data once, reuse across all modules

**Architecture Principle**: Input format diversity → Unified WAV + FFT cache → Standardized processing pipeline

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Local Audio Playback (Priority: P1)

A user wants to play lossless audio files (FLAC, WAV) on their local machine using command-line tools.

**Why this priority**: This is the core functionality of XPU - the ability to play music files is the minimum viable product that delivers immediate value to users.

**Independent Test**: Can be fully tested by running `xpuLoad song.flac | xpuIn2Wav | xpuPlay` and verifying audio output through the system's default audio device.

**Acceptance Scenarios**:

1. **Given** a FLAC file in the Music directory, **When** the user runs `xpuLoad song.flac | xpuIn2Wav | xpuPlay`, **Then** the audio plays through the system's default output device with no errors
2. **Given** multiple FLAC files, **When** the user runs `xpuQueue add ~/Music/*.flac && xpuQueue play`, **Then** all files are added to the queue and play sequentially
3. **Given** a corrupted audio file, **When** the user attempts to load it, **Then** a clear error message is displayed indicating the file is invalid
4. **Given** a WAV file at 96kHz, **When** the user plays it, **Then** the audio plays at the correct sample rate without downsampling artifacts
5. **Given** audio playback with FFT cache, **When** replaying the same file, **Then** FFT data loads from cache (10-100x faster than recomputing)

---

### User Story 2 - Queue Management (Priority: P2)

A user wants to manage a playlist of songs with controls for next, previous, pause, and resume.

**Why this priority**: Queue management is essential for practical music listening - users rarely play just one song. This builds on Story 1 by adding playback control capabilities.

**Independent Test**: Can be tested by adding multiple songs to a queue, then using queue commands to navigate and control playback, verifying the correct song plays at each step.

**Acceptance Scenarios**:

1. **Given** a queue with 5 songs, **When** the user runs `xpuQueue next`, **Then** playback advances to the next song in the queue
2. **Given** a queue with 5 songs currently on song 3, **When** the user runs `xpuQueue previous`, **Then** playback returns to song 2
3. **Given** a playing song, **When** the user runs `xpuQueue pause`, **Then** audio playback stops at the current position
4. **Given** a paused song, **When** the user runs `xpuQueue resume`, **Then** playback resumes from the paused position
5. **Given** a queue with songs, **When** the user runs `xpuQueue list`, **Then** the current queue contents are displayed with the currently playing song indicated
6. **Given** a queue with 1000 songs, **When** queue operations are performed, **Then** all operations complete within 50ms
7. **Given** queue state modification, **When** the daemon restarts, **Then** queue state is restored from persistent storage

---

### User Story 3 - Audio Processing (Priority: P2)

A user wants to adjust audio quality during playback using equalizer presets, volume control, and fade effects.

**Why this priority**: Audio processing enhances the listening experience and is a core part of the modular pipeline. This enables users to customize sound output.

**Independent Test**: Can be tested by playing audio through the processing pipeline with different EQ presets and volume levels, verifying the audio output changes accordingly.

**Acceptance Scenarios**:

1. **Given** a playing song, **When** the user runs `xpuLoad song.flac | xpuIn2Wav | xpuProcess --eq rock | xpuPlay`, **Then** the audio plays with rock equalizer settings applied
2. **Given** a playing song, **When** the user applies volume 0.5, **Then** the audio plays at 50% of the original volume
3. **Given** a playing song, **When** the user applies fade-in over 2 seconds, **Then** the audio gradually increases from silence to full volume over the specified duration
4. **Given** a playing song with EQ applied, **When** the user changes the EQ preset, **Then** the new EQ settings are applied immediately to the audio output
5. **Given** FFT cache available, **When** EQ is applied, **Then** frequency-domain processing uses cached FFT data for faster computation

---

### User Story 4 - AI Agent Integration via MCP (Priority: P3)

A user wants to control music playback using natural language through Claude AI or other AI agents.

**Why this priority**: This is a key differentiator for XPU - AI-Ready architecture enables voice/text control of music. However, it depends on Stories 1-3 being functional first.

**Independent Test**: Can be tested by starting the xpuDaemon with MCP support, then sending MCP tool calls to verify the daemon responds correctly and controls the CLI modules.

**Acceptance Scenarios**:

1. **Given** the xpuDaemon is running with MCP enabled, **When** Claude sends an `xpu_play` command via MCP, **Then** the music starts playing and a success response is returned
2. **Given** music is playing, **When** Claude sends an `xpu_queue_add` command with a file path via MCP, **Then** the file is added to the queue and acknowledged
3. **Given** the daemon is running, **When** Claude requests current playback status via `xpu_queue_list`, **Then** the current queue and playback state are returned in structured format
4. **Given** music is playing, **When** Claude sends `xpu_pause` via MCP, **Then** playback pauses and confirmation is returned
5. **Given** multiple MCP clients connected, **When** commands are sent, **Then** daemon handles all clients with < 200ms response time

---

### User Story 5 - REST API Control (Priority: P3)

A user wants to control playback through a REST API for integration with web applications or home automation systems.

**Why this priority**: REST API enables third-party integrations and web interfaces. It's secondary to CLI and MCP access but important for extensibility.

**Independent Test**: Can be tested by making HTTP requests to the REST API endpoints and verifying the correct responses and audio system behavior.

**Acceptance Scenarios**:

1. **Given** the xpuDaemon is running, **When** a client sends POST `/api/play` with a file path, **Then** the specified file begins playing
2. **Given** music is playing, **When** a client sends POST `/api/pause`, **Then** playback pauses
3. **Given** the daemon is running, **When** a client sends GET `/api/queue`, **Then** the current queue contents are returned as JSON
4. **Given** the daemon is running, **When** a client sends PUT `/api/volume` with value 0.8, **Then** the playback volume is set to 80%
5. **Given** 10 concurrent REST API connections, **When** requests are made, **Then** all connections are handled without performance degradation

---

### Edge Cases

- What happens when the user attempts to play a file format that is not supported (e.g., MP3, AAC)?
- How does the system handle a corrupted audio file mid-playback?
- What happens when the audio device is disconnected during playback?
- How does the queue behave when the last song finishes playing?
- What happens when the daemon receives an invalid command via MCP or REST API?
- How does the system handle running out of disk space for FFT cache?
- What happens when multiple clients try to control playback simultaneously?
- How does the system handle extremely large audio files (> 2GB)?
- What happens when the user specifies a sample rate that exceeds the hardware capability?
- What happens when FFT cache is corrupted or incomplete?
- How does the system handle invalid FFT cache IDs?
- What happens when configuration file contains invalid values?

---

## Requirements *(mandatory)*

### Functional Requirements

#### Core CLI Modules (MVP Phase 1)

- **FR-001**: System MUST provide `xpuLoad` module to parse FLAC and WAV audio files
- **FR-002**: System MUST provide `xpuIn2Wav` module to convert supported lossless formats to standard WAV format
- **FR-003**: System MUST provide `xpuPlay` module to output audio to local playback device
- **FR-004**: System MUST provide `xpuQueue` module to manage playback queue with add, list, next, previous, pause, resume, clear operations
- **FR-005**: System MUST provide `xpuProcess` module supporting volume control, EQ presets (rock, pop, classical, jazz, flat), and fade-in/fade-out
- **FR-006**: System MUST provide `xpuDaemon` to run as background process managing state and coordinating CLI modules

#### Audio Format Support (MVP Constraints)

- **FR-007**: System MUST support FLAC format up to 96kHz sample rate, 24-bit depth, stereo
- **FR-008**: System MUST support WAV format up to 96kHz sample rate, 24-bit depth, stereo
- **FR-009**: System MUST NOT support lossy formats (MP3, AAC, OGG) in MVP
- **FR-010**: System MUST maintain audio quality through the processing pipeline with no transcoding to lossy formats

#### xpuIn2Wav FFT Caching (Core Performance Feature)

- **FR-011**: xpuIn2Wav MUST compute FFT frequency spectrum data during conversion
- **FR-012**: xpuIn2Wav MUST cache FFT data to disk for reuse by subsequent modules
- **FR-013**: xpuIn2Wav MUST support configurable FFT size (1024/2048/4096/8192) with default 2048
- **FR-014**: xpuIn2Wav MUST support disabling FFT cache via `--no-fft-cache` flag
- **FR-015**: FFT cache MUST be stored in user's cache directory (~/.cache/xpu/fft on Linux, ~/Library/Caches/xpu/fft on macOS, %LOCALAPPDATA%\xpu\cache\fft on Windows)
- **FR-016**: FFT cache MUST include magnitude.bin (amplitude spectrum), phase.bin (phase spectrum), meta.json (metadata), config.json (FFT parameters)
- **FR-017**: FFT cache data format MUST be binary float32 arrays with dimensions [num_frames × freq_bins × channels]
- **FR-018**: xpuIn2Wav MUST output cache_id to stderr for use by other modules
- **FR-019**: Cached FFT replay MUST be at least 10x faster than recomputing from audio file

#### Module Communication Protocol

- **FR-020**: CLI modules MUST communicate via Unix pipes (stdin/stdout) following the pipeline pattern
- **FR-021**: CLI modules MUST output structured JSON for metadata and status information via stdout
- **FR-022**: CLI modules MUST use stderr for logging and error messages in JSON format
- **FR-023**: CLI modules MUST return standardized exit codes (0 for success, non-zero for errors)
- **FR-024**: All JSON messages MUST follow version 1.0 protocol specification with fields: version, success, data/error, request_id

#### MCP Integration (MVP Phase 2)

- **FR-025**: xpuDaemon MUST implement MCP server protocol over stdio (JSON-RPC 2.0)
- **FR-026**: xpuDaemon MUST expose MCP tools for all playback controls (play, pause, resume, stop, seek)
- **FR-027**: xpuDaemon MUST expose MCP tools for queue operations (add, list, clear, next, previous)
- **FR-028**: xpuDaemon MUST expose MCP tools for metadata queries (get metadata, search)
- **FR-029**: xpuDaemon MUST expose MCP tools for audio processing (volume, EQ, mute)
- **FR-030**: MCP tool calls MUST respond within 200ms for all operations except actual audio playback

#### REST API (MVP Phase 1)

- **FR-031**: xpuDaemon MUST provide REST API over HTTP for playback control
- **FR-032**: REST API MUST support CRUD operations on the playback queue
- **FR-033**: REST API MUST return JSON responses for all endpoints
- **FR-034**: REST API MUST include appropriate HTTP status codes and error messages
- **FR-035**: xpuDaemon MUST support at least 10 concurrent REST API connections

#### Module Orchestrator

- **FR-036**: xpuDaemon MUST include a module orchestrator to coordinate CLI module execution
- **FR-037**: Orchestrator MUST manage the lifecycle of module processes (spawn, monitor, terminate)
- **FR-038**: Orchestrator MUST maintain playback state separate from CLI modules (stateless modules, stateful daemon)

#### Queue State Persistence

- **FR-039**: xpuQueue MUST persist queue state to JSON file on every modification
- **FR-040**: Queue state MUST be restored on daemon restart
- **FR-041**: Queue MUST support maximum 1000 tracks in MVP
- **FR-042**: Queue operations MUST complete within 50ms

#### Error Handling

- **FR-043**: System MUST provide clear error messages for unsupported file formats
- **FR-044**: System MUST provide clear error messages for corrupted or invalid audio files
- **FR-045**: System MUST gracefully handle audio device disconnection during playback
- **FR-046**: System MUST validate all input parameters before executing operations
- **FR-047**: System MUST handle FFT cache read/write failures gracefully
- **FR-048**: System MUST provide specific error codes for different failure scenarios (1000-1099 range)

#### Configuration

- **FR-049**: System MUST read configuration from xpuSetting.conf in standard config location
- **FR-050**: Configuration MUST specify target sample rate, bit depth, and channels for playback
- **FR-051**: Configuration MUST specify audio output device selection
- **FR-052**: Configuration MUST include FFT cache settings (enabled, cache_dir, fft_size, hop_size, window)
- **FR-053**: System MUST use sensible defaults if configuration file is not present
- **FR-054**: Configuration file format MUST be TOML

#### Platform Support

- **FR-055**: System MUST support Linux (ALSA for audio output)
- **FR-056**: System MUST support macOS (CoreAudio for audio output)
- **FR-057**: System MUST support Windows (WASAPI for audio output)
- **FR-058**: Cache directory paths MUST be platform-specific following OS conventions

#### Performance Requirements

- **FR-059**: Audio playback MUST start within 100ms of issuing the play command
- **FR-060**: Memory usage for a single playback stream MUST stay below 500MB
- **FR-061**: FFT cache computation for a 5-minute song MUST complete within 30 seconds on modern hardware
- **FR-062**: System MUST play audio continuously without gaps when transitioning between queue items

### Key Entities

- **Audio Track**: Represents a music file with metadata (title, artist, album, duration, file path, codec, sample rate, bit depth, channels, file_size, modified_time)
- **Playback Queue**: Ordered list of Audio Tracks with current position indicator, queue_id, name, shuffle_mode, repeat_mode, created/modified timestamps
- **FFT Cache**: Stored frequency spectrum data computed from audio files (cache_id, cache_path, audio_info, fft_info, files: magnitude.bin, phase.bin, meta.json, config.json)
- **Playback State**: Current playback status (playing/paused/stopped), current track, position within track, volume level, active EQ preset, is_muted flag
- **Daemon Session**: Running instance of xpuDaemon with process ID, configuration, and active connections (MCP clients, REST API clients)
- **CLI Protocol Message**: Standard message format with version, command, params, request_id (request); version, success, data/error, request_id (response)
- **Configuration**: TOML-based config with [playback], [fft_cache], [queue], [rest_api], [mcp], [logging] sections

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can play a local FLAC file by executing a single command pipeline within 5 seconds of startup
- **SC-002**: Audio playback starts within 100ms of issuing the play command (measured from command to audio output)
- **SC-003**: The system supports playback of audio files up to 96kHz sample rate and 24-bit depth without quality degradation
- **SC-004**: Queue operations (add, next, previous, pause, resume) complete within 50ms
- **SC-005**: FFT cache computation for a 5-minute song completes within 30 seconds on modern hardware
- **SC-006**: Replaying a cached FFT file is at least 10x faster than recomputing from the audio file
- **SC-007**: xpuDaemon can handle at least 10 concurrent REST API connections without performance degradation
- **SC-008**: MCP tool calls respond within 200ms for all operations except actual audio playback
- **SC-009**: Memory usage for a single playback stream stays below 500MB
- **SC-010**: The system plays audio continuously without gaps or artifacts when transitioning between queue items
- **SC-011**: All CLI modules can be chained together in a pipeline and execute correctly with no manual intervention
- **SC-012**: Error messages are clear and actionable, allowing users to understand and resolve issues without consulting documentation
- **SC-013**: FFT cache files are correctly formatted and can be read by multiple modules (xpuProcess, xpuVisualize, etc.)
- **SC-014**: Queue state persists across daemon restarts with no data loss

---

## Out of Scope for MVP

The following features are explicitly NOT included in the MVP scope and are deferred to future versions:

### Deferred to v1.1+
- Electronic tube simulation (12AX7, EL34, 6L6)
- Advanced DSP effects (chorus, flanger, phaser, reverb)
- DLNA/AirPlay streaming protocols

### Deferred to v1.2+
- Audio visualization (xpuVisualize)
- Online database queries (MusicBrainz, AcousticBrainz)

### Explicitly Out of Scope
- Multi-user support (MVP is single-user local only)
- Cloud synchronization (local storage only)
- ALAC and DSD format support (only FLAC and WAV in MVP)
- Sample rates above 96kHz (future versions will support up to 768kHz)
- Multi-channel audio (5.1, 7.1) - only stereo in MVP
- Simultaneous playback of multiple audio streams
- Remote GPU acceleration for DSP processing (distributed execution mode)
- Server to edge device streaming (xpuStream module)
- xpuFingerprint module (audio fingerprinting)
- xpuClassify module (music classification)
- xpuMeta module (metadata editing)
- xpuQuery module (online database queries)
- xpuPlaylist module (smart playlist generation)
- xpuMatch module (duplicate detection)
- Web UI

### Note on FFT Cache

FFT caching is a CORE feature in MVP, not deferred. The architecture is designed around:
1. xpuIn2Wav computes FFT once and caches
2. Other modules (xpuProcess, future xpuVisualize, xpuFingerprint) can reuse cached FFT
3. This provides 10-100x performance improvement for frequency-domain operations
