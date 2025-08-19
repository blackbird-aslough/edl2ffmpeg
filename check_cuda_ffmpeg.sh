#!/bin/bash

echo "Checking FFmpeg CUDA support..."
echo "================================"

# Check FFmpeg version
echo -e "\n1. FFmpeg version:"
ffmpeg -version | head -1

# Check for CUDA hwaccel
echo -e "\n2. Hardware accelerations:"
ffmpeg -hwaccels 2>&1 | grep -E "(cuda|nvdec|vaapi)" || echo "No CUDA/NVDEC found in hwaccels"

# Check for NVENC encoders
echo -e "\n3. NVENC encoders:"
ffmpeg -encoders 2>&1 | grep -E "nvenc" || echo "No NVENC encoders found"

# Check for CUVID decoders
echo -e "\n4. CUVID decoders:"
ffmpeg -decoders 2>&1 | grep -E "cuvid" || echo "No CUVID decoders found"

# Check FFmpeg configuration
echo -e "\n5. FFmpeg configuration (CUDA-related):"
ffmpeg -buildconf 2>&1 | grep -E "(cuda|nvenc|nvdec|cuvid|nv-codec)" || echo "No CUDA-related build flags found"

# Check if AV_HWDEVICE_TYPE_CUDA is defined
echo -e "\n6. Checking for CUDA hwdevice support in our build:"
if grep -q "AV_HWDEVICE_TYPE_CUDA" /nix/store/*/include/libavutil/hwcontext.h 2>/dev/null; then
    echo "✓ AV_HWDEVICE_TYPE_CUDA is defined in FFmpeg headers"
else
    echo "✗ AV_HWDEVICE_TYPE_CUDA not found in FFmpeg headers"
fi

echo -e "\n7. Checking pkg-config for FFmpeg version:"
pkg-config --modversion libavcodec || echo "pkg-config not working"

echo -e "\nDone!"