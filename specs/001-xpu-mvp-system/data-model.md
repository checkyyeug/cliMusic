# Data Model: XPU AI-Ready Music Playback System MVP

**Feature**: 001-xpu-mvp-system
**Date**: 2026-01-07
**Status**: Draft

---

## Overview

This document defines the core data structures used throughout the XPU system. These structures are used for:
- CLI module communication (stdin/stdout JSON protocol)
- REST API request/response bodies
- MCP tool parameters and return values
- Queue state persistence
- FFT cache metadata

---

## 1. Audio Track

Represents a music file with metadata.

### Schema

```json
{
  "track_id": "string (UUID)",
  "file_path": "string (absolute path)",
  "title": "string",
  "artist": "string",
  "album": "string",
  "duration": "number (seconds, float)",
  "codec": "string (flac|wav)",
  "sample_rate": "number (Hz, integer)",
  "bit_depth": "number (bits, integer)",
  "channels": "number (integer)",
  "file_size": "number (bytes, integer)",
  "modified_time": "string (ISO 8601)"
}
```

### Validation Rules

| Field | Required | Type | Constraints |
|-------|----------|------|-------------|
| track_id | Yes | UUID | Auto-generated |
| file_path | Yes | string | Must exist, readable, .flac or .wav extension |
| title | No | string | Max 255 chars |
| artist | No | string | Max 255 chars |
| album | No | string | Max 255 chars |
| duration | Yes | number | > 0 |
| codec | Yes | string | "flac" or "wav" only |
| sample_rate | Yes | number | 44100, 48000, 88200, 96000 |
| bit_depth | Yes | number | 16 or 24 |
| channels | Yes | number | 2 (stereo) for MVP |
| file_size | Yes | number | > 0 |
| modified_time | Yes | string | ISO 8601 format |

### Relationships

- One Track can be in multiple Queues
- One Track has one FFT Cache (if processed)

---

## 2. Playback Queue

Ordered list of Audio Tracks with current position indicator.

### Schema

```json
{
  "queue_id": "string (UUID)",
  "name": "string",
  "tracks": ["array of Track objects"],
  "current_index": "number (integer, -1 if empty)",
  "shuffle_mode": "boolean",
  "repeat_mode": "string (off|all|one)",
  "created_time": "string (ISO 8601)",
  "modified_time": "string (ISO 8601)"
}
```

### Validation Rules

| Field | Required | Type | Constraints |
|-------|----------|------|-------------|
| queue_id | Yes | UUID | Auto-generated |
| name | Yes | string | Max 255 chars |
| tracks | Yes | array | Max 1000 tracks in MVP |
| current_index | Yes | number | -1 (empty) to tracks.length-1 |
| shuffle_mode | Yes | boolean | |
| repeat_mode | Yes | string | "off", "all", or "one" |
| created_time | Yes | string | ISO 8601 format |
| modified_time | Yes | string | ISO 8601 format |

### State Transitions

```
[Empty] --add--> [Has Tracks] --play--> [Playing]
   ^                             |
   |                             v
   +-----------remove------------[Paused]
                             |
                             v
                         [Stopped]
```

### Relationships

- One Queue contains multiple Tracks (ordered)
- One Queue has one current Playback State

---

## 3. FFT Cache

Stored frequency spectrum data computed from audio files.

### Metadata Schema

```json
{
  "cache_id": "string (content hash)",
  "track_id": "string (UUID)",
  "cache_path": "string (absolute path to cache directory)",
  "audio_info": {
    "duration": "number (seconds)",
    "sample_rate": "number (Hz)",
    "channels": "number",
    "bit_depth": "number"
  },
  "fft_info": {
    "fft_size": "number (1024|2048|4096|8192)",
    "hop_size": "number (typically fft_size/2)",
    "window_function": "string (hann|hamming|blackman)",
    "num_frames": "number",
    "freq_bins": "number",
    "freq_resolution": "number (Hz)"
  },
  "files": {
    "magnitude": "string (filename)",
    "phase": "string (filename)",
    "meta": "string (filename)",
    "config": "string (filename)"
  },
  "created_time": "string (ISO 8601)",
  "size_bytes": "number"
}
```

### Validation Rules

| Field | Required | Type | Constraints |
|-------|----------|------|-------------|
| cache_id | Yes | string | SHA-256 hash of audio content |
| track_id | Yes | UUID | References Track.track_id |
| fft_size | Yes | number | Power of 2, 1024-8192 |
| num_frames | Yes | number | > 0 |
| freq_bins | Yes | number | fft_size/2 + 1 |
| size_bytes | Yes | number | Total cache size |

### File Structure

```
~/.cache/xpu/fft/<cache_id>/
├── meta.json          # Metadata schema above
├── magnitude.bin      # Binary float32 [frames x bins x channels]
├── phase.bin          # Binary float32 [frames x bins x channels]
└── config.json        # FFT configuration
```

### Binary Format

- **magnitude.bin**: Float32 array, row-major (C order)
- **phase.bin**: Float32 array, row-major (C order)
- Dimensions: [num_frames, freq_bins, channels]
- Size calculation: num_frames * freq_bins * channels * 4 bytes

### Relationships

- One FFT Cache belongs to one Track
- One FFT Cache can be used by multiple modules (xpuProcess, xpuFingerprint)

---

## 4. Playback State

Current playback status and position.

### Schema

