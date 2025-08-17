# edl2ffmpeg

High-performance EDL (Edit Decision List) renderer using direct FFmpeg library integration.

## Overview

edl2ffmpeg is an open-source tool that renders EDL files to compressed video output by directly linking with FFmpeg libraries, eliminating the performance overhead of pipe-based communication. It features a clean, extensible architecture designed for maximum performance with support for SIMD optimizations and lazy evaluation of compositor instructions.

## Features

- **Direct FFmpeg Integration**: Links directly with FFmpeg libraries for maximum performance
- **Lazy Evaluation**: Transforms EDL timeline into lazily-evaluated compositor instructions
- **Memory Efficient**: Frame buffer pooling to minimize allocations
- **Extensible Effects System**: Modular effect architecture ready for SIMD/GPU acceleration
- **Multi-format Support**: Handles common codecs (H.264, H.265, ProRes)
- **Hardware Acceleration**: Auto-detection and support for NVENC, VAAPI, VideoToolbox
- **Real-time Progress**: Visual progress bar with FPS and ETA reporting

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
  -b, --bitrate <bitrate>  Video bitrate (default: 4000000)
  -p, --preset <preset>    Encoder preset (default: medium)
  --crf <value>            Constant Rate Factor (default: 23)
  --hw-accel <type>        Hardware acceleration (auto/cuda/vaapi/videotoolbox/none)
  -v, --verbose            Enable verbose logging
  -q, --quiet              Suppress all non-error output
  -h, --help               Show this help message

Examples:
  edl2ffmpeg input.json output.mp4
  edl2ffmpeg input.json output.mp4 --codec libx265 --crf 28
  edl2ffmpeg input.json output.mp4 -b 8000000 -p fast
  edl2ffmpeg input.json output.mp4 --hw-accel cuda    # Use NVIDIA hardware encoding
  edl2ffmpeg input.json output.mp4 --hw-accel none    # Force software encoding
```

## EDL Format

The EDL file should be in JSON format with the following structure:

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
				"out": 10,
				"width": 1920,
				"height": 1080,
				"fps": 30
			},
			"topFade": 1.0,
			"tailFade": 0.5
		},
		{
			"in": 0,
			"out": 10,
			"track": {
				"type": "video",
				"number": 1,
				"subtype": "transform"
			},
			"source": {
				"in": 0,
				"out": 10,
				"controlPoints": [
					{
						"point": 0,
						"panx": 0,
						"pany": 0,
						"zoomx": 1.0,
						"zoomy": 1.0,
						"rotate": 0,
						"shape": 1
					}
				]
			}
		}
	]
}
```

### EDL Properties

- `fps`: Frame rate of the output video (optional, must be evenly divisible into the quantum rate)
- `width`: Width of the output video in pixels (optional)
- `height`: Height of the output video in pixels (optional)
- `clips`: Array of clip objects

### Clip Properties

- `in`: Start time on timeline (seconds, required)
- `out`: End time on timeline (seconds, required)
- `track`: Track information (required)
  - `type`: Track type ("video", "audio", "subtitle", "burnin", "caption")
  - `number`: Track number (positive integer, 1-based)
  - `subtype`: Optional subtype ("effects", "transform", "colour", "level", "pan")
  - `subnumber`: Optional ordering for effects subtracks
- `source` or `sources`: Source definition (required, can be single object or array)
  - `in`: Start time in source (seconds)
  - `out`: End time in source (seconds)
  - One of:
    - `uri`: Path/URI to media file (relative paths resolved from EDL location)
    - `location`: Object with `id` and `type` for remote sources
    - `generate`: Object with `type` ("black", "colour", "demo", "testpattern")
  - Optional source properties:
    - `width`, `height`: Source dimensions
    - `fps`: Source frame rate
    - `speed`: Speed factor (> 0)
    - `gamma`: Gamma correction (video only)
    - `trackId`: Track within source ("V1", "A1", etc.)
    - `audiomix`: "avg" to mix all audio channels
    - `text`: Text content (for subtitle/burnin tracks)
- Optional clip properties:
  - `topFade`: Fade-in duration (seconds)
  - `tailFade`: Fade-out duration (seconds)
  - `topFadeYUV`: Fade-in color (6-digit hex YUV)
  - `tailFadeYUV`: Fade-out color (6-digit hex YUV)
  - `motion`: Motion control object
    - `offset`: Time offset
    - `duration`: Effect duration
    - `bezier`: Array of bezier control points
  - `transition`: Transition to next clip
    - `source` or `sources`: Source for transition
    - `type`: Transition type
    - `invert`: Boolean to invert transition
    - `points`: Number of points
    - `xsquares`: Grid squares
  - `channelMap`: Audio channel mapping
  - `textFormat`: Text formatting for subtitles/burnin
  - `sync`: Sync ID to link related clips

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

- 1080p30 video: 60+ fps processing (2x realtime) on CPU
- Hardware encoding: 200+ fps with NVENC/VideoToolbox
- Zero-copy GPU passthrough for frames without effects
- Memory usage: Under 500MB for typical operations
- Support for H.264, H.265, ProRes codecs

### Optimization Roadmap

- SIMD optimizations for effects (SSE4.2, AVX2, AVX-512)
- GPU acceleration for effects (OpenCL, CUDA, Vulkan)
- Multi-threaded pipeline architecture
- [x] Hardware encoder/decoder support (IMPLEMENTED)
- [x] Zero-copy GPU passthrough for unmodified frames (IMPLEMENTED)

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