#!/bin/bash

# Reference generator for edl2ffmpeg tests
# Uses the pipe-based system to generate reference outputs for comparison

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURE_DIR="${SCRIPT_DIR}/fixtures"
REFERENCE_DIR="${SCRIPT_DIR}/references"
EDL_DIR="${SCRIPT_DIR}/sample_edls"

# Path to the pipe-based tool (update this to your actual path)
PIPE_TOOL="${PIPE_TOOL:-ftv_toffmpeg}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
	echo "Usage: $0 [OPTIONS]"
	echo "Generate reference outputs for edl2ffmpeg tests"
	echo ""
	echo "Options:"
	echo "  -p PATH     Path to pipe-based tool (default: ftv_toffmpeg)"
	echo "  -e EDL      Process only specific EDL file"
	echo "  -c          Generate checksums only (no video files)"
	echo "  -f          Extract key frames as PNG"
	echo "  -m          Calculate quality metrics (SSIM/PSNR)"
	echo "  -h          Show this help message"
	echo ""
	echo "Environment:"
	echo "  PIPE_TOOL   Path to pipe-based rendering tool"
}

# Parse command line arguments
CHECKSUM_ONLY=false
EXTRACT_FRAMES=false
CALC_METRICS=false
SPECIFIC_EDL=""

while getopts "p:e:cfmh" opt; do
	case $opt in
		p) PIPE_TOOL="$OPTARG" ;;
		e) SPECIFIC_EDL="$OPTARG" ;;
		c) CHECKSUM_ONLY=true ;;
		f) EXTRACT_FRAMES=true ;;
		m) CALC_METRICS=true ;;
		h) usage; exit 0 ;;
		*) usage; exit 1 ;;
	esac
done

# Check if pipe tool exists
if ! command -v "$PIPE_TOOL" &> /dev/null; then
	echo -e "${RED}Error: Pipe-based tool '$PIPE_TOOL' not found${NC}"
	echo "Please set PIPE_TOOL environment variable or use -p option"
	exit 1
fi

# Check if fixtures exist
if [ ! -d "$FIXTURE_DIR" ]; then
	echo -e "${YELLOW}Fixtures directory not found. Generating fixtures...${NC}"
	"${FIXTURE_DIR}/generate_fixtures.sh"
fi

# Create reference directories
mkdir -p "${REFERENCE_DIR}/videos"
mkdir -p "${REFERENCE_DIR}/checksums"
mkdir -p "${REFERENCE_DIR}/frames"
mkdir -p "${REFERENCE_DIR}/metrics"

# Function to generate checksum for a video
generate_checksum() {
	local video_file=$1
	local output_file=$2
	
	echo -e "${BLUE}Generating checksum for $(basename $video_file)...${NC}"
	
	# Generate frame checksums
	ffmpeg -i "$video_file" -f framemd5 -c copy "$output_file.framemd5" 2>/dev/null
	
	# Generate overall file checksum
	sha256sum "$video_file" > "$output_file.sha256"
	
	# Generate perceptual hash (if available)
	if command -v ffmpeg &> /dev/null && ffmpeg -filters 2>/dev/null | grep -q "perceptualhash"; then
		ffmpeg -i "$video_file" -vf "perceptualhash" -f null - 2>&1 | \
			grep "perceptual_hash" > "$output_file.phash" || true
	fi
}

# Function to extract key frames
extract_frames() {
	local video_file=$1
	local output_dir=$2
	local base_name=$(basename "$video_file" .mp4)
	
	echo -e "${BLUE}Extracting key frames from $(basename $video_file)...${NC}"
	
	mkdir -p "$output_dir/$base_name"
	
	# Extract first, middle, and last frames
	duration=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$video_file")
	mid_time=$(echo "$duration / 2" | bc -l)
	
	ffmpeg -ss 0 -i "$video_file" -frames:v 1 "$output_dir/$base_name/frame_first.png" -y 2>/dev/null
	ffmpeg -ss "$mid_time" -i "$video_file" -frames:v 1 "$output_dir/$base_name/frame_middle.png" -y 2>/dev/null
	ffmpeg -sseof -1 -i "$video_file" -frames:v 1 "$output_dir/$base_name/frame_last.png" -y 2>/dev/null
	
	# Extract frames at 1-second intervals (up to 10 frames)
	for i in {1..10}; do
		if (( $(echo "$i <= $duration" | bc -l) )); then
			ffmpeg -ss $i -i "$video_file" -frames:v 1 "$output_dir/$base_name/frame_${i}s.png" -y 2>/dev/null
		fi
	done
}

