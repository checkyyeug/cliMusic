# Changelog

All notable changes to XPU will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added - Phase 1 (Current Development)

#### Core Infrastructure
- Complete project structure with CMake build system
- Cross-platform support for Windows, macOS, Linux
- Code quality tools (clang-tidy, clang-format, cppcheck)
- Installation scripts for all platforms

#### Shared Library (libxpu)
- **Protocol Layer**: 60+ error codes with HTTP mapping
- **Utils Layer**: Logger, PlatformUtils, ConfigLoader, ConfigValidator
- **Audio Layer**: AudioFormat, AudioMetadata, AudioProperties
- **Extension Interfaces**: All 8 Phase 2-5 interfaces defined

#### Module 1: xpuLoad (Audio File Parser)
- Complete FFmpeg integration with metadata extraction
- Support for FLAC, WAV, ALAC, DSD, MP3, AAC, OGG, OPUS
- **NEW**: DSD format support (DSF files) with 5th-order noise shaping
- Metadata extraction (title, artist, album, track, genre, year)

#### Module 2: xpuIn2Wav (Format Converter + FFT Cache)
- FFTW3 integration with Hann windowing
- **NEW**: Complete FFT cache implementation with SHA256 hashing
- Cache file structure (meta.json, magnitude.bin, phase.bin, config.json)
- Cache statistics tracking (hit rate, size)
- Atomic writes for data safety
- 10-100x speedup potential

#### Module 3: xpuPlay (Low-Latency Audio Output)
- **NEW**: Cross-platform audio backend abstraction
- **NEW**: WASAPI backend (Windows) - <50ms latency with exclusive mode
- **NEW**: CoreAudio backend (macOS) - HAL callback-based
- **NEW**: ALSA backend (Linux) - dmix support
- Real-time status output (10Hz push to stdout)
- Device enumeration and selection

#### Module 4: xpuQueue (Queue Management)
- **NEW**: Thread-safe queue manager
- **NEW**: JSON persistence with atomic writes
- 4 playback modes: Sequential, Random, LoopSingle, LoopAll
- Shuffle functionality
- Queue navigation (next, previous, jump)

#### Module 5: xpuProcess (DSP Effects)
- **NEW**: Volume control (0-200%) with soft clipping
- **NEW**: Fade-in/fade-out effects
- **NEW**: 3-band equalizer with 6 presets
- Biquad filter implementation

#### Module 6: xpuDaemon (Background Daemon)
- **NEW**: Daemon lifecycle management
- **NEW**: Process spawning and monitoring
- **NEW**: Pipeline orchestration (xpuLoad → xpuIn2Wav → xpuPlay)
- **NEW**: Configuration hot-reload with file watcher
- **NEW**: State persistence with JSON format
- Signal handling (SIGTERM, SIGINT, SIGHUP)

#### Testing
- Complete test framework (unit, integration, contract, benchmark)
- **NEW**: Unit tests for AudioFileLoader and DSDDecoder
- **NEW**: Unit tests for FFTEngine with cache testing
- **NEW**: Unit tests for QueueManager
- **NEW**: Unit tests for DSP effects (Volume, Fade, EQ)
- **NEW**: Integration tests for complete pipeline

#### Documentation
- **NEW**: QUICKSTART.md - Installation and basic usage
- **NEW**: ARCHITECTURE.md - Complete system architecture
- **NEW**: BUILD.md - Build instructions for all platforms

### Changed
- Improved error handling with 60+ error codes
- Enhanced configuration system with validation
- Better cross-platform path handling

### Fixed
- Memory leaks in audio decoders
- Race conditions in queue operations
- Buffer overflow in DSD decoder

## [1.0.0] - Target Release

### Planned Features (Phase 2-5)

#### Phase 2: REST API
- HTTP/JSON API
- Web interface
- Remote control
- API documentation

#### Phase 3: Extended Features
- Audio fingerprinting (Chromaprint)
- Genre classification (Machine Learning)
- Visualization (Spectrum, Waveform, Envelope)
- Advanced DSP (Reverb, Chorus, Tube Amp, Phaser, Flanger)
- Metadata enrichment (MusicBrainz, Acoustid)

#### Phase 4: Distribution
- Audio streaming (HTTP, multicast)
- Distributed cache (Redis, Memcached)
- Network audio (DLNA, AirPlay)

#### Phase 5: Advanced AI
- Agent protocol integration
- AI-powered recommendations
- Smart playlist generation
- Natural language interface

## [0.1.0] - Prototype

### Added
- Initial project structure
- Basic audio playback
- Simple queue management
- Configuration system

---

## Versioning Scheme

- **Major version**: Incompatible API changes
- **Minor version**: Backwards-compatible functionality additions
- **Patch version**: Backwards-compatible bug fixes

## Release Schedule

- **Phase 1**: Current (Foundation & Core Modules)
- **Phase 2**: Q2 2026 (REST API)
- **Phase 3**: Q3 2026 (Extended Features)
- **Phase 4**: Q4 2026 (Distribution)
- **Phase 5**: Q1 2027 (Advanced AI)

## Migration Guide

### From 0.x to 1.0

1. Update configuration file format
2. Migrate queue state
3. Update custom scripts for new CLI

### From 1.0 to 2.0 (Phase 2)

1. API endpoints changed
2. REST API required for remote control
3. Web interface available

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Development workflow
- Code review process
- Testing requirements
