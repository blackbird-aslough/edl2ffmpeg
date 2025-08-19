#!/bin/bash

# Test CUDA hardware decoding with fixed CUDA device context creation

echo "Testing CUDA hardware decoding..."
echo "================================"

# Create a test video if it doesn't exist
if [ ! -f test_input.mp4 ]; then
    echo "Creating test video..."
    ffmpeg -f lavfi -i testsrc=duration=5:size=1920x1080:rate=30 -c:v libx264 test_input.mp4
fi

# Test with CUDA hardware acceleration (should now work)
echo -e "\n1. Testing with CUDA acceleration:"
./build/edl2ffmpeg tests/sample_edls/simple_single_clip.json test_cuda.mp4 --hw-accel cuda -v 2>&1 | grep -E "(Hardware|CUDA|device|Using|Failed|Error)"

# Test with software decoding for comparison
echo -e "\n2. Testing with software decoding:"
./build/edl2ffmpeg tests/sample_edls/simple_single_clip.json test_software.mp4 --hw-accel none -v 2>&1 | grep -E "(Hardware|Using|device)"

# Check if NVIDIA GPU is available
echo -e "\n3. Checking NVIDIA GPU availability:"
nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>/dev/null || echo "NVIDIA GPU not detected or nvidia-smi not available"

# Check for CUDA-capable FFmpeg
echo -e "\n4. Checking FFmpeg CUDA support:"
ffmpeg -hwaccels 2>&1 | grep -i cuda || echo "CUDA hwaccel not found in FFmpeg"
ffmpeg -decoders 2>&1 | grep -E "h264_cuvid|hevc_cuvid" | head -5 || echo "CUVID decoders not found in FFmpeg"
ffmpeg -encoders 2>&1 | grep -E "h264_nvenc|hevc_nvenc" | head -5 || echo "NVENC encoders not found in FFmpeg"

echo -e "\nTest complete!"