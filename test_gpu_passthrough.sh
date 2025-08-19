#!/bin/bash

# Test GPU passthrough with shared hardware context

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Testing GPU passthrough with shared hardware context${NC}"

# Create test video if it doesn't exist
TEST_VIDEO="test_video.mp4"
if [ ! -f "$TEST_VIDEO" ]; then
    echo -e "${YELLOW}Creating test video...${NC}"
    ffmpeg -f lavfi -i testsrc2=duration=5:size=1920x1080:rate=30 -c:v libx264 -preset ultrafast "$TEST_VIDEO" -y 2>/dev/null
fi

# Create a simple EDL
cat > test_gpu_passthrough.json << EOF
{
    "version": "1.0",
    "type": "composition",
    "generator": "gpu_passthrough_test",
    "created": "2024-08-18T00:00:00Z",
    "timeline": {
        "renderSettings": {
            "width": 1920,
            "height": 1080,
            "pixelFormat": "rgba",
            "colorSpace": "sRGB",
            "fps": 30
        },
        "duration": 150,
        "clips": [
            {
                "id": "clip1",
                "source": {
                    "id": "source1",
                    "type": "media",
                    "uri": "$TEST_VIDEO"
                },
                "sourceStartFrame": 0,
                "startFrame": 0,
                "endFrame": 150,
                "track": {
                    "type": "video",
                    "number": 0
                }
            }
        ]
    }
}
EOF

# Test configurations
echo -e "\n${YELLOW}1. Testing software decode + software encode (baseline)${NC}"
./build/edl2ffmpeg test_gpu_passthrough.json output_sw_sw.mp4 -v --hw-accel none 2>&1 | grep -E "(Using|hardware|GPU|passthrough|isHardwareFrame|hw_frames_ctx)"

echo -e "\n${YELLOW}2. Testing hardware decode + hardware encode (GPU passthrough)${NC}"
./build/edl2ffmpeg test_gpu_passthrough.json output_hw_hw.mp4 -v --hw-accel cuda --hw-decode --hw-encode 2>&1 | grep -E "(Using|hardware|GPU|passthrough|isHardwareFrame|hw_frames_ctx|Shared)"

echo -e "\n${YELLOW}3. Testing VAAPI hardware acceleration${NC}"
./build/edl2ffmpeg test_gpu_passthrough.json output_vaapi.mp4 -v --hw-accel vaapi --hw-decode --hw-encode 2>&1 | grep -E "(Using|hardware|GPU|passthrough|isHardwareFrame|hw_frames_ctx|Shared)"

# Compare file sizes
echo -e "\n${YELLOW}File sizes:${NC}"
ls -lh output_sw_sw.mp4 output_hw_hw.mp4 output_vaapi.mp4 2>/dev/null || true

# Clean up
rm -f test_gpu_passthrough.json

echo -e "\n${GREEN}GPU passthrough test completed!${NC}"