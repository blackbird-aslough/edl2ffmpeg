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
			"source": {
				"uri": "video.mp4",
				"trackId": "V1",
				"in": 0,
				"out": 10
			},
			"in": 0,
			"out": 10,
			"track": {
				"type": "video",
				"number": 1
			},
			"topFade": 1.0,
			"tailFade": 0.5,
			"motion": {
				"panX": 0,
				"panY": 0,
				"zoomX": 1.0,
				"zoomY": 1.0,
				"rotation": 0
			},
			"transition": {
				"type": "dissolve",
				"duration": 1.0
			}
		}
	]
}
```

### EDL Properties

- `fps`: Frame rate of the output video
- `width`: Width of the output video in pixels
- `height`: Height of the output video in pixels
- `clips`: Array of clip objects

### Clip Properties

- `source`: Source media information
  - `uri`: Path to the media file (publishing EDL format)
  - `trackId`: Track identifier (e.g., "V1" for video track 1)
  - `in`: Start time in source (seconds)
  - `out`: End time in source (seconds)
- `in`: Start time on timeline (seconds)
- `out`: End time on timeline (seconds)
- `track`: Track information
  - `type`: Track type ("video", "audio", "subtitle", "caption")
  - `number`: Track number
- `topFade`: Fade-in duration (seconds)
- `tailFade`: Fade-out duration (seconds)
- `motion`: Transform parameters
  - `panX`, `panY`: Pan position (-1 to 1)
  - `zoomX`, `zoomY`: Zoom factors
  - `rotation`: Rotation in degrees
- `transition`: Transition to next clip
  - `type`: Transition type ("dissolve", "wipe", "slide")
  - `duration`: Transition duration (seconds)

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