#!/bin/bash
# Test script for hardware acceleration fixes

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Hardware Acceleration Test Script${NC}"
echo "=================================="

# Create test video if it doesn't exist
if [ ! -f test_video.mp4 ]; then
    echo "Creating test video..."
    ffmpeg -f lavfi -i testsrc=duration=10:size=1920x1080:rate=30 -pix_fmt yuv420p test_video.mp4
fi

# Build the project
echo -e "\n${YELLOW}Building project...${NC}"
if [ ! -d build ]; then
    mkdir build
fi
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_GPU=ON
make -j$(nproc)
cd ..

# Test function
test_hardware() {
    local hw_type=$1
    local codec=$2
    local output_file=$3
    
    echo -e "\n${YELLOW}Testing $hw_type with codec $codec...${NC}"
    
    # Run with hardware acceleration
    if ./build/edl2ffmpeg tests/sample_edls/simple_single_clip.json "$output_file" --hw-accel "$hw_type" --codec "$codec" -v; then
        echo -e "${GREEN}✓ $hw_type encoding successful${NC}"
        
        # Check file was created and has content
        if [ -f "$output_file" ] && [ -s "$output_file" ]; then
            echo -e "${GREEN}✓ Output file created successfully${NC}"
            
            # Get file info
            ffprobe -v error -show_entries format=duration,size -of default=noprint_wrappers=1 "$output_file"
        else
            echo -e "${RED}✗ Output file not created or empty${NC}"
            return 1
        fi
    else
        echo -e "${RED}✗ $hw_type encoding failed${NC}"
        return 1
    fi
}

# Detect available hardware acceleration
echo -e "\n${YELLOW}Detecting hardware acceleration...${NC}"
./build/edl2ffmpeg --list-hw-devices || true

# Test VAAPI (if available on Linux)
if [ -e /dev/dri/renderD128 ] || [ -e /dev/dri/card0 ]; then
    echo -e "\n${YELLOW}VAAPI device detected${NC}"
    test_hardware "vaapi" "h264_vaapi" "output_vaapi_h264.mp4" || true
    test_hardware "vaapi" "hevc_vaapi" "output_vaapi_hevc.mp4" || true
else
    echo -e "${YELLOW}No VAAPI devices found${NC}"
fi

# Test NVENC (if NVIDIA GPU available)
if nvidia-smi &> /dev/null; then
    echo -e "\n${YELLOW}NVIDIA GPU detected${NC}"
    test_hardware "nvenc" "h264_nvenc" "output_nvenc_h264.mp4" || true
    test_hardware "nvenc" "hevc_nvenc" "output_nvenc_hevc.mp4" || true
else
    echo -e "${YELLOW}No NVIDIA GPU found${NC}"
fi

# Test software encoding for comparison
echo -e "\n${YELLOW}Testing software encoding...${NC}"
if ./build/edl2ffmpeg tests/sample_edls/simple_single_clip.json output_software.mp4 --codec libx264 -v; then
    echo -e "${GREEN}✓ Software encoding successful${NC}"
else
    echo -e "${RED}✗ Software encoding failed${NC}"
fi

# Clean up
echo -e "\n${YELLOW}Cleaning up test files...${NC}"
rm -f output_*.mp4

echo -e "\n${GREEN}Test complete!${NC}"