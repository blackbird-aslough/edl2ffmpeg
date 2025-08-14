# GPU Acceleration Guide for edl2ffmpeg

## Overview

edl2ffmpeg supports hardware-accelerated video encoding and decoding on Linux (NVIDIA NVENC/NVDEC, Intel/AMD VAAPI) and macOS (VideoToolbox). GPU acceleration can provide significant performance improvements, especially for high-resolution content.

## Quick Start

### Basic GPU Usage

```bash
# Auto-detect and use best available hardware acceleration (default)
./edl2ffmpeg input.json output.mp4

# Force specific hardware acceleration
./edl2ffmpeg input.json output.mp4 --hw-accel cuda      # NVIDIA CUDA
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi     # Intel/AMD VAAPI
./edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox  # macOS VideoToolbox

# Disable hardware acceleration
./edl2ffmpeg input.json output.mp4 --hw-accel none
```

## Command-Line Options

- `--hw-accel <type>`: Hardware acceleration type
  - `auto`: Auto-detect best available (default)
  - `none`: Disable hardware acceleration
  - `cuda`: NVIDIA CUDA/NVENC
  - `vaapi`: Intel/AMD VAAPI
  - `videotoolbox`: macOS VideoToolbox

- `-v, --verbose`: Show which hardware acceleration is being used

**Note**: Hardware acceleration is automatically detected and used when available. The system will automatically fall back to software encoding if hardware acceleration fails.

## Performance Benchmarks

### Test Configuration
- Input: 1920x1080 @ 25fps, 300 seconds (7500 frames)
- Codec: H.264
- System: Linux with NVIDIA GPU

### Results

| Mode | Time | Real-time Factor | Notes |
|------|------|------------------|-------|
| CPU Only | 29.7s | 10.1x | 16 threads, ~250 FPS |
| GPU Encoding | 18.5s | 16.2x | NVENC, ~440 FPS |

**Performance Improvement: ~38% faster with GPU encoding**

### Expected Improvements

GPU acceleration provides even better results with:
- Higher resolutions (4K, 8K)
- Multiple streams
- Real-time encoding requirements
- Complex encoding parameters

## Platform-Specific Setup

### Linux - NVIDIA

Requirements:
- NVIDIA GPU with NVENC support (GTX 600 series or newer)
- NVIDIA drivers installed
- CUDA toolkit (optional, for maximum performance)

```bash
# Check if NVIDIA GPU is available
nvidia-smi

# Use CUDA/NVENC
./edl2ffmpeg input.json output.mp4 --hw-accel cuda
```

### Linux - Intel/AMD

Requirements:
- Intel GPU (Intel HD Graphics or newer) or AMD GPU
- VA-API drivers installed

```bash
# Install VA-API drivers (Ubuntu/Debian)
sudo apt-get install vainfo intel-media-va-driver

# Check VA-API support
vainfo

# Use VAAPI
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi
```

### macOS

Requirements:
- macOS 10.13 or newer
- Apple Silicon or Intel Mac with supported GPU

```bash
# VideoToolbox is built-in on macOS
./edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox
```

## Troubleshooting

### Hardware not detected

```bash
# Run with verbose mode to see detection
./edl2ffmpeg input.json output.mp4 -v
```

### Decoder errors

Some formats may not support hardware decoding. The system will automatically fall back to software decoding when necessary.

### Performance not improved

Ensure:
1. GPU drivers are properly installed
2. GPU supports the codec (H.264/H.265)
3. Not using complex filters that require CPU processing

### Multi-GPU systems

The system will automatically select the first available GPU. For manual selection in multi-GPU systems, use environment variables:

```bash
# NVIDIA: Use second GPU
CUDA_VISIBLE_DEVICES=1 ./edl2ffmpeg input.json output.mp4 --hw-accel cuda
```

## Building with GPU Support

### Nix Environment

```bash
# With CUDA support (NVIDIA)
export NIXPKGS_ALLOW_UNFREE=1
nix-shell

# Build with all GPU libraries
nix build
```

### CMake Options

```bash
cmake .. -DENABLE_GPU=ON
```

## Supported Codecs

### NVIDIA NVENC
- H.264 (h264_nvenc)
- H.265/HEVC (hevc_nvenc)
- AV1 (av1_nvenc) - RTX 40 series only

### Intel/AMD VAAPI
- H.264 (h264_vaapi)
- H.265/HEVC (hevc_vaapi)
- VP8 (vp8_vaapi)
- VP9 (vp9_vaapi)
- AV1 (av1_vaapi) - newer GPUs

### macOS VideoToolbox
- H.264 (h264_videotoolbox)
- H.265/HEVC (hevc_videotoolbox)

## Best Practices

1. **Always test**: GPU acceleration behavior varies by hardware and driver version

2. **Monitor GPU usage**: Use `nvidia-smi`, `intel_gpu_top`, or Activity Monitor

3. **Fallback strategy**: Always have CPU encoding as fallback for compatibility

4. **Quality settings**: GPU encoders may have different quality characteristics than CPU encoders

5. **Power efficiency**: GPU encoding typically uses less power than CPU encoding

## Implementation Details

### Zero-Copy GPU Passthrough

When using hardware acceleration and no effects are applied to a frame, edl2ffmpeg implements zero-copy passthrough, keeping frames in GPU memory throughout the pipeline. This provides maximum performance for simple cuts and concatenations.

### Platform-Specific Optimizations

**macOS VideoToolbox**: B-frames are automatically disabled due to PTS/DTS ordering issues with VideoToolbox. This ensures reliable encoding at the cost of slightly larger file sizes.

**NVIDIA CUDA**: Supports both NVENC for encoding and NVDEC for decoding with automatic fallback to software when needed.

## Future Enhancements

- GPU-accelerated filters and effects
- Multi-GPU parallel encoding
- Hardware frame upload/download optimization
- Dynamic GPU selection based on load

## See Also

- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
- [Intel Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)
- [VideoToolbox Documentation](https://developer.apple.com/documentation/videotoolbox)