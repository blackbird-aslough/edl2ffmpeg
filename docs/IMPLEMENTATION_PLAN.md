# edl2ffmpeg Implementation Plan

## Project Overview

### Goals
Build a high-performance, open-source tool to render EDL (Edit Decision List) files and output compressed video using FFmpeg libraries directly, eliminating the performance overhead of pipe-based communication.

### Key Objectives
- **Direct FFmpeg Integration**: Link directly with FFmpeg libraries for maximum performance
- **Clean Architecture**: Implement a clean, extensible design suitable for open-sourcing
- **Performance Focus**: SIMD and GPU optimizations for all effects and transforms
- **Lazy Evaluation**: Transform EDL into lazily-evaluated compositor instructions
- **Compatibility**: Match the output quality and features of existing ftv_toffmpeg tool

### Design Principles
- Zero-copy operations where possible
- Memory pool for frame buffers to minimize allocations
- Pipeline architecture for decode/process/encode stages
- Abstracted effect system for easy SIMD/GPU implementation
- C++20 modern features for clean, efficient code

## Architecture Design

### System Components

```
┌─────────────┐     ┌──────────────┐     ┌────────────────────┐
│  EDL File   │────▶│  EDL Parser  │────▶│ Instruction        │
└─────────────┘     └──────────────┘     │ Generator          │
                                          └────────────────────┘
                                                    │
                                                    ▼
┌─────────────┐     ┌──────────────┐     ┌────────────────────┐
│ Media Files │────▶│   FFmpeg     │────▶│ Frame Compositor   │
└─────────────┘     │   Decoder    │     └────────────────────┘
                    └──────────────┘               │
                                                    ▼
                                          ┌────────────────────┐
                                          │ FFmpeg Encoder     │
                                          └────────────────────┘
                                                    │
                                                    ▼
                                          ┌────────────────────┐
                                          │ Output Video File  │
                                          └────────────────────┘
```

### Data Flow Pipeline

1. **EDL Parsing**: Read and parse JSON EDL file into internal structures
2. **Instruction Generation**: Transform EDL timeline into per-frame compositor instructions
3. **Frame Processing Loop**:
   - Pull next instruction from generator
   - Decode required source frame(s) from media
   - Apply transforms and effects
   - Encode output frame

### Compositor Instruction Format

Following the videolib external compositor format pattern:

```cpp
struct CompositorInstruction {
	enum Type { DrawFrame, GenerateColor, NoOp, Transition };
	Type type;
	
	// Source information
	int trackNumber;
	std::string mediaId;
	int64_t sourceFrameNumber;
	
	// Transform parameters
	float panX, panY;      // -1 to 1
	float zoomX, zoomY;    // zoom factors
	float rotation;        // degrees
	bool flip;
	
	// Effects
	float fade;            // 0-1
	std::vector<Effect> effects;
	
	// Transition (if applicable)
	TransitionInfo transition;
	float transitionProgress;
};
```

## Phase 1: Minimal Implementation

### Core Components Required

1. **Build System (CMake)**
   - C++20 standard requirement
   - FFmpeg library detection and linking
   - Optimization flags (-O3, -march=native for development)
   - SIMD support detection

2. **Project Structure**
```
edl2ffmpeg/
├── CMakeLists.txt
├── README.md
├── IMPLEMENTATION_PLAN.md
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── edl/
│   │   ├── EDLParser.h             # EDL JSON parsing
│   │   ├── EDLParser.cpp
│   │   └── EDLTypes.h              # EDL data structures
│   ├── compositor/
│   │   ├── CompositorInstruction.h # Instruction definitions
│   │   ├── InstructionGenerator.h  # EDL to instruction conversion
│   │   ├── InstructionGenerator.cpp
│   │   ├── FrameCompositor.h       # Frame processing
│   │   └── FrameCompositor.cpp
│   ├── media/
│   │   ├── FFmpegDecoder.h         # Media decoding wrapper
│   │   ├── FFmpegDecoder.cpp
│   │   ├── FFmpegEncoder.h         # Media encoding wrapper
│   │   ├── FFmpegEncoder.cpp
│   │   └── MediaTypes.h            # Common media structures
│   └── utils/
│       ├── Logger.h                # Logging utilities
│       ├── Logger.cpp
│       ├── FrameBuffer.h           # Frame memory management
│       └── FrameBuffer.cpp
├── tests/
│   ├── test_edl_parser.cpp
│   ├── test_decoder.cpp
│   └── sample_edls/
│       └── simple_single_clip.json
└── docs/
    └── performance_notes.md
```

