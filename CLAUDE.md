# CLAUDE.md - edl2ffmpeg Project Context

## Project Overview

edl2ffmpeg is a high-performance EDL (Edit Decision List) renderer that processes JSON-formatted EDL files and outputs compressed video using direct FFmpeg library integration. This eliminates the performance overhead of pipe-based communication used in the original ftv_toffmpeg tool.

**GitHub Repository:** https://github.com/blackbird-aslough/edl2ffmpeg (private)

### Core Purpose
- Render EDL files to video with maximum performance
- Direct FFmpeg library integration (no pipes/subprocesses)
- Designed for open-sourcing with clean, extensible architecture
- Performance-focused with SIMD/GPU optimization readiness
- Uses publishing EDL format with 'uri' field for media references

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
   - `HardwareAcceleration`: Auto-detection and management of hardware encoders/decoders
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

# Test with hardware acceleration
./edl2ffmpeg input.json output.mp4 --hw-accel auto  # Auto-detect (default)
./edl2ffmpeg input.json output.mp4 --hw-accel cuda  # Force NVIDIA CUDA
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi # Force VAAPI (Linux)
./edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox # Force VideoToolbox (macOS)
./edl2ffmpeg input.json output.mp4 --hw-accel none  # Force software
```

## Performance Considerations

### Current Optimizations
- Frame buffer pooling reduces allocations
- Lazy instruction generation minimizes memory usage
- Direct FFmpeg library calls eliminate pipe overhead
- 32-byte memory alignment for SIMD readiness
- Smart seeking logic: only seeks when going backward or jumping >60 frames ahead
- Optimized for sequential frame access (minimal seeking overhead)
- Hardware acceleration auto-detection for encoding/decoding
- Zero-copy GPU passthrough for frames without effects
- Platform-specific encoder settings matching reference encoder (ftv_toffmpeg)

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
- [x] Hardware encoder/decoder support (VAAPI, NVDEC, VideoToolbox) - IMPLEMENTED

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

### Issue: Decoder errors when processing EDL
**Solution**: Ensure EDL frame rate matches source media frame rate. Frame rate conversion (e.g., 25fps to 30fps) can cause repeated seeks and decoder state issues.

### Issue: Build fails with FFmpeg errors
**Solution**: Check FFmpeg version and development headers are installed:
```bash
pkg-config --modversion libavcodec
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```
**Note**: The codebase supports FFmpeg 2.x, 3.x, 4.x, and 5.x with compatibility macros.

### Issue: Platform differences in output (Linux vs macOS)
**Solution**: This is intentional due to B-frame handling differences. macOS VideoToolbox has B-frames disabled to avoid PTS/DTS ordering issues. See `FFMPEG_PLATFORM_DIFFERENCES.md` for details.

### Issue: Hardware acceleration not working
**Solution**: Check hardware support with:
```bash
./edl2ffmpeg input.json output.mp4 -v  # Verbose mode shows HW detection
```
To force software encoding: `--hw-accel none`

### Issue: Crashes or hangs during cleanup with hardware acceleration
**Solution**: This is a known issue with FFmpeg hardware codec cleanup. See [Hardware Acceleration Cleanup Guide](docs/hardware-acceleration-cleanup.md) for the correct cleanup sequence. The key is calling `avcodec_close()` before `avcodec_free_context()` for hardware codecs.

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

## Nix Development Environment

This project uses Nix for reproducible development environments with lorri + direnv for automatic environment loading and caching.

### Environment Setup
- **lorri**: Provides cached nix-shell environments for faster loading
- **direnv**: Automatically loads the environment when entering the project directory
- **Configuration**: `.envrc` file with `use nix` directive

### Detecting Nix Environment
To check if you're in the Nix development environment:
```bash
# Check for IN_NIX_SHELL environment variable
if [[ -n "$IN_NIX_SHELL" ]]; then
    echo "In Nix shell"
fi

# Check if FFmpeg libraries are available
pkg-config --modversion libavcodec  # Should show version if in Nix shell
```

### Building in Nix Environment
When in the Nix environment (automatically loaded by direnv):
```bash
# Dependencies are already available, just build normally
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

If NOT in Nix environment:
```bash
# Option 1: Use nix-shell directly
nix-shell --run "./build-nix.sh"

# Option 2: Allow direnv and reload
direnv allow
direnv reload
# Then build normally

# Option 3: Use the build script which detects environment
./build-nix.sh
```

### Build Script Behavior
The `build-nix.sh` script automatically detects if it's running in a Nix environment:
- If IN_NIX_SHELL is set: Runs build commands directly
- If not in Nix shell: Wraps commands with `nix-shell --run`

### Common Nix Issues

**Issue**: "Package libavcodec was not found" when building
**Solution**: You're not in the Nix environment. Run `direnv allow` or use `nix-shell`

**Issue**: Slow environment loading
**Solution**: Ensure lorri daemon is running: `systemctl --user status lorri`

**Issue**: Environment not loading automatically
**Solution**: 
```bash
direnv allow  # Allow .envrc to be executed
direnv reload # Force reload the environment
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