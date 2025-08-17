# edl2ffmpeg

High-performance EDL (Edit Decision List) renderer using direct FFmpeg library integration.

## Overview

edl2ffmpeg is an open-source tool that renders EDL files to compressed video output by directly linking with FFmpeg libraries, eliminating the performance overhead of pipe-based communication. It features a clean, extensible architecture designed for maximum performance with support for SIMD optimizations and lazy evaluation of compositor instructions.

## Features

- **Direct FFmpeg Integration**: Links directly with FFmpeg libraries for maximum performance
- **Publishing EDL Format**: Supports the publishing EDL JSON format with 'uri' field for media references
- **Lazy Evaluation**: Transforms EDL timeline into lazily-evaluated compositor instructions
- **Memory Efficient**: Frame buffer pooling to minimize allocations
- **Extensible Effects System**: Modular effect architecture ready for SIMD/GPU acceleration
- **Multi-format Support**: Handles common codecs (H.264, H.265, ProRes, VP9)
- **Hardware Acceleration**: Auto-detection and support for NVENC, VAAPI, VideoToolbox
- **Zero-copy GPU Passthrough**: Frames without effects stay on GPU for maximum performance
- **Real-time Progress**: Visual progress bar with FPS and ETA reporting
- **Track Alignment**: Automatic null clip insertion for proper track synchronization
- **Sources Array Support**: Single-element sources arrays for future multi-source clips
- **Generate Sources**: Built-in black frame generation

## Building

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- FFmpeg development libraries (supports 2.x, 3.x, 4.x, 5.x)
- nlohmann/json (automatically fetched if not found)

### Build Instructions

```bash
# Clone repository
git clone https://github.com/yourusername/edl2ffmpeg.git
cd edl2ffmpeg

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests (optional)
ctest

# Install (optional)
sudo make install
```

### CMake Options

- `BUILD_TESTS`: Build test suite (ON by default)
- `ENABLE_SIMD`: Enable SIMD optimizations (ON by default)
- `ENABLE_GPU`: Enable GPU acceleration (OFF by default)
- `USE_SYSTEM_FFMPEG`: Use system FFmpeg instead of building (ON by default)
- `PROFILE_BUILD`: Enable profiling instrumentation (OFF by default)

## Usage

### Basic Usage

```bash
edl2ffmpeg input.json output.mp4
```

### Command Line Options

```
Usage: edl2ffmpeg <edl_file> <output_file> [options]

Options:
  -c, --codec <codec>      Video codec (default: libx264)
  -b, --bitrate <bitrate>  Video bitrate in bps (default: auto)
  -p, --preset <preset>    Encoder preset (default: medium)
  --crf <value>            Constant Rate Factor (default: 23)
  --hw-accel <type>        Hardware acceleration (auto/cuda/vaapi/videotoolbox/none, default: auto)
  --hw-encode              Force hardware encoding when available
  --hw-decode              Force hardware decoding when available
  --async-depth <n>        Hardware encoder async depth (default: 4)
  -v, --verbose            Enable verbose logging
  -q, --quiet              Suppress all non-error output
  -h, --help               Show this help message

Examples:
  edl2ffmpeg input.json output.mp4
  edl2ffmpeg input.json output.mp4 --codec libx265 --crf 28
  edl2ffmpeg input.json output.mp4 -b 8000000 -p fast
  edl2ffmpeg input.json output.mp4 --hw-accel cuda --hw-encode   # NVIDIA GPU encoding
  edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox --hw-encode --hw-decode  # Full macOS hardware acceleration
  edl2ffmpeg input.json output.mp4 --hw-accel none    # Force software encoding
```

## EDL Format

The tool supports the publishing EDL JSON format. See [UNSUPPORTED_EDL_FEATURES.md](docs/UNSUPPORTED_EDL_FEATURES.md) for features not yet implemented.

### Example EDL Files

#### Simple Video Clip
```json
{
  "fps": 30,
  "width": 1920,
  "height": 1080,
  "clips": [
    {
      "in": 0,
      "out": 10,
      "track": {
        "type": "video",
        "number": 1
      },
      "source": {
        "uri": "video.mp4",
        "in": 0,
        "out": 10
      },
      "topFade": 1.0,
      "tailFade": 0.5
    }
  ]
}
```

#### Using Sources Array
```json
{
  "clips": [
    {
      "in": 0,
      "out": 5,
      "track": {"type": "video", "number": 1},
      "sources": [
        {
          "uri": "clip.mp4",
          "in": 0,
          "out": 5
        }
      ]
    }
  ]
}
```

#### Black Frame Generation
```json
{
  "clips": [
    {
      "in": 0,
      "out": 3,
      "track": {"type": "video", "number": 1},
      "source": {
        "generate": {"type": "black"},
        "in": 0,
        "out": 3,
        "width": 1920,
        "height": 1080
      }
    }
  ]
}
```

