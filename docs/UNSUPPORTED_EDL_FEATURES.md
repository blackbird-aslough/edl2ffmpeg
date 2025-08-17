# Unsupported EDL Features

This document lists EDL format features that are defined in the reference parser but are not yet implemented in edl2ffmpeg.

## Currently Supported Features

### Basic Features
- ✅ Single source clips
- ✅ Sources array (single element only)
- ✅ Generate source type (black frames only)
- ✅ Basic video and audio tracks
- ✅ Effects tracks with basic brightness/contrast
- ✅ Track alignment with null clips
- ✅ Fade in/out (topFade, tailFade)
- ✅ Basic validation (in < out, required fields)
- ✅ FPS, width, height settings
- ✅ Publishing EDL format with 'uri' field

### Source Types
- ✅ Media sources (uri-based)
- ✅ Generate sources (black only)
- ✅ Effect sources (basic structure)
- ✅ Transform sources (basic)
- ✅ Subtitle sources

## Unsupported Features (Will Reject EDL)

The parser will explicitly reject EDLs containing these features with descriptive error messages:

### Source Types
- ❌ **Location sources** - External location references
- ❌ **Generate types other than black** - colour, testpattern, demo
- ❌ **Multiple sources in single clip** - Arrays with more than one source

### Motion and Animation
- ❌ **Motion bezier curves** - Complex speed control with bezier interpolation
- ❌ **Complex motion control** - Advanced motion parameters beyond basic pan/zoom

### Advanced Effects
- ❌ **Font/fonts in clips** - Text overlay fonts
- ❌ **Transition clips with sources** - Complex transitions between clips
- ❌ **Channel map with level != 1.0** - Audio level adjustments in channel mapping

### Track Types
- ❌ **Caption tracks** - Automatically skipped (like reference)

## Partially Supported Features

These features have basic support but not full implementation:

### Effects
- ⚠️ **Effect sources** - Structure is parsed but effects beyond brightness/contrast may not render correctly
- ⚠️ **Transform tracks** - Parsed but transform application is limited
- ⚠️ **Colour tracks** - Parsed but color correction is not implemented

### Transitions
- ⚠️ **Basic transitions** - Structure is parsed but only dissolve is implemented
- ⚠️ **Transition parameters** - Stored but not all are used

### Audio
- ⚠️ **Channel mapping** - Basic support, but only 1:1 mapping
- ⚠️ **Audio mix modes** - audiomix field is parsed but not implemented
- ⚠️ **Pan/level tracks** - Parsed but not applied

### Text
- ⚠️ **Text formatting** - TextFormat is parsed but rendering is basic
- ⚠️ **Subtitle/burnin tracks** - Parsed but not rendered

## Features Stored for Future Use

These features are parsed and stored but not actively used:

- 📦 YUV fade colors (topFadeYUV, tailFadeYUV)
- 📦 Sync groups (sync field)
- 📦 Track IDs (trackId for source selection)
- 📦 Gamma correction
- 📦 Speed factors
- 📦 Source dimensions (width/height in sources)
- 📦 FPS in sources

## Implementation Priority

For future development, these features should be prioritized:

1. **High Priority**
   - Multiple sources support (for concatenation)
   - Other generate types (colour, testpattern)
   - Complex transitions

2. **Medium Priority**
   - Motion bezier curves
   - Audio channel mapping with levels
   - Text overlay rendering

3. **Low Priority**
   - Location sources
   - Caption tracks
   - Advanced color correction

## Error Messages

When an unsupported feature is encountered, the parser provides specific error messages:

- "Location sources are not supported"
- "Generate type 'X' is not yet supported. Only 'black' is currently supported."
- "Multiple sources in a single clip are not yet supported"
- "Motion bezier curves are not supported"
- "Font/fonts in clips are not supported"
- "Transition clips with sources are not supported"
- "Channel map level must be 1.0 (other values not supported)"

## Testing Unsupported Features

To test that unsupported features are properly rejected, use the sample EDLs in `tests/sample_edls/unsupported/` directory.