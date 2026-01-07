# CLI Protocol Specification

**Version**: 1.0.0
**Feature**: 001-xpu-mvp-system

---

## Overview

XPU CLI modules communicate via stdin/stdout using JSON messages. This protocol defines the standard message format, error handling, and module-specific commands.

---

## Transport Layer

- **Input**: stdin (UTF-8 encoded JSON)
- **Output**: stdout (UTF-8 encoded JSON)
- **Logging**: stderr (JSON-formatted log messages)
- **Exit Codes**: 0 (success), non-zero (failure)

---

## Message Format

### Request (stdin → module)

```json
{
  "version": "1.0",
  "command": "string",
  "params": {},
  "request_id": "uuid"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| version | string | Yes | Protocol version |
| command | string | Yes | Command name (module-specific) |
| params | object | Yes | Command parameters |
| request_id | string | Yes | UUID for request tracking |

### Response (module → stdout)

**Success Response**:

```json
{
  "version": "1.0",
  "success": true,
  "data": {},
  "error": null,
  "request_id": "uuid"
}
```

**Error Response**:

```json
{
  "version": "1.0",
  "success": false,
  "data": null,
  "error": {
    "code": "number",
    "message": "string",
    "details": {}
  },
  "request_id": "uuid"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| version | string | Yes | Protocol version |
| success | boolean | Yes | true or false |
| data | object/null | Yes | Response data (success) |
| error | object/null | Yes | Error object (failure) |
| request_id | string | Yes | Matches request |

### Log Message (module → stderr)

```json
{
  "level": "info",
  "message": "string",
  "timestamp": "2026-01-07T12:00:00Z",
  "module": "xpuLoad",
  "context": {}
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| level | string | Yes | debug, info, warn, error |
| message | string | Yes | Log message |
| timestamp | string | Yes | ISO 8601 UTC |
| module | string | Yes | Module name |
| context | object | No | Additional context |

---

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 1000 | UNKNOWN_ERROR | Uncategorized error |
| 1001 | INVALID_REQUEST | Malformed request JSON |
| 1002 | MISSING_PARAM | Required parameter missing |
| 1003 | INVALID_PARAM | Parameter validation failed |
| 1004 | FILE_NOT_FOUND | File not found |
| 1005 | FILE_READ_ERROR | Error reading file |
| 1006 | FILE_PARSE_ERROR | Error parsing file format |
| 1007 | AUDIO_DEVICE_ERROR | Audio device error |
| 1008 | QUEUE_EMPTY | Queue is empty |
| 1009 | QUEUE_FULL | Queue at max capacity |
| 1010 | OUT_OF_RANGE | Index/position out of range |
| 1011 | NOT_SUPPORTED | Operation not supported |
| 1012 | PERMISSION_DENIED | Insufficient permissions |

---

## Module Specifications

### xpuLoad

Parse audio file and extract metadata.

**Command**: `load`

**Request**:
```json
{
  "command": "load",
  "params": {
    "file_path": "string (absolute path)"
  }
}
```

**Response**:
```json
{
  "success": true,
  "data": {
    "track": { /* Audio Track object */ },
    "audio_stream": "base64 encoded PCM data"
  }
}
```

**Errors**:
- 1004: File not found
- 1005: File read error
- 1006: Unsupported codec (not FLAC/WAV)

---

### xpuIn2Wav

Convert to WAV format and compute FFT cache.

**Command**: `convert`

**Request**:
```json
{
  "command": "convert",
  "params": {
    "target_sample_rate": "number (optional, default from config)",
    "target_bit_depth": "number (optional, default from config)",
    "fft_cache": "boolean (optional, default true)",
    "fft_size": "number (optional, default 2048)"
  }
}
```

**Response**:
```json
{
  "success": true,
  "data": {
    "wav_header": { /* WAV format header */ },
    "pcm_data": "base64 encoded PCM",
    "fft_cache": {
      "cache_id": "string",
      "cache_path": "string",
      "fft_info": {}
    }
  }
}
```

**Errors**:
- 1003: Invalid sample rate or bit depth
- 1007: FFT computation error

---

### xpuPlay

Output audio to device.

**Command**: `play`

**Request**:
```json
{
  "command": "play",
  "params": {
    "device": "string (optional, default from config)",
    "buffer_size": "number (optional, default 4096)"
  }
}
```

**Response**:
```json
{
  "success": true,
  "data": {
    "playing": true,
    "device": "string",
    "latency_ms": "number"
  }
}
```

**Command**: `stop`

**Response**:
```json
{
  "success": true,
  "data": {
    "playing": false
  }
}
```

**Errors**:
- 1007: Audio device not found
- 1007: Audio device open failed

---

### xpuQueue

Manage playback queue.

**Command**: `add`

**Request**:
```json
{
  "command": "add",
  "params": {
    "track": { /* Audio Track object */ },
    "position": "number (optional, append to end)"
  }
}
```

**Response**:
```json
{
  "success": true,
  "data": {
    "queue": { /* Queue object */ },
    "added_at": "number (index)"
  }
}
```

**Command**: `list`

**Response**:
```json
{
  "success": true,
  "data": {
    "queue": { /* Queue object */ }
  }
}
```

**Command**: `remove`

**Request**:
```json
{
  "command": "remove",
  "params": {
    "index": "number"
  }
}
```

**Command**: `clear`

**Response**:
```json
{
  "success": true,
  "data": {
    "queue": { /* Empty Queue object */ }
  }
}
```

**Errors**:
- 1008: Queue empty
- 1009: Queue full
- 1010: Index out of range

---

### xpuProcess

Apply DSP processing.

**Command**: `volume`

**Request**:
```json
{
  "command": "volume",
  "params": {
    "level": "number (0.0-1.0)"
  }
}
```

**Command**: `eq`

**Request**:
```json
{
  "command": "eq",
  "params": {
    "preset": "string (flat|rock|pop|classical|jazz)"
  }
}
```

**Command**: `fade`

**Request**:
```json
{
  "command": "fade",
  "params": {
    "type": "string (in|out)",
    "duration_ms": "number"
  }
}
```

**Errors**:
- 1003: Invalid parameter value

---

## Streaming Mode

For binary audio data (PCM), modules can operate in streaming mode:

1. Send JSON request with `"streaming": true`
2. Module responds with JSON metadata
3. Module writes binary PCM data to stdout
4. Module logs progress to stderr

**Example**:

```bash
# Pipeline: xpuLoad | xpuIn2Wav | xpuPlay
xpuLoad song.flac --binary | xpuIn2Wav | xpuPlay
```

In this mode:
- Initial handshake via JSON
- Subsequent data via raw binary
- Final status via stderr JSON

---

## Concurrency Considerations

- Each module instance handles one request at a time
- For concurrent operations, spawn multiple module instances
- Queue state is managed by xpuDaemon (not individual modules)
- Modules are stateless (state in daemon)

---

## Testing

### Contract Tests

Verify protocol compliance:

```python
def test_xpuload_protocol():
    request = {
        "version": "1.0",
        "command": "load",
        "params": {"file_path": "/path/to/song.flac"},
        "request_id": str(uuid4())
    }

    response = run_xpuload(request)

    assert response["version"] == "1.0"
    assert response["request_id"] == request["request_id"]
    assert "data" in response
    assert response["success"] in [True, False]
```

---

## Version Compatibility

### Protocol Versioning

- Major version: Breaking changes
- Minor version: Additions only

### Backward Compatibility

Modules MUST:
- Accept requests with older minor versions
- Respond with requested protocol version
- Default to latest version if not specified
