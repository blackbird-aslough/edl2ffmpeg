# FFmpeg API Flow in ffmpeg_read

This document details the sequence of FFmpeg API calls made by `ffmpeg_read.cc` for video/audio file operations.

## 1. File Opening and Initialization

### 1.1 Opening a Media File

The `openFileAndProbe()` function (ffmpeg_read.cc:1080) performs the initial file opening:

```c
AVFormatContext *ctx = avformat_alloc_context()
↓
avformat_open_input(&ctx, filename, format, &options)
↓
avformat_find_stream_info(ctx, nullptr)
```

**Key Points:**
- `avformat_alloc_context()`: Allocates format context structure
- `avformat_open_input()`: Opens the file and reads header information
- `avformat_find_stream_info()`: Probes the file to find stream information
- Custom "foxy" protocol support is handled via protocol prefixes

### 1.2 Video Stream Initialization

The `OpenVideoStream()` function (ffmpeg_read.cc:1400) finds and prepares video streams:

```c
openFileAndProbe(filename)
↓
av_dump_format() [optional, for debugging]
↓
// Iterate through streams to find video stream
for each stream:
    Check codec->codec_type == AVMEDIA_TYPE_VIDEO
    Set stream->discard = AVDISCARD_ALL for unused streams
↓
// Extract metadata (rotation, etc.)
av_dict_get(stream->metadata, "rotate", NULL, 0)
```

### 1.3 Video Codec Initialization

The `OpenVideoCodec()` function (ffmpeg_read.cc:1480) opens the video decoder:

```c
// Set thread count for multithreaded decoding
pVideoCodecCtx->thread_count = Threads
↓
avcodec_find_decoder(codec_id)
↓
// Set codec options if needed
av_dict_set(&codecOpts, "ignore_sei_recovery_points", "1", 0)
↓
avcodec_open2(pVideoCodecCtx, pVideoCodec, &codecOpts)
↓
av_dict_free(&codecOpts)
```

### 1.4 Audio Stream Initialization

The `InitAudio()` function (ffmpeg_read.cc:1909) handles audio setup:

```c
openFileAndProbe(filename)
↓
// Find all audio streams
for each stream:
    Check codec->codec_type == AVMEDIA_TYPE_AUDIO
    Create audio_reader for each audio stream
↓
// For each audio stream:
avcodec_find_decoder(codec_id)
↓
avcodec_open2(pAudioCodecCtx, pAudioCodec, NULL)
↓
// Initialize audio resampler
swr_alloc_set_opts(...)
swr_init(swr)
```

## 2. Frame Reading and Decoding

### 2.1 Packet Reading

The packet reading uses a custom wrapper `av_read_frame_throws()` (ffmpeg_read.cc:276):

```c
av_read_frame(formatContext, packet)
// Returns:
//   - 0: Success, packet read
//   - AVERROR_EOF: End of file reached
//   - AVERROR(EAGAIN): Retry needed
//   - Other: Error condition
```

### 2.2 Video Decoding Flow

The `GetNextVideoFrame()` function (ffmpeg_read.cc:3038) decodes video:

```c
GetNextVideoPacket()
    ↓
    av_read_frame_throws(pVideoFormatCtx, packet)
    ↓
    // Check for keyframe
    if (!(packet->flags & AV_PKT_FLAG_KEY))
        Skip non-keyframes until first keyframe
↓
avcodec_decode_video2(pVideoCodecCtx, pVideoFrame, &frameFinished, packet)
↓
// If multithreaded and at EOF, flush decoder:
if (!packet->data)
    Send NULL packets until no more frames
↓
// Apply deinterlacing filter if needed
av_buffersrc_add_frame(buffersrc_ctx, frame)
av_buffersink_get_frame(buffersink_ctx, filtered_frame)
↓
// Color space conversion if needed
sws_scale(pScaleContext, src_data, src_linesize, 0, height, dst_data, dst_linesize)
```

**Frame Memory Management:**
- Uses `av_frame_alloc()` / `av_frame_free()` wrapped in RAII unique_ptr
- Uses `av_packet_alloc()` / `av_packet_free()` for packets

### 2.3 Audio Decoding Flow

The `GetNextAVFrame()` function in audio_reader (ffmpeg_read.cc:2400) decodes audio:

```c
FillPacketQueue()
    ↓
    av_read_frame_throws(pAudioFormatCtx, packet)
↓
avcodec_decode_audio4(pAudioCodecCtx, frame, &gotFrame, packet)
↓
// Audio resampling to common format
swr_convert(swr, output_samples, max_out_samples, input_samples, in_samples)
```