# Function to calculate quality metrics
calculate_metrics() {
	local ref_video=$1
	local test_video=$2
	local output_file=$3
	
	if [ ! -f "$test_video" ]; then
		echo "Test video not found, skipping metrics"
		return
	fi
	
	echo -e "${BLUE}Calculating SSIM/PSNR metrics...${NC}"
	
	# Calculate SSIM
	ssim=$(ffmpeg -i "$ref_video" -i "$test_video" -lavfi "ssim" -f null - 2>&1 | grep "SSIM" | tail -1)
	
	# Calculate PSNR
	psnr=$(ffmpeg -i "$ref_video" -i "$test_video" -lavfi "psnr" -f null - 2>&1 | grep "PSNR" | tail -1)
	
	# Save metrics
	echo "{" > "$output_file"
	echo "  \"ssim\": \"$ssim\"," >> "$output_file"
	echo "  \"psnr\": \"$psnr\"" >> "$output_file"
	echo "}" >> "$output_file"
}

# Function to process an EDL file
process_edl() {
	local edl_file=$1
	local base_name=$(basename "$edl_file" .json)
	
	echo -e "${GREEN}Processing EDL: $base_name${NC}"
	
	# Generate reference video using pipe-based tool
	if [ "$CHECKSUM_ONLY" = false ]; then
		echo -e "${YELLOW}Rendering with pipe-based tool...${NC}"
		output_video="${REFERENCE_DIR}/videos/${base_name}_reference.mp4"
		
		# Run the pipe-based tool (adjust command as needed for your tool)
		"$PIPE_TOOL" "$edl_file" "$output_video"
		
		if [ $? -eq 0 ]; then
			echo -e "${GREEN}✓ Reference video generated${NC}"
			
			# Generate checksums
			generate_checksum "$output_video" "${REFERENCE_DIR}/checksums/${base_name}"
			
			# Extract frames if requested
			if [ "$EXTRACT_FRAMES" = true ]; then
				extract_frames "$output_video" "${REFERENCE_DIR}/frames"
			fi
			
			# Calculate metrics if requested and test video exists
			if [ "$CALC_METRICS" = true ]; then
				test_video="output_${base_name}.mp4"
				if [ -f "$test_video" ]; then
					calculate_metrics "$output_video" "$test_video" "${REFERENCE_DIR}/metrics/${base_name}.json"
				fi
			fi
		else
			echo -e "${RED}✗ Failed to generate reference for $base_name${NC}"
			return 1
		fi
	else
		# Checksum only mode - assume video already exists
		if [ -f "${REFERENCE_DIR}/videos/${base_name}_reference.mp4" ]; then
			generate_checksum "${REFERENCE_DIR}/videos/${base_name}_reference.mp4" \
				"${REFERENCE_DIR}/checksums/${base_name}"
		else
			echo -e "${YELLOW}Warning: Reference video not found for $base_name${NC}"
		fi
	fi
}

# Main processing
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Reference Generator for edl2ffmpeg${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Pipe tool: ${BLUE}$PIPE_TOOL${NC}"
echo ""

# Process EDL files
if [ -n "$SPECIFIC_EDL" ]; then
	# Process specific EDL
	if [ -f "$SPECIFIC_EDL" ]; then
		process_edl "$SPECIFIC_EDL"
	else
		echo -e "${RED}Error: EDL file '$SPECIFIC_EDL' not found${NC}"
		exit 1
	fi
else
	# Process all EDL files
	for edl in "$EDL_DIR"/*.json; do
		if [ -f "$edl" ]; then
			process_edl "$edl"
		fi
	done
fi

# Generate summary
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Reference Generation Complete${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "References saved to: ${BLUE}${REFERENCE_DIR}${NC}"

if [ "$CHECKSUM_ONLY" = false ]; then
	video_count=$(ls -1 "${REFERENCE_DIR}/videos"/*.mp4 2>/dev/null | wc -l)
	echo -e "Videos generated: ${GREEN}$video_count${NC}"
fi

checksum_count=$(ls -1 "${REFERENCE_DIR}/checksums"/*.framemd5 2>/dev/null | wc -l)
echo -e "Checksums generated: ${GREEN}$checksum_count${NC}"

if [ "$EXTRACT_FRAMES" = true ]; then
	frame_count=$(find "${REFERENCE_DIR}/frames" -name "*.png" 2>/dev/null | wc -l)
	echo -e "Frames extracted: ${GREEN}$frame_count${NC}"
fi

if [ "$CALC_METRICS" = true ]; then
	metrics_count=$(ls -1 "${REFERENCE_DIR}/metrics"/*.json 2>/dev/null | wc -l)
	echo -e "Metrics calculated: ${GREEN}$metrics_count${NC}"
fi

echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "1. Run edl2ffmpeg on the same EDL files"
echo "2. Use compare_outputs.sh to validate results"
echo "3. Review any differences in the comparison report"