### Minimal Feature Set Implementation

#### Stage 1: EDL Parser
```cpp
class EDLParser {
public:
	struct EDL {
		int fps;
		int width;
		int height;
		std::vector<Clip> clips;
	};
	
	struct Clip {
		double in, out;          // Timeline position
		Track track;
		Source source;
	};
	
	static EDL parse(const std::string& filename);
	static EDL parseJSON(const nlohmann::json& j);
};
```

#### Stage 2: FFmpeg Decoder
```cpp
class FFmpegDecoder {
private:
	AVFormatContext* formatCtx;
	AVCodecContext* codecCtx;
	int videoStreamIndex;
	FrameBufferPool framePool;
	
public:
	FFmpegDecoder(const std::string& filename);
	~FFmpegDecoder();
	
	// Seek to specific frame number
	bool seekToFrame(int64_t frameNumber);
	
	// Get decoded frame (from pool)
	std::shared_ptr<AVFrame> getFrame(int64_t frameNumber);
	
	// Media properties
	int getWidth() const;
	int getHeight() const;
	AVPixelFormat getPixelFormat() const;
	AVRational getFrameRate() const;
};
```

#### Stage 3: FFmpeg Encoder
```cpp
class FFmpegEncoder {
private:
	AVFormatContext* formatCtx;
	AVCodecContext* codecCtx;
	AVStream* videoStream;
	int64_t frameCount;
	
public:
	struct Config {
		std::string codec = "libx264";
		int bitrate = 4000000;  // 4Mbps
		AVPixelFormat pixelFormat = AV_PIX_FMT_YUV420P;
		int width, height;
		AVRational frameRate;
	};
	
	FFmpegEncoder(const std::string& filename, const Config& config);
	~FFmpegEncoder();
	
	bool writeFrame(AVFrame* frame);
	bool finalize();
};
```

#### Stage 4: Instruction Generator
```cpp
class InstructionGenerator {
private:
	EDLParser::EDL edl;
	int currentFrame;
	int totalFrames;
	
public:
	InstructionGenerator(const EDLParser::EDL& edl);
	
	// Iterator interface for lazy evaluation
	class Iterator {
		CompositorInstruction current;
		int frameNumber;
		
	public:
		CompositorInstruction operator*() const;
		Iterator& operator++();
		bool operator!=(const Iterator& other) const;
	};
	
	Iterator begin();
	Iterator end();
	
	// Direct access
	CompositorInstruction getInstructionForFrame(int frameNumber);
};
```

#### Stage 5: Frame Compositor
```cpp
class FrameCompositor {
private:
	FrameBufferPool outputPool;
	// Future: effect processors, SIMD functions
	
public:
	FrameCompositor(int width, int height, AVPixelFormat format);
	
	// Process single frame with instruction
	std::shared_ptr<AVFrame> processFrame(
		const std::shared_ptr<AVFrame>& input,
		const CompositorInstruction& instruction
	);
	
	// For initial implementation - just pass through
	// Later: implement transforms, effects, etc.
};
```