### 2.4 Caption/Subtitle Extraction

Caption data is extracted from:
- Side data: `av_packet_get_side_data(packet, AV_PKT_DATA_A53_CC, &size)`
- SMPTE VBI data streams
- QuickTime subtitle tracks (codec_tag 'c708')

## 3. Seeking Operations

### 3.1 Video Seeking

The `SeekVideo()` function (ffmpeg_read.cc:3544) performs video seeks:

```c
// Calculate target timestamp
int64_t ts = av_rescale_q(time, output_timebase, stream_timebase)
↓
av_seek_frame(pVideoFormatCtx, VideoStream, ts, AVSEEK_FLAG_BACKWARD)
↓
// If seek fails, fallback:
av_seek_frame(pVideoFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD)
↓
// Flush codec buffers after seek
avcodec_flush_buffers(pVideoCodecCtx)
↓
// Clear packet queues and reset state
FreeVideoPackets()
```

### 3.2 Audio Seeking

The `SeekAudioKey()` function (ffmpeg_read.cc:3747) handles audio seeks:

```c
// Calculate target timestamp for each audio stream
int64_t ts = av_rescale_q(time, output_timebase, stream_timebase)
↓
av_seek_frame(pAudioFormatCtx, AudioStream, ts, AVSEEK_FLAG_BACKWARD)
↓
// If seek fails, reset to beginning:
avformat_close_input(&pAudioFormatCtx)
pAudioFormatCtx = openFileAndProbe(filename)
[reinitialize all audio streams]
↓
// Flush audio codec
avcodec_flush_buffers(pAudioCodecCtx)
```

## 4. Cleanup and Closing

### 4.1 Video Cleanup

The `FreeVideo()` function (ffmpeg_read.cc:2233) releases video resources:

```c
// Close video codec
avcodec_close(pVideoCodecCtx)
↓
// Free filter graph if used
avfilter_graph_free(&filter_graph)
↓
// Close format context
avformat_close_input(&pVideoFormatCtx)
↓
// Free scaling context
sws_freeContext(pScaleContext)
```

### 4.2 Audio Cleanup

The `FreeAudio()` function (ffmpeg_read.cc:2279) releases audio resources:

```c
// For each audio reader:
avcodec_close(pAudioCodecCtx)
swr_free(&swr)
↓
// Close format context
avformat_close_input(&pAudioFormatCtx)
```

### 4.3 General Cleanup Pattern

```c
// Packets are managed with smart pointers
av_packet_free(&packet)  // Automatic via unique_ptr destructor

// Frames are managed similarly
av_frame_free(&frame)     // Automatic via unique_ptr destructor

// Format contexts must be explicitly closed
avformat_close_input(&formatContext)
avformat_free_context(formatContext)  // If not opened yet
```

## 5. Error Handling

### 5.1 Error Recovery Strategies

- **EAGAIN**: Retry the operation
- **EOF**: Normal end of stream, return false/nullptr
- **Decode errors at EOF**: Ignore (stream may be truncated)
- **Other errors**: Throw exception or log warning

### 5.2 Timestamp Handling

The code uses `AV_NOPTS_VALUE` to indicate invalid timestamps and includes extensive timestamp smoothing logic for audio to handle:
- Missing timestamps
- Timestamp discontinuities
- Variable frame rate content

## 6. Special Features

### 6.1 Foxy Protocol

Custom streaming protocol that supports:
- Remote file access with seeking
- Protocol forwarding
- Bandwidth optimization

### 6.2 Compatibility Modes

Multiple compatibility levels (1-7) control:
- Timestamp handling
- Rotation metadata processing
- Audio packet timestamp modes
- Stream selection behavior

### 6.3 Performance Optimizations

- **Multithreaded decoding**: `codec->thread_count` setting
- **Packet buffering**: Maintains queues for smooth playback
- **Selective stream processing**: `stream->discard = AVDISCARD_ALL` for unused streams
- **Hardware acceleration**: Not currently implemented but structure supports it

## Notes

- The code uses older FFmpeg APIs (`avcodec_decode_video2` / `avcodec_decode_audio4`) rather than the newer send/receive API
- Extensive use of RAII and smart pointers for resource management
- Thread-safe design with atomic flags for interruption handling
- Supports multiple audio streams with mixing capabilities