#!/bin/bash

echo "Testing NVENC with different approaches..."
echo "=========================================="

# Test 1: Try with auto hardware acceleration
echo -e "\n1. Testing with auto hardware acceleration:"
./build/edl2ffmpeg ../test_simple3.json output_auto.mp4 --hw-accel auto --hw-encode -v 2>&1 | tail -10

# Test 2: Try with VAAPI (if available)
echo -e "\n2. Testing with VAAPI:"
./build/edl2ffmpeg ../test_simple3.json output_vaapi.mp4 --hw-accel vaapi --hw-encode -v 2>&1 | tail -10

# Test 3: Try without hardware encoding (software only)
echo -e "\n3. Testing with software encoding:"
./build/edl2ffmpeg ../test_simple3.json output_software.mp4 -v 2>&1 | tail -10

# Test 4: Check what encoders are actually available
echo -e "\n4. Available encoders:"
ffmpeg -encoders 2>&1 | grep -E "(264|nvenc|vaapi)" | head -10

echo -e "\nTest complete!"