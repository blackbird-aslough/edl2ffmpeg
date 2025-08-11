# FTV Tools Documentation

This document describes two key tools in the FBT framework for video processing: `ftv_toffmpeg` and `ftv_extract_rawav`.

## ftv_toffmpeg

A Python script that converts FTV/EDL files to various video formats using FFmpeg.

### Synopsis
```bash
ftv_toffmpeg [options] <filename.edl> <output filename> [<output filename>]...
```

### Description
`ftv_toffmpeg` is a comprehensive video conversion tool that reads EDL (Edit Decision List) files and produces output in various formats by piping raw audio/video data through FFmpeg. It supports both standard FTV files and Blackbird-compressed formats.

### Input Options
- `--blackbird`: Source is blackbird format

### Output Format Options
- `-f<format>`: Output file format (default: dvd/mpeg program stream)
- `--codec <c>`: Output video codec (default: mpeg2video)
- `--acodec <c>`: Output audio codec (default: mp2)
- `--rate <rate>`: Video bitrate in kbps (default: 4000)
- `--crf <crf>`: H.264/H.265 Constant Rate Factor (0-51 or 63)
- `--arate <rate>`: Audio bitrate in kbps per channel (default: 192)
- `--aspect <a>`: Video aspect ratio (e.g., 4:3, 16:9)
- `-r<hz>`: Audio sample rate in Hz (default: 48000)
- `--pixel-format <format>`: Pixel format options:
	- `yuv422p` (default)
	- `yuv420p`
	- `yuv422p10`, `yuv420p10` (10-bit)
	- `yuv422p16`, `yuv420p16` (16-bit)
	- `rgb`, `bgr`
- `--speedunadjust`: Output NTSC-like footage at 23.98/29.97/59.94fps
- `--no-still-boxing`: Don't letterbox/pillarbox stills
- `--interlaced`: Output interlaced video
- `--deinterlace`: Always produce deinterlaced output

### Video Filters
- `--logo <file>`: Superimpose logo on video
- `--fps <num>`: Subsample video frames
- `-sw<width>`: Scale to given width
- `-sh<height>`: Scale to given height
- `-r[lrtb]<pix>`: Crop pixels from left/right/top/bottom

### Audio Filters
- `--volume <v>`: Change volume by factor (e.g., 0.5, -6dB)

### Preset Option Sets
Format: video-codec@rate/audio-codec@rate/file-format

- `--mpeg2`: mpeg2@4000kbps/mp2@192kbps/dvd (default)
- `--mp4`: mpeg4@4000kbps/libfdk_aac@192kbps/mp4
- `--ogg`: libtheora@4000kbps/libvorbis@192kbps/ogg
- `--gif`: Generate animated GIF
- `--ipod`: mpeg4@684kbps:320x240/libfdk_aac@192kbps/mp4
- `--flv`: flv@400kbps/libmp3lame@44.1kHz+64kbps/flv
- `--avi`: rawvideo:bgr/pcmaudio/avi
- `--dv`/`--dvcpro`: DV output (forces speedunadjust and scale)
- `--wmv`: wmv2@684kbps/vmav2@56kbps/asf
- `--h264`/`--h264onepass`: h264@436kbps/libfdk_aac@64kbps/mp4
- `--h264twopass`: Two-pass H.264 encoding
- `--h265`: h265@CRF26/libfdk_aac@128kbps/mp4
- `--mxf`: mpeg2@4000kbps/pcmaudio/mxf
- `--html5`: Output ogg and h264 files @1000kbps/128kbps
- `--mp3`: Audio only - libmp3lame@192kbps/mp3
- `--hls`: HTTP Live Streaming - h264@2000kbps/libfdk_aac@256kbps/mpegts
- `--dnxhd`: DNxHD@220Mbps/pcmaudio/mxf
- `--xdcam`: MPEG-2 422P@HL @50Mbps/pcmaudio/mxf
- `--xdcamex`: MPEG-2 MP@HL @35Mbps/pcmaudio/mxf
- `--avcintra50`/`--avcintra100`: AVC Intra profiles

### Advanced Options
- `--kill <file>`: Terminate if file appears
- `--tmp <dir>`: Use directory for temporary files
- `--first-pass <opt>`: Extra ffmpeg first pass option
- `--ffmpeg-opt <opt>`: Extra ffmpeg option
- `--ffmpeg-opt-clear`: Clear existing ffmpeg options
- `--ffmpeg-fmt-opt <fmt>:<opt>`: Format-specific ffmpeg option
- `--extra-output`: Produce additional output file
- `--modify-all-outputs`: Apply format options to all outputs
- `--no-faststart`: Don't move index to start for QuickTime formats
- `--ftv-output`: Pipe .v file to command ({} replaced by pipe name)
- `--ftv-output-only`: Like ftv-output but no ffmpeg output
- `--ftv-ffmpeg-read-threads <num>`: Number of read threads (default: 1)
- `--captions-embed <l>:<f>`: Embed SRT captions with ISO 639-2 language
- `--progress <csvfilename>`: Write progress timing to CSV
- `--report <filename>`: Write error reports to file
- `--from-build-tree`: Run binaries from build tree for testing