#### Stage 6: Main Application
```cpp
int main(int argc, char* argv[]) {
	// Parse command line arguments
	std::string edlFile = argv[1];
	std::string outputFile = argv[2];
	
	// Parse EDL
	auto edl = EDLParser::parse(edlFile);
	
	// Initialize components
	std::unordered_map<std::string, std::unique_ptr<FFmpegDecoder>> decoders;
	for (const auto& clip : edl.clips) {
		if (decoders.find(clip.source.mediaId) == decoders.end()) {
			decoders[clip.source.mediaId] = 
				std::make_unique<FFmpegDecoder>(getMediaPath(clip.source.mediaId));
		}
	}
	
	FFmpegEncoder::Config encoderConfig;
	encoderConfig.width = edl.width;
	encoderConfig.height = edl.height;
	encoderConfig.frameRate = {edl.fps, 1};
	FFmpegEncoder encoder(outputFile, encoderConfig);
	
	FrameCompositor compositor(edl.width, edl.height, AV_PIX_FMT_YUV420P);
	InstructionGenerator generator(edl);
	
	// Process frames
	for (const auto& instruction : generator) {
		auto& decoder = decoders[instruction.mediaId];
		auto inputFrame = decoder->getFrame(instruction.sourceFrameNumber);
		auto outputFrame = compositor.processFrame(inputFrame, instruction);
		encoder.writeFrame(outputFrame.get());
	}
	
	encoder.finalize();
	return 0;
}
```

## Phase 2: Performance Optimizations

### Memory Management
- **Frame Buffer Pool**: Pre-allocate AVFrames to avoid allocation overhead
- **Zero-Copy Pipeline**: Pass frame pointers through pipeline, avoid copying pixel data
- **Alignment**: Ensure buffers are aligned for SIMD operations (32-byte for AVX)

### Threading Architecture
```cpp
class Pipeline {
	// Decode thread
	std::thread decodeThread;
	BlockingQueue<DecodedFrame> decodeQueue;
	
	// Process thread(s)
	std::vector<std::thread> processThreads;
	BlockingQueue<ProcessedFrame> processQueue;
	
	// Encode thread
	std::thread encodeThread;
	
	// Coordination
	std::atomic<bool> running;
	std::condition_variable frameAvailable;
};
```

### SIMD Optimization Preparation
- Abstract pixel operations into function pointers
- Runtime CPU feature detection
- Prepare for SSE4.2, AVX2, AVX-512 implementations
- Example: YUV to RGB conversion, scaling, blending

## Phase 3: Future Expansions

### Effects System
```cpp
class Effect {
public:
	virtual void apply(AVFrame* frame) = 0;
	virtual bool canUseSIMD() const { return false; }
	virtual bool canUseGPU() const { return false; }
};

class BrightnessEffect : public Effect {
	float factor;
public:
	void apply(AVFrame* frame) override;
	void applySIMD(AVFrame* frame);  // AVX2 implementation
	void applyGPU(AVFrame* frame);   // OpenCL/CUDA
};
```

### Transform System
- 2D affine transformations (pan, zoom, rotate)
- Bicubic/Lanczos interpolation for scaling
- SIMD-accelerated transform matrices

### GPU Acceleration
- OpenCL for cross-platform GPU support
- CUDA for NVIDIA GPUs
- Vulkan compute shaders for modern GPUs
- GPU frame buffer management

### Additional Track Support
- Multiple video tracks with compositing
- Audio track support with mixing
- Subtitle/caption rendering
- Effects tracks

## Technical Specifications

### EDL Data Structures
```cpp
namespace edl {
	struct Source {
		std::string mediaId;
		std::string trackId;  // "V1", "A1", etc.
		double in, out;        // Source timecode
		
		// Optional
		int width, height;
		int fps;
		float rotation;
		bool flip;
	};
	
	struct Track {
		enum Type { Video, Audio, Subtitle, Caption };
		Type type;
		int number;
		std::string subtype;  // "transform", "effects", etc.
		int subnumber;
	};
	
	struct Clip {
		double in, out;        // Timeline position
		Track track;
		Source source;
		
		// Optional
		float topFade, tailFade;
		Motion motion;
		Transition transition;
	};
	
	struct EDL {
		int fps;
		int width, height;
		std::vector<Clip> clips;
	};
}
```

### Memory Management Strategy
- Use `std::shared_ptr<AVFrame>` with custom deleter for automatic cleanup
- Frame buffer pools with thread-local storage for multi-threading
- Lazy allocation - only allocate when needed
- Memory-mapped file I/O for large media files

