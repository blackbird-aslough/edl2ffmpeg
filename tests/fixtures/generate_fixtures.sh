#!/bin/bash

# Test fixture generator for edl2ffmpeg
# Generates synthetic test videos using FFmpeg's built-in sources
# These are deterministic and reproducible across different systems

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Generating test fixtures...${NC}"

# Function to generate a test video
generate_video() {
	local name=$1
	local filter=$2
	local duration=$3
	local fps=$4
	local size=$5
	local extra_args=${6:-}
	
	echo -e "${YELLOW}Generating ${name}...${NC}"
	ffmpeg -y -loglevel error -f lavfi -i "${filter}" \
		-c:v libx264 -preset ultrafast -crf 22 \
		-pix_fmt yuv420p -t ${duration} -r ${fps} -s ${size} \
		${extra_args} \
		"${OUTPUT_DIR}/${name}"
	echo -e "${GREEN}✓ ${name}${NC}"
}

# Function to generate a test audio
generate_audio() {
	local name=$1
	local filter=$2
	local duration=$3
	local sample_rate=$4
	
	echo -e "${YELLOW}Generating ${name}...${NC}"
	ffmpeg -y -loglevel error -f lavfi -i "${filter}" \
		-t ${duration} -ar ${sample_rate} \
		"${OUTPUT_DIR}/${name}"
	echo -e "${GREEN}✓ ${name}${NC}"
}

# 1. Basic test patterns
generate_video "test_bars_1080p_30fps_10s.mp4" \
	"smptebars=size=1920x1080:rate=30:duration=10" \
	10 30 1920x1080

generate_video "test_bars_720p_60fps_5s.mp4" \
	"smptebars=size=1280x720:rate=60:duration=5" \
	5 60 1280x720

# 2. Color sources for testing effects
generate_video "color_red_1080p_30fps_5s.mp4" \
	"color=c=red:size=1920x1080:rate=30:duration=5" \
	5 30 1920x1080

generate_video "color_green_1080p_30fps_5s.mp4" \
	"color=c=green:size=1920x1080:rate=30:duration=5" \
	5 30 1920x1080

generate_video "color_blue_1080p_30fps_5s.mp4" \
	"color=c=blue:size=1920x1080:rate=30:duration=5" \
	5 30 1920x1080

# 3. Gradient patterns for testing brightness/contrast
generate_video "gradient_h_1080p_30fps_5s.mp4" \
	"gradients=size=1920x1080:rate=30:duration=5:c0=000000:c1=ffffff:x0=0:x1=1920:y0=0:y1=0" \
	5 30 1920x1080

generate_video "gradient_v_1080p_30fps_5s.mp4" \
	"gradients=size=1920x1080:rate=30:duration=5:c0=000000:c1=ffffff:x0=0:x1=0:y0=0:y1=1080" \
	5 30 1920x1080

# 4. Counter overlay for frame accuracy testing
generate_video "counter_1080p_30fps_10s.mp4" \
	"testsrc2=size=1920x1080:rate=30:duration=10" \
	10 30 1920x1080

# 5. Moving patterns for motion testing
generate_video "scrolling_text_1080p_30fps_10s.mp4" \
	"color=c=black:size=1920x1080:rate=30:duration=10,drawtext=fontfile=/System/Library/Fonts/Helvetica.ttc:fontsize=72:fontcolor=white:x=(w-text_w)/2:y=h-80*t:text='SCROLLING TEST TEXT'" \
	10 30 1920x1080

# 6. Different frame rates for testing frame rate conversion
generate_video "test_bars_1080p_24fps_5s.mp4" \
	"smptebars=size=1920x1080:rate=24:duration=5" \
	5 24 1920x1080

generate_video "test_bars_1080p_25fps_5s.mp4" \
	"smptebars=size=1920x1080:rate=25:duration=5" \
	5 25 1920x1080

generate_video "test_bars_1080p_50fps_5s.mp4" \
	"smptebars=size=1920x1080:rate=50:duration=5" \
	5 50 1920x1080

# 7. Different resolutions for testing scaling
generate_video "test_bars_480p_30fps_5s.mp4" \
	"smptebars=size=640x480:rate=30:duration=5" \
	5 30 640x480

generate_video "test_bars_4k_30fps_3s.mp4" \
	"smptebars=size=3840x2160:rate=30:duration=3" \
	3 30 3840x2160

# 8. Noise pattern for compression testing
generate_video "noise_1080p_30fps_5s.mp4" \
	"testsrc2=size=1920x1080:rate=30:duration=5,geq=random(1)*255:128:128" \
	5 30 1920x1080

# 9. Checkerboard pattern
generate_video "checkerboard_1080p_30fps_5s.mp4" \
	"testsrc=size=1920x1080:rate=30:duration=5,format=yuv420p" \
	5 30 1920x1080

# 10. Generate test audio files
generate_audio "tone_1khz_48k_5s.wav" \
	"sine=frequency=1000:sample_rate=48000:duration=5" \
	5 48000

generate_audio "sweep_48k_5s.wav" \
	"sine=frequency=20:sample_rate=48000:duration=5:beep_factor=1000" \
	5 48000

# 11. Complex test with multiple elements
echo -e "${YELLOW}Generating complex test video...${NC}"
ffmpeg -y -loglevel error \
	-f lavfi -i "testsrc2=size=1920x1080:rate=30:duration=10" \
	-f lavfi -i "sine=frequency=440:sample_rate=48000:duration=10" \
	-filter_complex "[0:v]split=2[v1][v2];[v1]crop=960:1080:0:0[left];[v2]crop=960:1080:960:0,negate[right];[left][right]hstack[v]" \
	-map "[v]" -map 1:a \
	-c:v libx264 -preset ultrafast -crf 22 -pix_fmt yuv420p \
	-c:a aac -b:a 128k \
	"${OUTPUT_DIR}/complex_split_screen_10s.mp4"
echo -e "${GREEN}✓ complex_split_screen_10s.mp4${NC}"

# Generate file list
echo -e "${YELLOW}Generating fixture list...${NC}"
ls -la "${OUTPUT_DIR}"/*.mp4 "${OUTPUT_DIR}"/*.wav 2>/dev/null | awk '{print $NF}' | xargs -n1 basename > "${OUTPUT_DIR}/fixtures.txt"

echo -e "${GREEN}✅ All fixtures generated successfully!${NC}"
echo -e "${GREEN}Total size: $(du -sh ${OUTPUT_DIR} | cut -f1)${NC}"

# Generate checksums for verification
echo -e "${YELLOW}Generating checksums...${NC}"
(cd "${OUTPUT_DIR}" && sha256sum *.mp4 *.wav 2>/dev/null > checksums.sha256)
echo -e "${GREEN}✓ Checksums saved to checksums.sha256${NC}"