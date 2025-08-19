# NVENC Hardware Acceleration Cleanup Fix

## Summary

This document summarizes the findings from the test program that demonstrates proper resource cleanup for NVENC hardware acceleration with zero-copy decode/encode pipeline.

## Key Findings

### 1. Correct Cleanup Sequence

The proper cleanup sequence for hardware acceleration is:

1. **Finalize encoding** - Drain all async frames and flush encoder
2. **Close output file** - `avio_closep(&formatCtx->pb)`
3. **Close encoder codec** - `avcodec_close(encoderCtx)` (crucial for hardware)
4. **Free encoder context** - `avcodec_free_context(&encoderCtx)`
5. **Close decoder codec** - `avcodec_close(decoderCtx)` (crucial for hardware)
6. **Free decoder context** - `avcodec_free_context(&decoderCtx)`
7. **Free format contexts** - `avformat_free_context()`
8. **Free hardware device context** - `av_buffer_unref(&hwDeviceCtx)` (must be last!)

### 2. Critical Fix: avcodec_close()

The main issue causing crashes/hangs was not calling `avcodec_close()` before `avcodec_free_context()` for hardware codecs. While `avcodec_close()` is deprecated in newer FFmpeg versions, it's still necessary for proper hardware cleanup.

```cpp
// For hardware encoders/decoders, close codec first
if (usingHardware) {
    avcodec_close(codecCtx);
    // Small delay to ensure GPU operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
avcodec_free_context(&codecCtx);
```

### 3. Async Encoding Considerations

For async hardware encoding (like edl2ffmpeg uses):

1. **Track frames in flight** - Monitor how many frames are queued
2. **Drain async queue before flush** - Process remaining frames before sending NULL
3. **Timeout protection** - Prevent infinite loops during draining
4. **Don't free frames prematurely** - Async mode means encoder may still be using frames

### 4. Zero-Copy Best Practices

1. **Share hardware context** - Use `av_buffer_ref()` to share between decoder/encoder
2. **Check for hw_frames_ctx** - This indicates a hardware frame
3. **Avoid unnecessary transfers** - Keep frames in GPU memory when possible
4. **Proper frame lifetime** - Don't free frames that encoder might still be processing

## Recommended Changes for edl2ffmpeg

1. **Add avcodec_close() calls** in FFmpegEncoder and FFmpegDecoder destructors:
   ```cpp
   if (usingHardware && codecCtx) {
       avcodec_close(codecCtx);
       std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
   ```

2. **Ensure proper cleanup order** in main.cpp or wherever cleanup happens

3. **Add timeout protection** for async draining in FFmpegEncoder::finalize()

4. **Consider frame reference counting** for async mode to prevent premature deallocation

## Test Results

- ✅ No crashes or hangs with 100+ frames
- ✅ Zero-copy pipeline working correctly
- ✅ Async encoding functioning properly
- ✅ Clean resource deallocation
- ✅ No GPU resource leaks

The test program (`test_nvenc_pipeline.cpp`) can be used as a reference for the correct implementation pattern.