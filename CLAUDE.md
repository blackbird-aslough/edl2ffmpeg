# CLAUDE.md - edl2ffmpeg Project Context

## Project Overview

edl2ffmpeg is a high-performance EDL (Edit Decision List) renderer that processes JSON-formatted EDL files and outputs compressed video using direct FFmpeg library integration. This eliminates the performance overhead of pipe-based communication used in the original ftv_toffmpeg tool.

**GitHub Repository:** https://github.com/blackbird-aslough/edl2ffmpeg (private)

### Core Purpose
- Render EDL files to video with maximum performance
- Direct FFmpeg library integration (no pipes/subprocesses)
- Designed for open-sourcing with clean, extensible architecture
- Performance-focused with SIMD/GPU optimization readiness

## Architecture

### Pipeline Flow
```
EDL File → Parser → Instruction Generator → Frame Loop → Output Video
                                              ↑      ↓
                                          Decoder  Compositor
                                                     ↓
                                                  Encoder
```

### Key Components

1. **EDL Parser** (`src/edl/`)
   - Parses JSON EDL files into internal C++ structures
   - Handles clips, tracks, sources, transitions, effects
   - Uses nlohmann/json library

2. **Instruction Generator** (`src/compositor/InstructionGenerator`)
   - Transforms EDL timeline into per-frame compositor instructions
   - Lazy evaluation via iterator pattern
   - Handles frame number to timecode conversions

3. **FFmpeg Wrappers** (`src/media/`)
   - `FFmpegDecoder`: Frame-accurate seeking and decoding
   - `FFmpegEncoder`: Configurable encoding with multiple codecs
   - RAII pattern for resource management

4. **Frame Compositor** (`src/compositor/FrameCompositor`)
   - Processes frames according to instructions
   - Currently implements: fade, brightness, contrast
   - Prepared for SIMD optimizations

5. **Memory Management** (`src/utils/FrameBuffer`)
   - Frame buffer pooling to minimize allocations
   - 32-byte alignment for SIMD operations
   - Thread-safe pool management

## Code Conventions

### C++ Style
- **Standard**: C++20
- **Namespaces**: `edl::`, `media::`, `compositor::`, `utils::`
- **Smart Pointers**: Use `std::shared_ptr<AVFrame>` with custom deleters
- **RAII**: All FFmpeg resources managed via RAII
- **Error Handling**: Exception-based with descriptive messages

### File Organization
```
src/
├── edl/           # EDL parsing and data structures
├── compositor/    # Frame composition and effects  
├── media/         # FFmpeg encoder/decoder wrappers
└── utils/         # Logging and memory management
```

### Formatting
- **Indentation**: Tabs (not spaces)
- **Braces**: Opening brace on same line
- **Line Length**: Prefer under 100 characters
- **Comments**: Minimal, code should be self-documenting

## Common Development Tasks

### Adding a New Effect

1. Add effect type to `CompositorInstruction.h`:
```cpp
struct Effect {
    enum Type {
        // ... existing effects ...
        YourNewEffect
    };
};
```

2. Implement in `FrameCompositor.cpp`:
```cpp
void FrameCompositor::applyYourEffect(AVFrame* frame, float strength) {
    // Effect implementation
    // Consider YUV color space
    // Prepare for SIMD optimization
}
```

3. Add to effect processing switch statement

### Adding Codec Support

Modify `FFmpegEncoder::setupEncoder()` to handle new codec:
```cpp
if (config.codec == "your_codec") {
    // Set codec-specific options
    av_opt_set(codecCtx->priv_data, "option", "value", 0);
}
```

### Implementing SIMD Optimizations

1. Check CPU capabilities at runtime
2. Create SIMD-optimized function variants
3. Use function pointers to select implementation
4. Ensure 32-byte alignment (already handled by FrameBufferPool)

Example structure:
```cpp
void applyEffect_scalar(AVFrame* frame);
void applyEffect_sse4(AVFrame* frame);  
void applyEffect_avx2(AVFrame* frame);
void applyEffect_avx512(AVFrame* frame);

// Select at runtime based on CPU
effectFunc = detectCPU() ? applyEffect_avx2 : applyEffect_scalar;
```

## Testing

### Running Tests
```bash
cd build
ctest
# Or directly:
./tests/test_edl_parser
```

### Adding Tests
- EDL parser tests go in `tests/test_edl_parser.cpp`
- Add sample EDL files to `tests/sample_edls/`
- Use assertions for validation

### Manual Testing
```bash
# Simple test with generated color bars
ffmpeg -f lavfi -i testsrc=duration=10:size=1920x1080:rate=30 test_video.mp4
./edl2ffmpeg tests/sample_edls/simple_single_clip.json output.mp4

# Test with verbose output
./edl2ffmpeg input.json output.mp4 -v

# Test different codec
./edl2ffmpeg input.json output.mp4 --codec libx265 --crf 28
```

