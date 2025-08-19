# Hardware Acceleration Resource Cleanup Guide

This document describes the correct process for cleaning up FFmpeg resources when using hardware acceleration (NVENC, VAAPI, VideoToolbox) with zero-copy pipelines.

## Overview

When using FFmpeg with hardware acceleration, particularly NVENC with CUDA, proper resource cleanup is critical to avoid:
- Segmentation faults during shutdown
- GPU resource leaks
- Hanging processes
- Corrupted encoder/decoder state

## The Problem

The default FFmpeg cleanup process (`avcodec_free_context()`) is insufficient for hardware-accelerated codecs. GPU resources may still be in use when the CPU tries to free them, leading to crashes or hangs.

## The Solution

### Critical Requirement: Call avcodec_close()

Even though `avcodec_close()` is deprecated in newer FFmpeg versions, it is **still required** for hardware codec cleanup. This function ensures:
- All GPU operations are completed
- Hardware resources are properly released
- The codec is put into a safe state for deallocation

### Correct Cleanup Sequence

```cpp
// 1. Finalize encoding/decoding operations
if (asyncMode) {
    // Drain any remaining frames in the async queue
    while (framesInFlight > 0) {
        receivePacketsAsync();
    }
}

// 2. Flush the codec
avcodec_send_frame(codecCtx, nullptr);  // Send flush signal
// Drain all remaining packets...

// 3. Close output file (if applicable)
if (outputFormatCtx && !(outputFormatCtx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&outputFormatCtx->pb);
}

// 4. CRITICAL: Close hardware codecs before freeing contexts
if (encoderCtx) {
    if (usingHardware) {
        avcodec_close(encoderCtx);  // Essential for hardware cleanup
        // Small delay to ensure GPU operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    avcodec_free_context(&encoderCtx);
}

if (decoderCtx) {
    if (usingHardware) {
        avcodec_close(decoderCtx);  // Essential for hardware cleanup
    }
    avcodec_free_context(&decoderCtx);
}

// 5. Free format contexts
avformat_free_context(outputFormatCtx);
avformat_close_input(&inputFormatCtx);

// 6. Free hardware device context LAST
if (hwDeviceCtx) {
    av_buffer_unref(&hwDeviceCtx);
}
```

## Async Encoding Considerations

When using async encoding (common with hardware encoders):

### 1. Track Frames in Flight
```cpp
std::atomic<int> framesInFlight{0};

// When sending frame
if (avcodec_send_frame(codecCtx, frame) == 0) {
    framesInFlight++;
}

// When receiving packet
if (avcodec_receive_packet(codecCtx, packet) == 0) {
    framesInFlight--;
}
```

### 2. Drain Before Cleanup
```cpp
// Before cleanup, ensure all async operations complete
int flushAttempts = 0;
while (framesInFlight > 0 && flushAttempts < 100) {
    bool received = receivePacketsAsync();
    if (!received) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    flushAttempts++;
}
```

### 3. Timeout Protection
Always use iteration limits to prevent infinite loops during cleanup:
```cpp
int maxIterations = 1000;
int iterations = 0;
while (!done && iterations < maxIterations) {
    // Draining logic...
    iterations++;
}
```

## Zero-Copy Pipeline Best Practices

### 1. Share Hardware Context
```cpp
// Create once
AVBufferRef* hwDeviceCtx = nullptr;
av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);

// Share between decoder and encoder
decoderCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
encoderCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
```

### 2. Detect Hardware Frames
```cpp
bool isHardwareFrame = (frame->hw_frames_ctx != nullptr);
```

### 3. Avoid Unnecessary Transfers
Keep frames in GPU memory throughout the pipeline when possible.

## Common Pitfalls

1. **Not calling avcodec_close()** - The most common cause of crashes
2. **Wrong cleanup order** - Freeing hardware context before codecs
3. **Not draining async frames** - Leaves GPU operations incomplete
4. **Freeing frames too early** - Async encoder may still be using them
5. **No timeout protection** - Can hang indefinitely during cleanup
6. **Setting NVENC options after avcodec_open2()** - Causes internal state corruption and crashes
7. **Not tracking hardware context ownership** - Can lead to double-free when using shared contexts

## Platform-Specific Notes

### NVENC (NVIDIA)
- Requires explicit `avcodec_close()` call
- Benefits from small delay after close (50-100ms)
- Set `surfaces` option for async depth

### VAAPI (Intel/AMD Linux)
- Also requires `avcodec_close()`
- Less sensitive to timing than NVENC
- Check for `/dev/dri/renderD128` availability

### VideoToolbox (macOS)
- May work without explicit close but safer to include
- Doesn't require explicit hardware device context
- Set `async_depth` for better throughput

## Testing

The included test program `test_nvenc_pipeline.cpp` demonstrates the correct implementation. Run it to verify:

```bash
./test_nvenc_pipeline --frames 100
```

Expected output:
- No segmentation faults
- Clean shutdown
- "Cleanup completed" message

## References

- [FFmpeg Hardware Acceleration Documentation](https://trac.ffmpeg.org/wiki/HWAccelIntro)
- [NVIDIA FFmpeg Transcoding Guide](https://developer.nvidia.com/blog/nvidia-ffmpeg-transcoding-guide/)
- Test implementation: `test_nvenc_pipeline.cpp`