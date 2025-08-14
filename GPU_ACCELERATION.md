# GPU Acceleration Guide for edl2ffmpeg

## Overview

edl2ffmpeg supports hardware-accelerated video encoding and decoding on Linux (NVIDIA NVENC/NVDEC, Intel/AMD VAAPI) and macOS (VideoToolbox). GPU acceleration can provide significant performance improvements, especially for high-resolution content.

## Quick Start

### Basic GPU Usage

```bash
# Auto-detect and use best available GPU
./edl2ffmpeg input.json output.mp4 --hw-accel auto --hw-encode

# Use specific GPU acceleration
./edl2ffmpeg input.json output.mp4 --hw-accel nvenc --hw-encode   # NVIDIA
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi --hw-encode   # Intel/AMD
./edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox --hw-encode  # macOS

# Enable both hardware encoding and decoding
./edl2ffmpeg input.json output.mp4 --hw-accel auto --hw-encode --hw-decode
```

## Command-Line Options

- `--hw-accel <type>`: Hardware acceleration type
  - `auto`: Auto-detect best available (default)
  - `none`: Disable hardware acceleration
  - `nvenc`: NVIDIA NVENC/NVDEC
  - `vaapi`: Intel/AMD VAAPI
  - `videotoolbox`: macOS VideoToolbox

- `--hw-device <index>`: GPU device index for multi-GPU systems (default: 0)

- `--hw-encode`: Enable hardware encoding

- `--hw-decode`: Enable hardware decoding

- `-v, --verbose`: Show which hardware acceleration is being used

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

# Use NVENC
./edl2ffmpeg input.json output.mp4 --hw-accel nvenc --hw-encode
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
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi --hw-encode
```

### macOS

Requirements:
- macOS 10.13 or newer
- Apple Silicon or Intel Mac with supported GPU

```bash
# VideoToolbox is built-in on macOS
./edl2ffmpeg input.json output.mp4 --hw-accel videotoolbox --hw-encode
```

## Troubleshooting

### Hardware not detected

```bash
# Run with verbose mode to see detection
./edl2ffmpeg input.json output.mp4 --hw-accel auto --hw-encode -v
```

### Decoder errors

Some formats may not support hardware decoding. Try encoding-only:

```bash
# Use hardware encoding only
./edl2ffmpeg input.json output.mp4 --hw-accel auto --hw-encode
```

### Performance not improved

Ensure:
1. GPU drivers are properly installed
2. GPU supports the codec (H.264/H.265)
3. Not using complex filters that require CPU processing

### Multi-GPU systems

```bash
# Use second GPU (index 1)
./edl2ffmpeg input.json output.mp4 --hw-accel nvenc --hw-device 1 --hw-encode
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

## Future Enhancements

- GPU-accelerated filters and effects
- Multi-GPU parallel encoding
- Hardware frame upload/download optimization
- Dynamic GPU selection based on load

## See Also

- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
- [Intel Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)
- [VideoToolbox Documentation](https://developer.apple.com/documentation/videotoolbox)