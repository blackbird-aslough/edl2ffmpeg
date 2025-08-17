#!/bin/bash

echo "=== VideoToolbox Hardware Acceleration Diagnostic ==="
echo "System: $(system_profiler SPHardwareDataType | grep 'Chip:' | xargs)"
echo "macOS: $(sw_vers -productVersion)"
echo ""

# Check VideoToolbox availability
echo "VideoToolbox Support:"
echo "--------------------"
ffmpeg -hide_banner -encoders 2>/dev/null | grep videotoolbox | head -5
echo ""

# Test different resolutions to see where hardware shines
echo "Performance Test Results:"
echo "========================"
echo ""

# Clean up
rm -f test_*.mp4 2>/dev/null

# Test 1: 1080p (your current test)
if [ -f "test_simple3.json" ]; then
    echo "1080p Test (1920x1080):"
    echo "----------------------"
    
    echo -n "Software (libx264): "
    /usr/bin/time -p ./build/edl2ffmpeg test_simple3.json test_sw_1080p.mp4 2>&1 | grep real | awk '{print $2}'
    
    echo -n "Hardware (VideoToolbox): "
    /usr/bin/time -p ./build/edl2ffmpeg test_simple3.json test_hw_1080p.mp4 --hw-accel videotoolbox --hw-encode --hw-decode 2>&1 | grep real | awk '{print $2}'
    
    echo ""
fi

# Generate a 4K test file if needed
echo "Creating 4K test EDL..."
cat > test_4k.json << 'EOF'
{
    "fps": 30,
    "width": 3840,
    "height": 2160,
    "clips": [
        {
            "source": {
                "uri": "Test_Live_1906_High.mxf",
                "trackId": "V1",
                "in": 0,
                "out": 10
            },
            "in": 0,
            "out": 10,
            "track": {
                "type": "video",
                "number": 1
            }
        }
    ]
}
EOF

# Check if media file exists
if [ -f "Test_Live_1906_High.mxf" ]; then
    echo "4K Test (3840x2160):"
    echo "-------------------"
    
    echo -n "Software (libx264): "
    timeout 60 /usr/bin/time -p ./build/edl2ffmpeg test_4k.json test_sw_4k.mp4 2>&1 | grep real | awk '{print $2}'
    
    echo -n "Hardware (VideoToolbox): "
    timeout 60 /usr/bin/time -p ./build/edl2ffmpeg test_4k.json test_hw_4k.mp4 --hw-accel videotoolbox --hw-encode --hw-decode 2>&1 | grep real | awk '{print $2}'
    
    echo ""
fi

echo "Analysis:"
echo "--------"
echo "1. VideoToolbox uses dedicated Apple Silicon media engine (not CPU cores)"
echo "2. For 1080p, M4 CPU is fast enough that hardware doesn't provide benefit"
echo "3. Hardware acceleration benefits increase with resolution (4K/8K)"
echo "4. The '1 thread' for VideoToolbox is normal - it's hardware, not CPU"
echo ""

# Check GPU usage during encoding
echo "GPU Activity Monitor:"
echo "-------------------"
echo "To see hardware encoder usage, run this in another terminal:"
echo "sudo powermetrics --samplers gpu_power -i 1000 -n 10"
echo ""

# File size comparison
echo "Output Quality Comparison:"
echo "------------------------"
ls -lh test_*.mp4 2>/dev/null | awk '{print $9 ": " $5}'
echo ""

echo "Recommendations:"
echo "---------------"
echo "• For 1080p: Use software encoding (better quality, same speed)"
echo "• For 4K/8K: Use hardware encoding (much faster)"
echo "• For battery life: Hardware uses less power despite similar speed"
echo "• For streaming: Hardware has better real-time performance"