```json
{
  "state": "string (stopped|playing|paused)",
  "current_track": "Track object or null",
  "position": "number (seconds, float)",
  "volume": "number (0.0-1.0)",
  "eq_preset": "string (flat|rock|pop|classical|jazz)",
  "is_muted": "boolean",
  "duration": "number (seconds, float or null)",
  "remaining": "number (seconds, float or null)"
}
```

### Validation Rules

| Field | Required | Type | Constraints |
|-------|----------|------|-------------|
| state | Yes | string | "stopped", "playing", or "paused" |
| position | Yes | number | 0 to duration |
| volume | Yes | number | 0.0 to 1.0 |
| eq_preset | Yes | string | Valid preset name |
| is_muted | Yes | boolean | |

### State Machine

```
stopped ──play──> playing
   ^               |
   |              v
   +----stop------ paused
         ^         |
         |         v
         +------pause
```

### Relationships

- One Playback State associated with one Queue
- One Playback State references one current Track (if any)

---

## 5. EQ Preset

Equalizer configuration presets.

### Schema

```json
{
  "preset_name": "string (flat|rock|pop|classical|jazz)",
  "bands": [
    {"frequency": "number (Hz)", "gain_db": "number"}
  ]
}
```

### Preset Definitions

| Preset | 60Hz | 250Hz | 1kHz | 4kHz | 16kHz |
|--------|------|-------|------|------|-------|
| flat | 0 | 0 | 0 | 0 | 0 |
| rock | +3 | +1 | 0 | +2 | +3 |
| pop | +1 | +3 | +2 | +1 | +2 |
| classical | +2 | +1 | 0 | +1 | +2 |
| jazz | +2 | +1 | +2 | +3 | +1 |

---

## 6. Daemon Session

Running instance of xpuDaemon.

### Schema

```json
{
  "session_id": "string (UUID)",
  "pid": "number (process ID)",
  "start_time": "string (ISO 8601)",
  "config": {
    "rest_api_port": "number",
    "mcp_enabled": "boolean",
    "log_level": "string"
  },
  "connections": {
    "rest_clients": "number",
    "mcp_clients": "number"
  }
}
```

---

## 7. CLI Protocol Message

Standard message format for CLI module communication.

### Request Schema (stdin)

```json
{
  "version": "string (major.minor)",
  "command": "string",
  "params": "object",
  "request_id": "string (UUID)"
}
```

### Response Schema (stdout)

```json
{
  "version": "string (major.minor)",
  "success": "boolean",
  "data": "object or null",
  "error": "string or null",
  "request_id": "string (UUID)"
}
```

### Log Message Schema (stderr)

```json
{
  "level": "string (debug|info|warn|error)",
  "message": "string",
  "timestamp": "string (ISO 8601)",
  "module": "string",
  "context": "object"
}
```

---

## 8. Configuration

xpuSetting.conf structure (TOML format).

### Schema

```toml
[playback]
target_sample_rate = 96000
target_bit_depth = 32
channels = 2
default_device = "auto"

[fft_cache]
enabled = true
cache_dir = "~/.cache/xpu/fft"
fft_size = 2048
hop_size = 1024
window = "hann"

[queue]
max_tracks = 1000
auto_save = true
save_interval = 60

[rest_api]
host = "127.0.0.1"
port = 8080
enabled = true

[mcp]
enabled = true
transport = "stdio"

[logging]
level = "info"
file = "~/.config/xpu/xpu.log"
max_size_mb = 100
```

---

## Entity Relationship Diagram

```
┌─────────────┐
│   Track     │
│             │
│ - track_id  │──────┐
│ - file_path │      │
│ - metadata  │      │ 1
│ - codec     │      │
│ - duration  │      │
└─────────────┘      │
                     │
        ┌────────────┴────────────────────┐
        │ 1                             │ N
        ▼                                ▼
┌─────────────┐                  ┌─────────────┐
│    Queue    │                  │  FFT Cache  │
│             │                  │             │
│ - queue_id  │                  │ - cache_id  │
│ - tracks[]  │──────┐           │ - fft_info  │
│ - position  │      │ 1         │ - files     │
└─────────────┘      │           └─────────────┘
                     │
                     │ N
                     ▼
            ┌─────────────┐
            │Playback State│
            │             │
            │ - state     │
            │ - position  │
            │ - volume    │
            │ - eq_preset │
            └─────────────┘
```

---

## Data Serialization

### JSON Format

All CLI protocol, REST API, and MCP messages use JSON.

**Libraries**:
- C++: nlohmann/json
- Python: built-in json module

### Binary Format

FFT cache binary files use raw float32 in native byte order.

**Endianness**: Native (little-endian on x86/x64_64)

---

## Persistence Strategy

| Data | Storage | Format | Invalidation |
|------|---------|--------|--------------|
| Queue state | File | JSON | Manual save, auto-save |
| FFT cache | Files | JSON + binary | Content hash (cache_id) |
| Configuration | File | TOML | Manual edit |
| Logs | File | Text | Rotation |

---

## Versioning

### Schema Version

Current version: `1.0.0`

Format: `major.minor.patch`

- **major**: Breaking changes to schema
- **minor**: Additions, backward compatible
- **patch**: Bug fixes, documentation

### Migration Strategy

- Major versions: Data migration script required
- Minor versions: New fields optional, old fields deprecated
- Patch versions: No changes to data structures
