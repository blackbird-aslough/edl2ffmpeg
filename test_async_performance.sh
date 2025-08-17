#!/bin/bash

echo "=== Async Encoding Performance Test ==="
echo "Testing async frame batching with VideoToolbox and software encoding"
echo ""

# Clean up previous outputs
rm -f output_*.mp4 2>/dev/null

# Test 1: Hardware with async encoding (VideoToolbox)
echo "Test 1: VideoToolbox with async encoding (16-frame batching)"
echo "-----------------------------------------------------------"
time ./build/edl2ffmpeg test_simple3.json output_hw_async.mp4 --hw-accel videotoolbox --hw-encode --hw-decode 2>&1 | grep -E "(FPS|Average FPS|Async encoding enabled)"
echo ""

# Test 2: Software encoding (baseline)
echo "Test 2: Software encoding (synchronous)"
echo "---------------------------------------"
time ./build/edl2ffmpeg test_simple3.json output_sw.mp4 2>&1 | grep -E "(FPS|Average FPS)"
echo ""

# Test 3: NVENC simulation (if available)
echo "Test 3: Testing with explicit h264 codec"
echo "----------------------------------------"
time ./build/edl2ffmpeg test_simple3.json output_h264.mp4 --codec h264 --hw-accel videotoolbox --hw-encode 2>&1 | grep -E "(FPS|Average FPS|Async encoding enabled)"
echo ""

echo "Performance Analysis:"
echo "-------------------"
echo "• Async encoding should show 'Async encoding enabled' in logs"
echo "• Hardware encoding should now batch up to 16 frames"
echo "• FPS should improve significantly for hardware encoding"
echo "• NVENC on Linux would show even better improvements"
echo ""

# Check if async mode was enabled
echo "Checking encoder configuration:"
./build/edl2ffmpeg test_simple3.json output_test.mp4 --hw-accel videotoolbox --hw-encode --hw-decode 2>&1 | grep -E "(Encoder initialized|Async encoding)" | head -2

rm -f output_test.mp4