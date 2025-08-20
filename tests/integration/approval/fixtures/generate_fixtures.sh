#!/bin/bash

# Generate test fixtures for edl2ffmpeg integration tests
# This script creates test videos and EDL JSON files needed for the test suite

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Generating Test Fixtures ==="

# Generate test video with color bars (1080p, 30fps, 10 seconds)
echo "Generating test_bars_1080p_30fps_10s.mp4..."
ffmpeg -y -f lavfi -i "testsrc2=size=1920x1080:rate=30:duration=10" \
	-c:v libx264 -preset fast -crf 23 \
	-pix_fmt yuv420p \
	test_bars_1080p_30fps_10s.mp4

# Generate solid color test videos for transitions
echo "Generating solid_red.mp4..."
ffmpeg -y -f lavfi -i "color=c=red:size=1920x1080:rate=30:duration=5" \
	-c:v libx264 -preset fast -crf 23 \
	-pix_fmt yuv420p \
	solid_red.mp4

echo "Generating solid_blue.mp4..."
ffmpeg -y -f lavfi -i "color=c=blue:size=1920x1080:rate=30:duration=5" \
	-c:v libx264 -preset fast -crf 23 \
	-pix_fmt yuv420p \
	solid_blue.mp4

echo "Generating solid_green.mp4..."
ffmpeg -y -f lavfi -i "color=c=green:size=1920x1080:rate=30:duration=5" \
	-c:v libx264 -preset fast -crf 23 \
	-pix_fmt yuv420p \
	solid_green.mp4

# Generate a video with counter for frame accuracy testing
echo "Generating counter_1080p_30fps_10s.mp4..."
ffmpeg -y -f lavfi -i "testsrc2=size=1920x1080:rate=30:duration=10" \
	-vf "drawtext=text='Frame %{n}':x=10:y=10:fontsize=48:fontcolor=white:box=1:boxcolor=black@0.5" \
	-c:v libx264 -preset fast -crf 23 \
	-pix_fmt yuv420p \
	counter_1080p_30fps_10s.mp4

echo "=== Test fixtures generated successfully ==="
echo "Generated files:"
ls -lh *.mp4