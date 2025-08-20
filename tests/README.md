# edl2ffmpeg Test Suite

Comprehensive test suite that compares edl2ffmpeg output against the reference ftv_toffmpeg renderer.

## Overview

The test suite includes:
- **Unit Tests**: Test individual components (EDL parser, etc.)
- **Approval Tests**: Fixed test cases with known good outputs
- **Generative Tests**: Property-based tests with random EDLs
- **Performance Tests**: Benchmark comparison against reference

## Quick Start

```bash
# Generate test fixtures (required first time)
cd tests
./fixtures/generate_fixtures.sh

# Run all tests
./run_tests.sh

# Run specific test types
./run_tests.sh --quick      # Quick smoke tests only
./run_tests.sh --approval   # Approval tests only
./run_tests.sh --generative # Property-based tests only
```

## Prerequisites

1. **Docker Desktop** must be running (for reference renderer)
2. **Test fixtures** must be generated:
   ```bash
   ./fixtures/generate_fixtures.sh
   ```
3. **Build the project**:
   ```bash
   cd build
   cmake ..
   make
   ```

## Test Types

### Approval Tests
Compare specific EDL features against reference implementation:
- Single/multiple clips
- Brightness/contrast effects
- Fade in/out
- Frame rate conversion
- Resolution scaling
- Frame-accurate seeking

### Generative Tests
Randomly generated EDLs to find edge cases:
- Random clip arrangements
- Random effect parameters
- Property: Any valid EDL should render without crashing
- Property: Output should be visually similar to reference

### Performance Tests
Benchmark edl2ffmpeg against reference renderer.

## Running Tests

### Command Line Options

```bash
./run_tests.sh [options]
  --quick          Run only quick smoke tests
  --approval       Run approval tests only
  --generative     Run generative property tests only
  --verbose        Enable verbose output
  --update-golden  Update golden reference files
  --seed <N>       Set random seed for generative tests
  --help           Show help message
```

### Using CMake/CTest

```bash
cd build
make test                    # Run all tests
make test_approval          # Run approval tests
make test_generative        # Run generative tests
make test_quick            # Run quick tests
make update_golden         # Update golden files
```

### Direct Test Execution

```bash
cd build/tests
./test_integration --help                           # Show Catch2 help
./test_integration "[approval]"                    # Run approval tests
./test_integration "[generative]" --rng-seed 12345 # Run with specific seed
./test_integration "Brightness*" -v high           # Run specific test verbose
```

## Debugging Failed Tests

### Reproducing Failures

When a generative test fails, it logs the seed:
```bash
# Reproduce with same seed
./test_integration "[generative]" --rng-seed 12345
```

### Inspecting Output

Failed test outputs are saved in:
- `tests/integration/generative/failures/` - Failed EDLs
- `/tmp/edl2ffmpeg_tests/` - Temporary render outputs

Keep temp files for inspection:
```bash
./test_integration --keep-temp-files
```

### Comparing Outputs Manually

```bash
# Generate both outputs
../../scripts/ftv_toffmpeg_wrapper_full.sh test.edl reference.mp4
../../build/edl2ffmpeg test.edl our.mp4

# Compare with FFmpeg
ffmpeg -i reference.mp4 -i our.mp4 -filter_complex "psnr" -f null -

# Visual comparison
ffplay reference.mp4  # In one terminal
ffplay our.mp4        # In another terminal
```

## Test Configuration

### Thresholds

- **PSNR > 35 dB**: Visually identical
- **PSNR > 30 dB**: Acceptable for effects
- **PSNR > 25 dB**: Acceptable for complex transforms
- **Frame difference < 5**: Maximum allowed frame number mismatch

### Test Fixtures

Generated test videos in `fixtures/`:
- Color bars at various resolutions/framerates
- Gradient patterns for effect testing
- Counter overlays for frame accuracy
- Noise patterns for compression testing

## Updating Golden Files

When intentionally changing rendering behavior:

```bash
# Review current differences
./run_tests.sh --approval

# Update golden files after verification
./run_tests.sh --approval --update-golden

# Or using CMake
cd build
make update_golden
```

## Writing New Tests

### Adding Approval Test

```cpp
TEST_CASE("New feature test", "[approval][feature]") {
    test::TestRunner runner;
    
    // Create EDL
    auto edl = test::templates::basicSingleClip("fixture.mp4", 5.0);
    
    // Run comparison
    auto result = runner.compareRenders(edl);
    
    // Check results
    REQUIRE(result.completed);
    CHECK(result.isVisuallyIdentical());
}
```

### Adding Generative Test

```cpp
TEST_CASE("Random feature test", "[generative]") {
    auto seed = GENERATE(take(10, random(0u, 1000000u)));
    CAPTURE(seed);
    
    test::EDLGenerator gen(seed);
    auto edl = gen.withYourFeature().generate();
    
    auto result = runner.compareRenders(edl);
    CHECK(result.avgPSNR > 30.0);
}
```

## Continuous Integration

Tests can be run in CI, but are currently configured for local execution only to avoid GitHub Actions costs. For CI setup, see `.github/workflows/` (not included).

## Troubleshooting

### Docker not running
```
Error: Docker is not running
```
Start Docker Desktop before running tests.

### Missing fixtures
```
Test fixtures not found
```
Run `./fixtures/generate_fixtures.sh`

### Reference renderer not found
```
Docker image 'ftv_toffmpeg:full' not found
```
Load the reference container image. See main README for instructions.

### Out of memory
Reduce test complexity or duration in generative tests.

### Tests hang
Check Docker Desktop resources. Increase CPU/memory allocation if needed.