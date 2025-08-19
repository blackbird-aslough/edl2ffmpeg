#!/bin/bash

# Test script to compare hardware encoding performance
# This helps verify that the fix doesn't regress Linux hardware encoding performance

echo "Performance Test for Hardware Encoding"
echo "======================================"

# Create a test EDL if it doesn't exist
if [ ! -f "test_simple3.json" ]; then
    echo "Error: test_simple3.json not found"
    exit 1
fi

# Function to run test and measure time
run_test() {
    local test_name=$1
    local output_file=$2
    shift 2
    local args=("$@")
    
    echo -n "Running $test_name... "
    
    # Run the command and measure time
    start_time=$(date +%s.%N)
    
    if ./build/edl2ffmpeg test_simple3.json "$output_file" "${args[@]}" > /dev/null 2>&1; then
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc)
        echo "Done in ${duration}s"
        
        # Get file size
        size=$(ls -lh "$output_file" | awk '{print $5}')
        echo "  Output size: $size"
        
        # Cleanup
        rm -f "$output_file"
    else
        echo "FAILED"
    fi
    
    echo ""
}

# Test different configurations
echo "1. CPU Encoding (baseline)"
run_test "CPU encoding" "output_cpu_test.mp4"

echo "2. VAAPI Hardware Encoding"
run_test "VAAPI encoding" "output_vaapi_test.mp4" --hw-accel vaapi --hw-encode

echo "3. VAAPI Hardware Encoding + Decoding"
run_test "VAAPI encode+decode" "output_vaapi_full_test.mp4" --hw-accel vaapi --hw-encode --hw-decode

# If NVIDIA GPU is available
if command -v nvidia-smi &> /dev/null; then
    echo "4. NVENC Hardware Encoding"
    run_test "NVENC encoding" "output_nvenc_test.mp4" --hw-accel nvenc --hw-encode
    
    echo "5. NVENC Hardware Encoding + Decoding"
    run_test "NVENC encode+decode" "output_nvenc_full_test.mp4" --hw-accel nvenc --hw-encode --hw-decode
fi

echo "Performance test complete!"
echo ""
echo "Note: The fix optimizes hardware frame transfer by attempting direct transfer first,"
echo "which should significantly improve performance for Linux hardware acceleration (VAAPI/NVENC)"
echo "while maintaining compatibility with VideoToolbox on macOS."