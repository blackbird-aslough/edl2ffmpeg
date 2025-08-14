#!/usr/bin/env bash

# Build script for edl2ffmpeg
# Can be run inside nix-shell or with nix-shell --run

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
JOBS=$(nproc)
RUN_TESTS=true
CLEAN_BUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --no-tests)
            RUN_TESTS=false
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug       Build in debug mode"
            echo "  --release     Build in release mode (default)"
            echo "  --clean       Clean build directory before building"
            echo "  --no-tests    Skip running tests"
            echo "  --jobs N      Number of parallel jobs (default: $(nproc))"
            echo "  --help        Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Building edl2ffmpeg (${BUILD_TYPE} mode)${NC}"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS=ON \
    -DENABLE_SIMD=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo -e "${YELLOW}Building with $JOBS parallel jobs...${NC}"
make -j"$JOBS"

# Run tests if enabled
if [ "$RUN_TESTS" = true ]; then
    echo -e "${YELLOW}Running tests...${NC}"
    if ctest --output-on-failure; then
        echo -e "${GREEN}All tests passed!${NC}"
    else
        echo -e "${RED}Some tests failed!${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "Executable location: ${PWD}/edl2ffmpeg"

# Show example usage
echo -e "\n${YELLOW}Example usage:${NC}"
echo "  ./edl2ffmpeg input.json output.mp4"
echo "  ./edl2ffmpeg input.json output.mp4 --codec libx265 --crf 28"