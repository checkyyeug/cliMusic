# xpuApi - REST API Server

XPU REST API Server with SSE (Server-Sent Events) streaming support.

## Features

- RESTful API for audio playback control
- SSE streaming for real-time audio data and status updates
- Multi-session support (concurrent playback)
- Queue management
- Device management

## Building

```bash
cd xpu
mkdir build && cd build
cmake ..
make xpuApi
```

## Usage

### Starting the Server

```bash
# Basic usage (localhost:8080)
xpuApi

# Custom port
xpuApi --port 9000

# Listen on all interfaces
xpuApi --host 0.0.0.0 --port 8080

# Verbose mode
xpuApi --verbose
```

### API Endpoints

#### Health Check
```http
GET /api/health
```

Response:
```json
{
  "status": "ok",
  "version": "4.1.0",
  "service": "xpu-api"
}
```

#### Play Audio
```http
POST /api/v3/play
Content-Type: application/json

{
  "file": "~/Music/song.flac",
  "options": {
    "volume": 0.8,
    "device": "default",
    "auto_resample": true
  }
}
```

Response:
```json
{
  "success": true,
  "data": {
    "session_id": "550e8400-e29b-41d4-a716-446655440000",
    "stream_url": "/api/stream/audio?session=550e8400-e29b-41d4-a716-446655440000",
    "metadata": {
      "file": "~/Music/song.flac"
    }
  }
}
```

#### Pause/Resume/Stop
```http
# Pause
POST /api/v3/pause
{"session": "session_id"}

# Resume
POST /api/v3/resume
{"session": "session_id"}

# Stop
POST /api/v3/stop
{"session": "session_id"}
```

#### Seek
```http
POST /api/v3/seek
{
  "session": "session_id",
  "position": 60.0
}
```

#### Set Volume
```http
POST /api/v3/volume
{
  "session": "session_id",
  "volume": 0.8,
  "fade_ms": 500
}
```

#### Get Status
```http
GET /api/v3/status
```

#### Queue Management
```http
# Add to queue
POST /api/v3/queue/add
{
  "files": ["~/Music/song1.flac", "~/Music/song2.flac"],
  "position": -1
}

# Get queue
GET /api/v3/queue?session=session_id

# Clear queue
DELETE /api/v3/queue?session=session_id

# Next track
POST /api/v3/queue/next
{"session": "session_id"}
```

#### List Devices
```http
GET /api/v3/devices
```

### SSE Streaming

Connect to the SSE stream to receive real-time audio data and status updates:

```http
GET /api/stream/audio?session=550e8400-e29b-41d4-a716-446655440000
Accept: text/event-stream
```

SSE Events:
- `metadata` - Audio format information
- `audio` - Audio chunk data
- `status` - Playback status updates
- `complete` - Playback finished
- `error` - Error occurred

### Example: JavaScript Client

```javascript
// Start playback
const response = await fetch('http://localhost:8080/api/v3/play', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({file: '~/Music/song.flac'})
});
const {stream_url, session_id} = await response.json();

// Connect to SSE stream
const eventSource = new EventSource(`http://localhost:8080${stream_url}`);

eventSource.addEventListener('metadata', (e) => {
  console.log('Metadata:', JSON.parse(e.data));
});

eventSource.addEventListener('status', (e) => {
  const status = JSON.parse(e.data);
  console.log('Status:', status.state, status.position);
});

eventSource.addEventListener('complete', () => {
  console.log('Playback finished');
  eventSource.close();
});

// Pause playback
await fetch('http://localhost:8080/api/v3/pause', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({session: session_id})
});
```

### Example: cURL

```bash
# Start playback
curl -X POST http://localhost:8080/api/v3/play \
  -H "Content-Type: application/json" \
  -d '{"file": "~/Music/song.flac"}'

# Get status
curl http://localhost:8080/api/v3/status

# Pause
curl -X POST http://localhost:8080/api/v3/pause \
  -H "Content-Type: application/json" \
  -d '{"session": "your-session-id"}'

# Listen to SSE stream
curl -N http://localhost:8080/api/stream/audio?session=your-session-id
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    xpuApi (HTTP Server)                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │ REST API    │  │ SSE Stream  │  │ Session Manager │  │
│  └─────────────┘  └─────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   CLI Pipeline (fork/exec)              │
│   xpuLoad → xpuIn2Wav → xpuProcess → xpuPlay           │
└─────────────────────────────────────────────────────────┘
```

## Dependencies

- cpp-httplib (HTTP server)
- nlohmann/json (JSON parsing)
- spdlog (Logging)
- XPU CLI modules (xpuLoad, xpuIn2Wav, xpuPlay, etc.)

## License

Same as XPU project.