### Error Handling
- RAII for all FFmpeg resources
- Exception-based error handling with specific exception types
- Graceful degradation (fallback to CPU if GPU fails)
- Comprehensive logging with levels (ERROR, WARN, INFO, DEBUG)

## Build and Testing

### Dependencies
- **FFmpeg** (4.4+ recommended)
  - libavcodec
  - libavformat
  - libavutil
  - libswscale
  - libswresample (for audio)
- **JSON Library**: nlohmann/json or RapidJSON
- **CMake** 3.16+
- **C++20 Compiler** (GCC 10+, Clang 12+, MSVC 2019+)

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

# Run tests
ctest

# Install (optional)
sudo make install
```

### CMake Configuration Options
```cmake
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENABLE_GPU "Enable GPU acceleration" OFF)
option(USE_SYSTEM_FFMPEG "Use system FFmpeg instead of building" ON)
option(PROFILE_BUILD "Enable profiling instrumentation" OFF)
```

### Test Strategy

1. **Unit Tests**
   - EDL parser with various JSON inputs
   - Instruction generator correctness
   - Frame buffer pool thread safety
   - Effect application correctness

2. **Integration Tests**
   - End-to-end with simple EDL files
   - Compare output with reference frames
   - Performance benchmarks
   - Memory leak detection with Valgrind

3. **Test EDL Files**
```json
{
	"fps": 30,
	"width": 1920,
	"height": 1080,
	"clips": [
		{
			"source": {
				"mediaId": "test_video.mp4",
				"trackId": "V1",
				"in": 0,
				"out": 10
			},
			"in": 0,
			"out": 10,
			"track": {"type": "video", "number": 1}
		}
	]
}
```

4. **Performance Testing**
   - Measure frames per second processing rate
   - Memory usage profiling
   - CPU usage per thread
   - Compare with pipe-based ftv_toffmpeg

### Continuous Integration
- GitHub Actions for Linux/macOS/Windows builds
- Automated testing on each commit
- Performance regression detection
- Code coverage reporting

## Performance Targets

### Initial Goals (Phase 1)
- Process 1080p30 video at minimum 60 fps (2x realtime)
- Memory usage under 500MB for typical operations
- Support for common codecs (H.264, H.265, ProRes)

### Optimized Goals (Phase 2+)
- Process 4K60 video at realtime or better
- Multi-track compositing at 30+ fps for 1080p
- GPU-accelerated effects at 100+ fps for 1080p
- Memory usage scaling: ~100MB base + (width × height × 4 × buffer_count)

## Development Timeline

### Month 1
- Week 1-2: Build system setup, FFmpeg integration
- Week 3: EDL parser, basic data structures
- Week 4: FFmpeg decoder/encoder wrappers

### Month 2
- Week 1: Instruction generator
- Week 2: Frame compositor (pass-through)
- Week 3: Main application, command-line interface
- Week 4: Testing, bug fixes, optimization

### Month 3+
- SIMD optimizations
- Effects implementation
- GPU acceleration research
- Multi-track support
- Performance tuning

## Notes and Considerations

1. **Color Space Handling**: Ensure proper color space conversions between different standards (BT.709, BT.601, BT.2020)

2. **Frame Accurate Seeking**: FFmpeg seeking can be imprecise; implement frame-accurate seeking with keyframe index

3. **Memory Alignment**: Ensure all pixel buffers are properly aligned for SIMD operations

4. **Error Recovery**: Implement graceful handling of corrupted frames, missing media files

5. **Progress Reporting**: Add progress callback mechanism for UI integration

6. **Configuration Files**: Support for preset configurations similar to ftv_toffmpeg's format options

7. **Licensing**: Ensure all dependencies are compatible with intended open-source license (likely MIT or Apache 2.0)

8. **Platform Support**: Design with cross-platform compatibility in mind (Linux, macOS, Windows)

This plan provides a solid foundation for building a high-performance, extensible EDL rendering system that can match and exceed the capabilities of existing tools while being suitable for open-source release.