## ftv_extract_rawav

A C++ program that extracts raw audio and video from FTV (.v) files.

### Synopsis
```bash
ftv_extract_rawav infile.v [options]
```

### Description
`ftv_extract_rawav` reads FTV video files and extracts raw audio and/or video data in various formats. It can read from files, stdin, or named pipes, and supports extensive video processing options.

### Input Options
- `-i[<fifo>]`: Read from stdin or named pipe
- `--edlin[=<fifo>]`: Read EDL from stdin or named pipe
- `--settings[=<fifo>]`: Read settings JSON from stdin or named pipe
- `--settings-help`: Show settings help and exit
- `--ftvout=<file>`: Output .v file of raw video to file/pipe
- `--kill=<file>`: Exit if file appears
- `--report=<file>`: Write error reports to file
- `--buffer=<seconds>`: Set buffering duration between input/output

### Video Output Options
- `-v<file>`: Output file for video data
- Video format options:
	- `--yuv`: YUV format (default)
	- `--yuyv`: YUYV format
	- `--yuv420p`: YUV420P planar format
	- `--yuv422p`: YUV422P planar format
	- `--yuv444p16`: YUV444P 16-bit format
	- `--yuv422p16`: YUV422P 16-bit format
	- `--yuv420p16`: YUV420P 16-bit format
	- `--uyvy16`: UYVY 16-bit format
	- `--v210`: V210 10-bit format
	- `--rgb`: RGB format
	- `--bgr`: BGR format
- `--subtitle`: Render subtitles onto video
- `--logo <image>`: Add logo in top right
- `--invert`: Invert video vertically
- `--fps=<num>`: Convert to specified framerate
- `--deinterlace`: Produce deinterlaced output
- `--threads=<num>`: Number of worker threads
- `--no-still-boxing`: Don't letterbox/pillarbox stills

### Video Processing Options
- `-sw<width>`: Scale to given width
- `-sh<height>`: Scale to given height
- `-rt<pix>`: Crop pixels from top
- `-rb<pix>`: Crop pixels from bottom
- `-rl<pix>`: Crop pixels from left
- `-rr<pix>`: Crop pixels from right

### Color Space Options
- `--colour-standard=<std>`: Set color standard (default: BT.709)
- `--colour-primaries=<std>`: Set color primaries
- `--colour-transfer=<std>`: Set transfer characteristic
- `--full-range=<bool>`: Full range (true) or MPEG range (false) (default: true)
- `-Yu`: Deprecated alias for `--full-range=false`

### Audio Output Options
- `-a<file>`: Output file for audio data
- `-r<rate>`: Audio sample rate in Hz (default: 48000)
- `-b<bits>`: Bits per sample (default: 16)
- `-c<channels>`: Number of audio channels (default: 2)
- `--no-mix`: Don't mix audio channels (allows >2 channels)

### Progress and Monitoring
- `--progress=<path>`: Write progress timing to CSV file

### Settings JSON Format
When using `--settings`, the JSON file may contain:
- `edl`: JSON or text EDL content
- `writerConfig`: JSON or text writer configuration

## Workflow

### Typical Pipeline
1. **ftv_extract_rawav** reads FTV files and extracts raw audio/video streams
2. **ftv_toffmpeg** uses ftv_extract_rawav internally (or ftv_bbd_extract_rawav for Blackbird) to extract data
3. Raw data is piped to FFmpeg for encoding to the desired output format

### Example Usage

Convert EDL to H.264 MP4:
```bash
ftv_toffmpeg --h264 input.edl output.mp4
```

Extract raw YUV video from FTV file:
```bash
ftv_extract_rawav input.v -voutput.yuv --yuv422p
```

Convert with custom settings:
```bash
ftv_toffmpeg --codec libx264 --crf 23 --acodec aac --arate 256 \
	-sw1920 -sh1080 input.edl output.mp4
```

Extract with subtitle rendering and logo:
```bash
ftv_extract_rawav input.v -voutput.yuv --subtitle --logo logo.png
```

## Notes

- Both tools support various pixel formats for compatibility with different workflows
- The tools handle color space conversions between different standards (BT.709, BT.601, etc.)
- Audio can be resampled, mixed, and converted between different bit depths
- Progress monitoring allows integration with larger workflows
- Kill file mechanism enables graceful termination in automated pipelines
- Buffer settings help manage memory usage for large files
- Thread control allows optimization for different hardware configurations