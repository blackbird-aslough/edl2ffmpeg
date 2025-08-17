#!/bin/bash

# Test script to compare VideoToolbox hardware acceleration performance
# Run this on your M4 MacBook to see the improvements

echo "=== VideoToolbox Performance Test ==="
echo "Testing with file: test_simple3.json"
echo ""

# Clean up previous outputs
rm -f output_hw.mp4 output_sw.mp4 2>/dev/null

# Test 1: Hardware acceleration with VideoToolbox
echo "Test 1: Hardware acceleration (VideoToolbox)"
echo "-------------------------------------------"
time ./build/edl2ffmpeg test_simple3.json output_hw.mp4 --hw-accel videotoolbox --hw-encode --hw-decode
echo ""

# Test 2: Software encoding (baseline)
echo "Test 2: Software encoding (baseline)"
echo "------------------------------------"
time ./build/edl2ffmpeg test_simple3.json output_sw.mp4
echo ""

# Compare file sizes
echo "File size comparison:"
echo "-------------------"
ls -lh output_hw.mp4 output_sw.mp4 2>/dev/null | awk '{print $9 ": " $5}'
echo ""

# Check encoder info
echo "Encoder information:"
echo "-------------------"
echo "Hardware output:"
ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,profile,has_b_frames -of default=noprint_wrappers=1 output_hw.mp4
echo ""
echo "Software output:"
ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,profile,has_b_frames -of default=noprint_wrappers=1 output_sw.mp4
echo ""

echo "=== Test Complete ==="
echo "Expected improvements in hardware mode:"
echo "- Should now use multiple threads (check log output)"
echo "- Should show similar or better FPS than software"
echo "- Should use high profile instead of main"
echo "- Should have GPU passthrough enabled"