## Performance Considerations

### Current Optimizations
- Frame buffer pooling reduces allocations
- Lazy instruction generation minimizes memory usage
- Direct FFmpeg library calls eliminate pipe overhead
- 32-byte memory alignment for SIMD readiness

### Profiling
```bash
# Build with profiling
cmake .. -DPROFILE_BUILD=ON
make

# Run with profiling
./edl2ffmpeg input.json output.mp4
gprof edl2ffmpeg gmon.out > profile.txt
```

### Bottlenecks to Address
1. **Decoding**: Consider parallel decode threads
2. **Color Conversion**: Prime candidate for SIMD
3. **Scaling**: Use hardware acceleration when available
4. **Effects**: Implement SIMD variants

## Areas for Improvement

### Phase 2 Features (Ready to Implement)
- [ ] SIMD optimizations for effects (SSE4.2, AVX2, AVX-512)
- [ ] Geometric transforms (pan, zoom, rotation)
- [ ] Multi-threaded pipeline (decode/process/encode)
- [ ] Hardware decoder support (VAAPI, NVDEC, VideoToolbox)

### Phase 3 Features (Future)
- [ ] GPU acceleration (OpenCL, CUDA, Vulkan compute)
- [ ] Multiple video track compositing
- [ ] Audio track support with mixing
- [ ] Advanced transitions (wipes, slides)
- [ ] Real-time preview mode

### Code Quality Improvements
- [ ] Add comprehensive error handling for all FFmpeg calls
- [ ] Implement progress callback mechanism for GUI integration
- [ ] Add configuration file support
- [ ] Create benchmark suite

## Important Notes

### Frame Accurate Seeking
FFmpeg seeking can be imprecise. The decoder implements:
1. Seek backwards to nearest keyframe
2. Decode forward to target frame
3. Cache decoded frames when possible

### Color Space Handling
- Currently assumes YUV420P for output
- Input can be any format (auto-converted)
- Effects must handle YUV color space correctly
- Future: Support multiple output formats

### Memory Alignment
- All frame buffers are 32-byte aligned
- Required for SIMD operations
- Handled automatically by FrameBufferPool

### Thread Safety
- FrameBufferPool is thread-safe
- Decoders/encoders are NOT thread-safe (one per thread)
- Logger is thread-safe

## Common Issues and Solutions

### Issue: "Codec not found"
**Solution**: Ensure FFmpeg is built with required codec support
```bash
ffmpeg -codecs | grep libx264
```

### Issue: Memory usage grows over time
**Solution**: Check frame pool is returning frames properly. Increase pool size if needed.

### Issue: Seeking produces wrong frames
**Solution**: This is often due to variable frame rate media. Ensure consistent frame rate in source media.

### Issue: Build fails with FFmpeg errors
**Solution**: Check FFmpeg version (4.4+ required) and development headers are installed:
```bash
pkg-config --modversion libavcodec
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

## Development Workflow

### Before Making Changes
1. Understand the pipeline flow
2. Check if similar functionality exists
3. Consider performance implications
4. Plan for future SIMD/GPU optimization

### When Adding Features
1. Follow existing patterns and conventions
2. Update relevant documentation
3. Add tests for new functionality
4. Profile performance impact

### Code Review Checklist
- [ ] No memory leaks (test with Valgrind)
- [ ] Proper error handling
- [ ] RAII for all resources
- [ ] Performance considerations addressed
- [ ] Tests added/updated
- [ ] Documentation updated

## Build Configurations

### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
# Includes debug symbols, assertions enabled
```

### Release Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make
# Full optimizations, assertions disabled
```

### Custom Optimizations
```bash
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
make
# Maximum optimization for local CPU
```

## Contact and Resources

### Related Projects
- Original ftv_toffmpeg tool (internal)
- videolib framework (for compositor instruction format reference)

### FFmpeg Documentation
- [FFmpeg Doxygen](https://ffmpeg.org/doxygen/trunk/)
- [FFmpeg Wiki](https://trac.ffmpeg.org/wiki)

### Performance Resources
- [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/)
- [Agner Fog's Optimization Manuals](https://www.agner.org/optimize/)

## Quick Command Reference

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Test
ctest
./edl2ffmpeg tests/sample_edls/simple_single_clip.json test.mp4

# Profile
valgrind --leak-check=full ./edl2ffmpeg input.json output.mp4
perf record -g ./edl2ffmpeg input.json output.mp4
perf report

# Debug
gdb ./edl2ffmpeg
lldb ./edl2ffmpeg

# Clean rebuild
rm -rf build
mkdir build && cd build
cmake .. && make -j$(nproc)
```

---

Remember: This is a performance-critical application. Always consider the performance implications of changes and prepare code for future SIMD/GPU optimizations.