#### With Effects Track
```json
{
  "clips": [
    {
      "in": 0,
      "out": 10,
      "track": {"type": "video", "number": 1},
      "source": {
        "uri": "video.mp4",
        "in": 0,
        "out": 10
      }
    },
    {
      "in": 0,
      "out": 10,
      "track": {
        "type": "video",
        "number": 1,
        "subtype": "effects",
        "subnumber": 1
      },
      "source": {
        "type": "brightness",
        "in": 0,
        "out": 10,
        "value": 1.5
      }
    }
  ]
}
```

### EDL Properties

- `fps`: Frame rate of the output video (default: 30)
- `width`: Width of the output video in pixels (default: 1920)
- `height`: Height of the output video in pixels (default: 1080)
- `clips`: Array of clip objects

### Clip Properties

- `in`: Start time on timeline (seconds, required)
- `out`: End time on timeline (seconds, required)
- `track`: Track information (required)
  - `type`: Track type ("video", "audio", "subtitle", "burnin")
  - `number`: Track number (positive integer, 1-based)
  - `subtype`: Optional subtype ("effects", "transform", "colour", "level", "pan")
  - `subnumber`: Optional ordering for effects subtracks (default: 1)
- `source` or `sources`: Source definition (required)
  - **Note**: `sources` array currently supports only single element
  - `in`: Start time in source (seconds, required)
  - `out`: End time in source (seconds, required, must be > in)
  
### Source Types

#### Media Source (from file)
```json
{
  "uri": "path/to/video.mp4",
  "in": 0,
  "out": 10,
  "trackId": "V1",
  "width": 1920,
  "height": 1080,
  "fps": 30,
  "speed": 1.0,
  "gamma": 1.0
}
```

#### Generate Source (currently only "black" supported)
```json
{
  "generate": {
    "type": "black"
  },
  "in": 0,
  "out": 5,
  "width": 1920,
  "height": 1080
}
```

#### Effect Source (for effects tracks)
```json
{
  "type": "brightness",
  "in": 0,
  "out": 10,
  "value": 1.5
}
```

### Optional Clip Properties

- `topFade`: Fade-in duration (seconds)
- `tailFade`: Fade-out duration (seconds)
- `motion`: Pan/zoom/rotation parameters
- `transition`: Transition settings (limited support)
- `channelMap`: Audio channel mapping (1:1 mapping only)
- `textFormat`: Text formatting for subtitles/burnin
- `sync`: Sync group ID

## Architecture

The system follows a pipeline architecture:

1. **EDL Parser**: Reads and parses JSON EDL files
2. **Instruction Generator**: Transforms EDL timeline into per-frame compositor instructions
3. **Frame Decoder**: Decodes source frames using FFmpeg
4. **Frame Compositor**: Applies transforms and effects
5. **Frame Encoder**: Encodes output frames using FFmpeg

### Key Components

- `EDLParser`: Parses EDL JSON files into internal structures
- `InstructionGenerator`: Generates compositor instructions with lazy evaluation
- `FFmpegDecoder`: Wraps FFmpeg decoding with frame-accurate seeking
- `FFmpegEncoder`: Wraps FFmpeg encoding with configurable codecs
- `HardwareAcceleration`: Auto-detects and manages hardware encoders/decoders
- `FrameCompositor`: Processes frames according to instructions
- `FrameBufferPool`: Manages frame memory with pooling

## Performance

### Current Performance

- 1080p30 video: 250+ fps processing on Apple Silicon
- Hardware encoding: 300+ fps with VideoToolbox on macOS
- Zero-copy GPU passthrough for frames without effects
- Memory usage: Under 500MB for typical operations
- Support for H.264, H.265, ProRes, VP9 codecs
- Platform-specific optimizations for consistent output

### Optimization Roadmap

- [x] Hardware encoder/decoder support (IMPLEMENTED)
- [x] Zero-copy GPU passthrough for unmodified frames (IMPLEMENTED)
- [x] Platform-specific B-frame handling for consistency (IMPLEMENTED)
- [ ] SIMD optimizations for effects (SSE4.2, AVX2, AVX-512)
- [ ] GPU acceleration for effects (OpenCL, CUDA, Vulkan)
- [ ] Multi-threaded pipeline architecture
- [ ] Multiple source concatenation support
- [ ] Advanced transition effects

## Development

### Project Structure

```
edl2ffmpeg/
├── src/
│   ├── edl/           # EDL parsing and data structures
│   ├── compositor/    # Frame composition and effects
│   ├── media/         # FFmpeg encoder/decoder wrappers
│   └── utils/         # Logging and memory management
├── tests/             # Test suite
└── docs/              # Documentation
```

### Contributing

Contributions are welcome! Please ensure:

1. Code follows C++20 standards
2. All tests pass
3. New features include tests
4. Documentation is updated

## License

[To be determined - likely MIT or Apache 2.0]

## Acknowledgments

- FFmpeg project for the excellent multimedia libraries
- nlohmann/json for JSON parsing
- Contributors and testers