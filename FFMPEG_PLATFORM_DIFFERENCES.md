# Platform Differences: Linux vs macOS Output

## Summary
The Linux and macOS outputs differ significantly due to **B-frame handling** in the encoder configuration. This is intentional behavior in the codebase to work around VideoToolbox issues.

## Key Differences Found

### 1. File Size
- **Linux output**: 18.9 MB (output-linux.mp4)
- **macOS output**: 25.6 MB (output.mp4)
- **Difference**: ~35% larger on macOS

### 2. Frame Count & Duration
- **Linux**: 7491 frames, 299.600 seconds
- **macOS**: 7495 frames, 299.760 seconds
- **Difference**: 4 extra frames on macOS

### 3. Bitrate
- **Linux**: ~505 kbps
- **macOS**: ~683 kbps
- **Difference**: ~35% higher bitrate on macOS

### 4. B-Frames (ROOT CAUSE)
- **Linux**: has_b_frames=2 (B-frames enabled)
- **macOS**: has_b_frames=0 (B-frames disabled)

## Root Cause Analysis

The difference stems from this code in `src/media/FFmpegEncoder.cpp:202-209`:

```cpp
// VideoToolbox has issues with B-frames and PTS/DTS ordering
// Disable B-frames for VideoToolbox, use 2 for other codecs
if (codecName.find("videotoolbox") != std::string::npos) {
    codecCtx->max_b_frames = 0; // No B-frames for VideoToolbox
    utils::Logger::debug("Setting max_b_frames=0 for VideoToolbox encoder");
} else {
    codecCtx->max_b_frames = 2; // Default B-frames for other codecs
    utils::Logger::debug("Setting max_b_frames=2 for encoder: {}", codecName);
}
```

### Why This Happens

1. **On macOS**: When hardware encoding is auto-detected or enabled, the code may use `h264_videotoolbox` encoder
2. **VideoToolbox Limitation**: Apple's VideoToolbox has known issues with B-frame PTS/DTS ordering
3. **Workaround**: The code disables B-frames for any encoder with "videotoolbox" in its name
4. **On Linux**: Uses standard `libx264` with B-frames enabled

### Impact of B-Frames

B-frames (Bidirectional frames) are a video compression technique that:
- Reference both past and future frames
- Provide better compression (smaller file sizes)
- Require frame reordering (different decode vs display order)

Without B-frames:
- Only P-frames (forward prediction) and I-frames (keyframes) are used
- Less efficient compression → larger file sizes
- Simpler frame ordering → better compatibility

## Verification

To confirm which encoder was used, run:
```bash
# Check if hardware encoding was used
ffprobe -v error -select_streams v:0 -show_entries stream=has_b_frames output.mp4
```

## Solutions

### Option 1: Force Software Encoding on macOS
```bash
./edl2ffmpeg input.json output.mp4 --hw-accel none
```

### Option 2: Accept Platform Differences
The current behavior is intentional to ensure reliable encoding on macOS with VideoToolbox.

### Option 3: Modify Code to Allow B-frames Configuration
Add a command-line option to control B-frame usage:
```cpp
// Add to Config struct
int maxBFrames = -1; // -1 = auto, 0 = disabled, >0 = specific count

// Modify the logic
if (config.maxBFrames >= 0) {
    codecCtx->max_b_frames = config.maxBFrames;
} else if (codecName.find("videotoolbox") != std::string::npos) {
    codecCtx->max_b_frames = 0; // VideoToolbox default
} else {
    codecCtx->max_b_frames = 2; // Other codecs default
}
```

## Recommendation

The current behavior is **correct and intentional**. The platform differences are a necessary trade-off for:
1. Reliable encoding on macOS with hardware acceleration
2. Avoiding VideoToolbox PTS/DTS ordering issues
3. Maintaining compatibility across platforms

If identical output is required across platforms:
- Use software encoding only (`--hw-accel none`)
- Or explicitly disable B-frames on all platforms
- Or implement platform-specific test expectations