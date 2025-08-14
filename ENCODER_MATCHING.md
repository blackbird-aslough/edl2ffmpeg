# Encoder Settings Matching - edl2ffmpeg

## Overview

The edl2ffmpeg encoder has been configured to match the output characteristics of the reference encoder (ftv_toffmpeg) as closely as possible. This ensures consistent output quality and compatibility when migrating from the legacy tool.

## Matched Settings

### Core Encoder Parameters

The following settings have been aligned with ftv_toffmpeg:

```cpp
// Pixel format
codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

// GOP settings
codecCtx->gop_size = 250;        // Keyframe interval
codecCtx->keyint_min = 25;       // Minimum keyframe interval

// Rate control
codecCtx->bit_rate = config.bitrate;  // Default: 4000000
codecCtx->rc_buffer_size = config.bitrate * 2;
codecCtx->rc_max_rate = config.bitrate * 1.5;

// Frame settings
codecCtx->time_base = {1, 1000};  // Millisecond precision
codecCtx->framerate = {config.fps, 1};

// Quality settings
if (config.crf >= 0) {
    av_opt_set_int(codecCtx->priv_data, "crf", config.crf, 0);
}
```

### Codec-Specific Settings

#### H.264 (libx264)
```cpp
// B-frame settings
codecCtx->max_b_frames = 2;       // Bidirectional frames
codecCtx->b_frame_strategy = 1;   // Adaptive B-frame placement

// x264 specific options
av_opt_set(codecCtx->priv_data, "preset", config.preset.c_str(), 0);
av_opt_set(codecCtx->priv_data, "tune", "film", 0);
av_opt_set(codecCtx->priv_data, "profile", "high", 0);
av_opt_set(codecCtx->priv_data, "level", "4.0", 0);
```

#### H.265 (libx265)
```cpp
// x265 specific options
av_opt_set(codecCtx->priv_data, "preset", config.preset.c_str(), 0);
av_opt_set(codecCtx->priv_data, "tune", "grain", 0);
```

#### ProRes
```cpp
// ProRes profile selection
av_opt_set(codecCtx->priv_data, "profile", "hq", 0);  // High Quality
```

## Platform-Specific Differences

### macOS VideoToolbox

VideoToolbox encoders have B-frames disabled to avoid PTS/DTS ordering issues:

```cpp
if (codecName.find("videotoolbox") != std::string::npos) {
    codecCtx->max_b_frames = 0;  // No B-frames for VideoToolbox
}
```

This results in:
- Slightly larger file sizes (~35% increase)
- More reliable encoding
- Better compatibility with Apple ecosystem

### Hardware Encoders

Hardware encoders (NVENC, VAAPI) may have different default settings but attempt to match quality:

```cpp
// NVENC example
if (codecName.find("nvenc") != std::string::npos) {
    av_opt_set(codecCtx->priv_data, "preset", "p4", 0);     // Balanced preset
    av_opt_set(codecCtx->priv_data, "rc", "vbr", 0);        // Variable bitrate
    av_opt_set(codecCtx->priv_data, "cq", "23", 0);         // Quality level
}
```

## Legacy FFmpeg Compatibility

The encoder supports FFmpeg versions 2.x through 5.x with compatibility macros:

```cpp
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 64, 0)
    // FFmpeg 2.x/3.x compatibility
    codecCtx->refcounted_frames = 1;
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 0, 0)
    // Modern packet handling
    AVPacket* packet = av_packet_alloc();
#else
    // Legacy packet handling
    AVPacket packet;
    av_init_packet(&packet);
#endif
```

## Verification

### Comparing Output

To verify encoder matching:

```bash
# Encode with edl2ffmpeg
./edl2ffmpeg input.json output_new.mp4

# Encode with ftv_toffmpeg (reference)
ftv_toffmpeg input.json output_ref.mp4

# Compare key parameters
ffprobe -v error -show_streams output_new.mp4 > new.txt
ffprobe -v error -show_streams output_ref.mp4 > ref.txt
diff new.txt ref.txt
```

### Key Metrics to Check

1. **Bitrate**: Should be within 5% of target
2. **GOP Size**: Should match (250 frames)
3. **Frame Count**: Should be identical
4. **Duration**: Should match to millisecond precision
5. **Codec Profile**: Should be "High" for H.264

### Expected Differences

Some differences are expected and acceptable:

1. **Encoder Version**: String may differ
2. **Encoding Time**: edl2ffmpeg should be faster
3. **File Size**: Minor variations (< 5%) are normal
4. **B-frames**: Platform-dependent (see above)

## Troubleshooting

### Output Doesn't Match

If output differs significantly:

1. **Check FFmpeg Version**:
   ```bash
   ffmpeg -version
   ```
   Ensure you're using a compatible version.

2. **Verify Codec Support**:
   ```bash
   ffmpeg -codecs | grep h264
   ```
   Ensure required codecs are available.

3. **Enable Verbose Mode**:
   ```bash
   ./edl2ffmpeg input.json output.mp4 -v
   ```
   Check encoder settings in log output.

4. **Force Software Encoding**:
   ```bash
   ./edl2ffmpeg input.json output.mp4 --hw-accel none
   ```
   Eliminate hardware encoder variables.

### Quality Issues

If quality doesn't match:

1. **Adjust CRF**:
   ```bash
   ./edl2ffmpeg input.json output.mp4 --crf 20  # Lower = better quality
   ```

2. **Change Preset**:
   ```bash
   ./edl2ffmpeg input.json output.mp4 --preset slow  # Better compression
   ```

3. **Increase Bitrate**:
   ```bash
   ./edl2ffmpeg input.json output.mp4 -b 8000000  # 8 Mbps
   ```

## Future Improvements

Planned enhancements for encoder matching:

1. **Configuration Profiles**: Load ftv_toffmpeg-compatible presets
2. **Automatic Detection**: Detect reference encoder settings from sample files
3. **Validation Tool**: Automated comparison with reference encoder
4. **Custom Presets**: User-defined encoding profiles

## References

- [FFmpeg Encoding Guide](https://trac.ffmpeg.org/wiki/Encode/H.264)
- [x264 Settings](https://www.videolan.org/developers/x264.html)
- [VideoToolbox Documentation](https://developer.apple.com/documentation/videotoolbox)
- ftv_toffmpeg source code (internal reference)