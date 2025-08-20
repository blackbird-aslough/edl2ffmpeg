# Test Suite Handoff Documentation

## Overview
A comprehensive test suite has been implemented to compare `edl2ffmpeg` output against the reference `ftv_toffmpeg` renderer. The infrastructure is complete and functional, but the reference renderer integration needs path mapping fixes.

## Current Status

### ✅ Fully Working
- **Test Framework**: Catch2 + ApprovalTests integrated and building correctly
- **edl2ffmpeg**: Renders all test cases successfully
- **Test Infrastructure**: All utilities (VideoComparator, TestRunner, EDLGenerator) implemented
- **Test Fixtures**: 15+ synthetic test videos generated in `tests/fixtures/`

### ❌ Needs Fixing
- **Reference Renderer**: Docker container fails with exit code 4 due to path mapping issues

## The Problem

When running tests, you'll see:
```
Reference failed with exit code: 4
ftv_extract_rawav failed, output:
ffmpeg failed, output:
Reference output file not created: /var/folders/.../edl2ffmpeg_tests/XXX_ref.mp4
```

The issue: The Docker container can't find the input video files referenced in the EDL because:
1. EDL contains path: `fixtures/test_bars_1080p_30fps_10s.mp4`
2. Test runs from: `/Users/.../edl2ffmpeg/tests/`
3. Docker mounts current dir to `/work`
4. But the container can't resolve the relative paths correctly

## How to Fix

### Quick Test
```bash
cd tests
# This works (edl2ffmpeg runs fine):
../build/edl2ffmpeg approval/fixtures/single_clip.json /tmp/test.mp4

# This fails (Docker path issue):
../scripts/ftv_toffmpeg_wrapper_full.sh approval/fixtures/single_clip.json /tmp/ref.mp4
```

### Solution Options

#### Option 1: Fix Path Resolution in Docker Wrapper
Modify `scripts/ftv_toffmpeg_wrapper_full.sh` to:
- Detect relative paths in EDL files
- Mount the fixtures directory explicitly
- Rewrite paths in EDL to be container-relative

#### Option 2: Use Absolute Paths
Modify `TestRunner::updateEDLVideoPath()` to:
- Convert relative paths to absolute before passing to Docker
- Ensure all paths are fully qualified

#### Option 3: Copy Files to Container Working Directory
- Copy test fixtures into `/work` inside container
- Adjust EDL paths to reference files in `/work`

### Files to Review

1. **Docker Wrapper**: `scripts/ftv_toffmpeg_wrapper_full.sh`
   - Lines 85-103: Mount point logic
   - Need to ensure fixtures directory is accessible

2. **Test Runner**: `tests/integration/common/TestRunner.cpp`
   - `updateEDLVideoPath()`: Updates paths in EDL
   - `runReference()`: Executes Docker wrapper

3. **Test EDLs**: `tests/approval/fixtures/*.json`
   - Currently use relative paths like `fixtures/test_bars_1080p_30fps_10s.mp4`

## Testing Your Fix

Once you've fixed the path mapping:

```bash
# Build tests
cd build
make test_integration

# Run quick tests
cd ../tests
../build/tests/test_integration "[quick]" --reporter compact

# If successful, you should see:
# "All tests passed (13 assertions in 2 test cases)"

# Run all tests
./run_tests.sh --approval
```

## Expected Behavior When Fixed

1. Both renderers produce output files
2. VideoComparator compares them frame-by-frame
3. Tests pass with PSNR > 35dB (visually identical)
4. No "Reference renderer failed" errors

## Test Suite Structure

```
tests/
├── fixtures/                 # Generated test videos (MP4, WAV)
├── integration/
│   ├── common/              # Test utilities
│   │   ├── VideoComparator  # Frame comparison (PSNR)
│   │   ├── TestRunner       # Executes both renderers
│   │   └── EDLGenerator     # Random EDL generation
│   ├── approval/            # Fixed test cases
│   └── generative/          # Random property tests
└── run_tests.sh             # Main test script
```

## Success Criteria

You'll know the fix is working when:
1. `ftv_toffmpeg_wrapper_full.sh` successfully renders test EDLs
2. Test output shows both renderers completing
3. PSNR values are reported (should be >35dB for identical output)
4. No Docker mount errors or file not found errors

## Notes

- The Docker image `ftv_toffmpeg:full` must be loaded (11.3GB)
- Docker Desktop must be running
- Test fixtures must be generated first (`./fixtures/generate_fixtures.sh`)
- The wrapper script already handles platform detection (ARM64/x86_64)

## Contact

The test infrastructure is complete - this is purely a Docker path configuration issue. Once fixed, the entire test suite will provide comprehensive validation that edl2ffmpeg matches the reference implementation.