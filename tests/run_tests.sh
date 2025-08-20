#!/bin/bash

# Test runner script for edl2ffmpeg comprehensive test suite
# Compares edl2ffmpeg output against reference ftv_toffmpeg renderer

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
TEST_TYPE="all"
VERBOSE=""
UPDATE_GOLDEN=""
SEED=""

while [[ $# -gt 0 ]]; do
	case $1 in
		--quick)
			TEST_TYPE="quick"
			shift
			;;
		--approval)
			TEST_TYPE="approval"
			shift
			;;
		--generative)
			TEST_TYPE="generative"
			shift
			;;
		--verbose|-v)
			VERBOSE="-v high"
			shift
			;;
		--update-golden)
			UPDATE_GOLDEN="UPDATE_GOLDEN=1"
			shift
			;;
		--seed)
			SEED="--rng-seed $2"
			shift 2
			;;
		--help|-h)
			echo "Usage: $0 [options]"
			echo "Options:"
			echo "  --quick          Run only quick smoke tests"
			echo "  --approval       Run approval tests only"
			echo "  --generative     Run generative property tests only"
			echo "  --verbose        Enable verbose output"
			echo "  --update-golden  Update golden reference files"
			echo "  --seed <N>       Set random seed for generative tests"
			echo "  --help           Show this help message"
			exit 0
			;;
		*)
			echo "Unknown option: $1"
			exit 1
			;;
	esac
done

# Check if we're in the test directory
if [[ ! -f "CMakeLists.txt" ]]; then
	cd tests 2>/dev/null || {
		echo -e "${RED}Error: Must run from project root or tests directory${NC}"
		exit 1
	}
fi

# Check if Docker is running (needed for reference renderer)
if ! docker info >/dev/null 2>&1; then
	echo -e "${YELLOW}Warning: Docker is not running. Reference renderer tests will fail.${NC}"
	echo -e "${YELLOW}Please start Docker Desktop and try again.${NC}"
	read -p "Continue anyway? (y/n) " -n 1 -r
	echo
	if [[ ! $REPLY =~ ^[Yy]$ ]]; then
		exit 1
	fi
fi

# Check if test fixtures exist
if [[ ! -f "fixtures/test_bars_1080p_30fps_10s.mp4" ]]; then
	echo -e "${YELLOW}Test fixtures not found. Generating...${NC}"
	./fixtures/generate_fixtures.sh || {
		echo -e "${RED}Failed to generate test fixtures${NC}"
		exit 1
	}
fi

# Build tests if needed
if [[ ! -f "../build/tests/test_integration" ]]; then
	echo -e "${YELLOW}Test executable not found. Building...${NC}"
	(cd ../build && cmake .. && make test_integration) || {
		echo -e "${RED}Failed to build tests${NC}"
		exit 1
	}
fi

# Check if edl2ffmpeg executable exists
if [[ ! -f "../build/edl2ffmpeg" ]]; then
	echo -e "${RED}Error: edl2ffmpeg executable not found${NC}"
	echo "Please build the project first: cd build && cmake .. && make"
	exit 1
fi

# Run tests based on type
echo -e "${BLUE}=== Running edl2ffmpeg Test Suite ===${NC}"
echo

case $TEST_TYPE in
	quick)
		echo -e "${GREEN}Running quick smoke tests...${NC}"
		$UPDATE_GOLDEN ../build/tests/test_integration "[quick]" --reporter compact $VERBOSE
		;;
	approval)
		echo -e "${GREEN}Running approval tests...${NC}"
		$UPDATE_GOLDEN ../build/tests/test_integration "[approval]" $VERBOSE
		;;
	generative)
		echo -e "${GREEN}Running generative property tests...${NC}"
		../build/tests/test_integration "[generative]" $SEED $VERBOSE
		;;
	all)
		echo -e "${GREEN}Running all tests...${NC}"
		
		# Quick tests first
		echo -e "\n${BLUE}Quick smoke tests:${NC}"
		$UPDATE_GOLDEN ../build/tests/test_integration "[quick]" --reporter compact
		
		# Then approval tests
		echo -e "\n${BLUE}Approval tests:${NC}"
		$UPDATE_GOLDEN ../build/tests/test_integration "[approval]" --reporter compact
		
		# Finally generative tests
		echo -e "\n${BLUE}Generative tests:${NC}"
		../build/tests/test_integration "[generative]" --reporter compact $SEED
		;;
esac

# Check exit code
if [[ $? -eq 0 ]]; then
	echo -e "\n${GREEN}✓ All tests passed!${NC}"
else
	echo -e "\n${RED}✗ Some tests failed${NC}"
	echo "To debug a specific failing test, run:"
	echo "  ../build/tests/test_integration <test-name> -v high"
	echo "To update golden files:"
	echo "  UPDATE_GOLDEN=1 ../build/tests/test_integration [approval]"
	exit 1
fi