# Test Fixture EDLs

This directory contains EDL files used for approval testing. These EDLs are designed to test specific features in isolation.

## Files

- `single_clip.json` - Basic single clip test
- `two_clips.json` - Two sequential clips
- `five_clips.json` - Five sequential clips
- `brightness_*.json` - Brightness effect at various levels
- `contrast_*.json` - Contrast effect at various levels
- `fade_*.json` - Fade in/out effects
- `complex_effects.json` - Multiple effects combined
- `framerate_*.json` - Frame rate conversion tests
- `resolution_*.json` - Resolution scaling tests
- `seek_*.json` - Frame-accurate seeking tests

## Generating Reference Output

To generate or update reference outputs for these EDLs:

```bash
# Generate reference output using ftv_toffmpeg
../../scripts/ftv_toffmpeg_wrapper_full.sh <edl_file> <output_file>

# Generate output using edl2ffmpeg
../../../build/edl2ffmpeg <edl_file> <output_file>

# Compare outputs
ffmpeg -i reference.mp4 -i our.mp4 -filter_complex "psnr" -f null -
```

## Updating Golden Files

To update the golden checksum files after verifying outputs are correct:

```bash
UPDATE_GOLDEN=1 ../../../build/tests/test_integration "[approval]"
```