#!/bin/bash

# Output comparison tool for edl2ffmpeg tests
# Compares rendered output against reference videos

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REFERENCE_DIR="${SCRIPT_DIR}/references"
OUTPUT_DIR="${SCRIPT_DIR}/output"
REPORT_FILE="${SCRIPT_DIR}/comparison_report.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Tolerance levels
SSIM_THRESHOLD=0.98  # Minimum acceptable SSIM
PSNR_THRESHOLD=40     # Minimum acceptable PSNR (dB)
FRAME_DIFF_THRESHOLD=0.01  # Maximum acceptable frame difference

usage() {
	echo "Usage: $0 [OPTIONS] test_video reference_video"
	echo "Compare test video output against reference"
	echo ""
	echo "Options:"
	echo "  -s THRESHOLD   SSIM threshold (default: 0.98)"
	echo "  -p THRESHOLD   PSNR threshold in dB (default: 40)"
	echo "  -f             Compare frame-by-frame checksums"
	echo "  -v             Verbose output"
	echo "  -o FILE        Output report file (default: comparison_report.txt)"
	echo "  -h             Show this help message"
}

VERBOSE=false
FRAME_COMPARE=false

while getopts "s:p:fvo:h" opt; do
	case $opt in
		s) SSIM_THRESHOLD="$OPTARG" ;;
		p) PSNR_THRESHOLD="$OPTARG" ;;
		f) FRAME_COMPARE=true ;;
		v) VERBOSE=true ;;
		o) REPORT_FILE="$OPTARG" ;;
		h) usage; exit 0 ;;
		*) usage; exit 1 ;;
	esac
done

shift $((OPTIND-1))

if [ $# -lt 2 ]; then
	echo -e "${RED}Error: Missing required arguments${NC}"
	usage
	exit 1
fi

TEST_VIDEO="$1"
REF_VIDEO="$2"

# Check files exist
if [ ! -f "$TEST_VIDEO" ]; then
	echo -e "${RED}Error: Test video not found: $TEST_VIDEO${NC}"
	exit 1
fi

if [ ! -f "$REF_VIDEO" ]; then
	echo -e "${RED}Error: Reference video not found: $REF_VIDEO${NC}"
	exit 1
fi

# Initialize report
echo "================================================" > "$REPORT_FILE"
echo "Video Comparison Report" >> "$REPORT_FILE"
echo "Generated: $(date)" >> "$REPORT_FILE"
echo "================================================" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
echo "Test Video: $TEST_VIDEO" >> "$REPORT_FILE"
echo "Reference Video: $REF_VIDEO" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Function to extract video properties
get_video_info() {
	local video=$1
	ffprobe -v quiet -print_format json -show_format -show_streams "$video"
}

# Function to compare basic properties
compare_properties() {
	echo -e "${BLUE}Comparing video properties...${NC}"
	echo "## Video Properties" >> "$REPORT_FILE"
	echo "" >> "$REPORT_FILE"
	
	# Get properties
	test_info=$(ffprobe -v quiet -select_streams v:0 -show_entries stream=width,height,r_frame_rate,duration,nb_frames -of csv=p=0 "$TEST_VIDEO")
	ref_info=$(ffprobe -v quiet -select_streams v:0 -show_entries stream=width,height,r_frame_rate,duration,nb_frames -of csv=p=0 "$REF_VIDEO")
	
	IFS=',' read -r test_width test_height test_fps test_duration test_frames <<< "$test_info"
	IFS=',' read -r ref_width ref_height ref_fps ref_duration ref_frames <<< "$ref_info"
	
	# Compare dimensions
	if [ "$test_width" = "$ref_width" ] && [ "$test_height" = "$ref_height" ]; then
		echo -e "${GREEN}✓ Resolution matches: ${test_width}x${test_height}${NC}"
		echo "✓ Resolution: ${test_width}x${test_height}" >> "$REPORT_FILE"
	else
		echo -e "${RED}✗ Resolution mismatch: ${test_width}x${test_height} vs ${ref_width}x${ref_height}${NC}"
		echo "✗ Resolution mismatch: ${test_width}x${test_height} vs ${ref_width}x${ref_height}" >> "$REPORT_FILE"
		return 1
	fi
	
	# Compare frame rate
	if [ "$test_fps" = "$ref_fps" ]; then
		echo -e "${GREEN}✓ Frame rate matches: $test_fps${NC}"
		echo "✓ Frame rate: $test_fps" >> "$REPORT_FILE"
	else
		echo -e "${YELLOW}⚠ Frame rate differs: $test_fps vs $ref_fps${NC}"
		echo "⚠ Frame rate differs: $test_fps vs $ref_fps" >> "$REPORT_FILE"
	fi
	
	# Compare duration (with tolerance)
	if [ -n "$test_duration" ] && [ -n "$ref_duration" ]; then
		duration_diff=$(echo "scale=3; ($test_duration - $ref_duration)" | bc)
		duration_diff_abs=$(echo "scale=3; if ($duration_diff < 0) -$duration_diff else $duration_diff" | bc)
		
		if (( $(echo "$duration_diff_abs < 0.1" | bc -l) )); then
			echo -e "${GREEN}✓ Duration matches: ${test_duration}s${NC}"
			echo "✓ Duration: ${test_duration}s" >> "$REPORT_FILE"
		else
			echo -e "${YELLOW}⚠ Duration differs: ${test_duration}s vs ${ref_duration}s (diff: ${duration_diff}s)${NC}"
			echo "⚠ Duration differs: ${test_duration}s vs ${ref_duration}s (diff: ${duration_diff}s)" >> "$REPORT_FILE"
		fi
	fi
	
	echo "" >> "$REPORT_FILE"
}

# Function to calculate SSIM
calculate_ssim() {
	echo -e "${BLUE}Calculating SSIM...${NC}"
	echo "## SSIM Analysis" >> "$REPORT_FILE"
	echo "" >> "$REPORT_FILE"
	
	# Calculate SSIM
	ssim_output=$(ffmpeg -i "$TEST_VIDEO" -i "$REF_VIDEO" -lavfi "ssim=stats_file=ssim.log" -f null - 2>&1 | grep "SSIM")
	
	# Extract average SSIM
	avg_ssim=$(echo "$ssim_output" | sed -n 's/.*All:\([0-9.]*\).*/\1/p')
	
	if [ -n "$avg_ssim" ]; then
		echo "Average SSIM: $avg_ssim" >> "$REPORT_FILE"
		
		if (( $(echo "$avg_ssim >= $SSIM_THRESHOLD" | bc -l) )); then
			echo -e "${GREEN}✓ SSIM: $avg_ssim (threshold: $SSIM_THRESHOLD)${NC}"
			echo "✓ SSIM passes threshold" >> "$REPORT_FILE"
		else
			echo -e "${RED}✗ SSIM: $avg_ssim (below threshold: $SSIM_THRESHOLD)${NC}"
			echo "✗ SSIM below threshold" >> "$REPORT_FILE"
			return 1
		fi
	else
		echo -e "${YELLOW}⚠ Could not calculate SSIM${NC}"
		echo "⚠ SSIM calculation failed" >> "$REPORT_FILE"
	fi
	
	# Clean up log file
	rm -f ssim.log
	
	echo "" >> "$REPORT_FILE"
}

# Function to calculate PSNR
calculate_psnr() {
	echo -e "${BLUE}Calculating PSNR...${NC}"
	echo "## PSNR Analysis" >> "$REPORT_FILE"
	echo "" >> "$REPORT_FILE"
	
	# Calculate PSNR
	psnr_output=$(ffmpeg -i "$TEST_VIDEO" -i "$REF_VIDEO" -lavfi "psnr=stats_file=psnr.log" -f null - 2>&1 | grep "PSNR")
	
	# Extract average PSNR
	avg_psnr=$(echo "$psnr_output" | sed -n 's/.*average:\([0-9.]*\).*/\1/p')
	
	if [ -n "$avg_psnr" ]; then
		echo "Average PSNR: ${avg_psnr} dB" >> "$REPORT_FILE"
		
		if (( $(echo "$avg_psnr >= $PSNR_THRESHOLD" | bc -l) )); then
			echo -e "${GREEN}✓ PSNR: ${avg_psnr} dB (threshold: ${PSNR_THRESHOLD} dB)${NC}"
			echo "✓ PSNR passes threshold" >> "$REPORT_FILE"
		else
			echo -e "${RED}✗ PSNR: ${avg_psnr} dB (below threshold: ${PSNR_THRESHOLD} dB)${NC}"
			echo "✗ PSNR below threshold" >> "$REPORT_FILE"
			return 1
		fi
	else
		echo -e "${YELLOW}⚠ Could not calculate PSNR${NC}"
		echo "⚠ PSNR calculation failed" >> "$REPORT_FILE"
	fi
	
	# Clean up log file
	rm -f psnr.log
	
	echo "" >> "$REPORT_FILE"
}

# Function to compare frame checksums
compare_frames() {
	echo -e "${BLUE}Comparing frame checksums...${NC}"
	echo "## Frame Checksum Comparison" >> "$REPORT_FILE"
	echo "" >> "$REPORT_FILE"
	
	# Generate frame checksums
	ffmpeg -i "$TEST_VIDEO" -f framemd5 -c copy test_frames.md5 2>/dev/null
	ffmpeg -i "$REF_VIDEO" -f framemd5 -c copy ref_frames.md5 2>/dev/null
	
	# Compare checksums
	diff_output=$(diff test_frames.md5 ref_frames.md5 2>&1) || true
	
	if [ -z "$diff_output" ]; then
		echo -e "${GREEN}✓ Frame checksums match exactly${NC}"
		echo "✓ All frame checksums match" >> "$REPORT_FILE"
	else
		# Count different frames
		diff_count=$(echo "$diff_output" | grep "^[<>]" | wc -l)
		total_frames=$(grep -c "^0," test_frames.md5)
		
		if [ "$total_frames" -gt 0 ]; then
			diff_percent=$(echo "scale=2; $diff_count * 100 / $total_frames" | bc)
			
			echo -e "${YELLOW}⚠ Frame differences: $diff_count/$total_frames (${diff_percent}%)${NC}"
			echo "⚠ Frame differences: $diff_count/$total_frames (${diff_percent}%)" >> "$REPORT_FILE"
			
			if [ "$VERBOSE" = true ]; then
				echo "First 10 differences:" >> "$REPORT_FILE"
				echo "$diff_output" | head -10 >> "$REPORT_FILE"
			fi
		fi
	fi
	
	# Clean up
	rm -f test_frames.md5 ref_frames.md5
	
	echo "" >> "$REPORT_FILE"
}

# Main comparison
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Video Output Comparison${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Run comparisons
PASS=true

compare_properties || PASS=false
calculate_ssim || PASS=false
calculate_psnr || PASS=false

if [ "$FRAME_COMPARE" = true ]; then
	compare_frames || PASS=false
fi

# Summary
echo "" >> "$REPORT_FILE"
echo "## Summary" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

if [ "$PASS" = true ]; then
	echo -e "${GREEN}========================================${NC}"
	echo -e "${GREEN}✓ All tests PASSED${NC}"
	echo -e "${GREEN}========================================${NC}"
	echo "Result: PASS" >> "$REPORT_FILE"
else
	echo -e "${RED}========================================${NC}"
	echo -e "${RED}✗ Some tests FAILED${NC}"
	echo -e "${RED}========================================${NC}"
	echo "Result: FAIL" >> "$REPORT_FILE"
fi

echo ""
echo -e "${BLUE}Full report saved to: $REPORT_FILE${NC}"

# Exit with appropriate code
[ "$PASS" = true ] && exit 0 || exit 1