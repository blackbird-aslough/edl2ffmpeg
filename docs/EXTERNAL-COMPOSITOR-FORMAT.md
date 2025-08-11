# External Compositor Data Format

This document describes the data structure passed from videolib to external compositors (WebGL and WebGPU) for rendering video frames.

## Overview

When videolib needs to render a frame using an external compositor, it passes:
1. An array of `IExportImageParameters` objects describing each layer to be composited
2. A `IDisplayApi` object providing access to raw image data and utility functions

## Primary Data Structure: IExportImageParameters

Each element in the image composition data array represents a layer to be rendered. Layers are composited in order (first layer is bottom/background).

### Core Properties

```typescript
interface IExportImageParameters {
	// Layer identification
	track: number;              // Track number (1-based, used as layer ID)

	// Image type and source
	type: "generated" | "tiled" | "bb10" | "yuv";
	// - "generated": Solid color or generated content
	// - "tiled": Large/zoomed image split into tiles
	// - "bb10": Blackbird-encoded video format (WebGPU only)
	// - "yuv": Standard YUV image data

	// Dimensions
	width?: number;             // Width of source image
	height?: number;            // Height of source image

	// Alpha channel
	alpha?: boolean;            // Whether alpha channel data is available

	// Transform properties
	panx?: number;              // Pan X position (-1 to 1, 0 = center)
	pany?: number;              // Pan Y position (-1 to 1, 0 = center)
	zoomx?: number;             // Zoom X factor (1 = no zoom)
	zoomy?: number;             // Zoom Y factor (1 = no zoom)
	rotate?: number;            // Rotation in degrees
	flip?: boolean;             // Horizontal flip

	// Fade effects
	fade?: number;              // Fade level (0 = transparent, 1 = opaque)
	fadeYUV?: string;           // Fade color in hex YUV format

	// Color correction
	colourMap?: boolean;        // Whether color map data is available

	// Effects and transitions
	imageEffects?: any[];       // Array of image effects to apply
	transition?: any;           // Transition definition (for transitions between clips)
	transitionProgress?: number; // Transition progress (0 to 1)

	// Tiled image specific (type="tiled")
	zoomFactor?: number;        // Zoom level as power of 2 (e.g., 2 = 2x zoom)
	tilesX?: number;            // Number of tiles horizontally
	tilesY?: number;            // Number of tiles vertically
	sourceOffsetX?: number;     // X offset of top-left tile
	sourceOffsetY?: number;     // Y offset of top-left tile

	// Blackbird format specific (type="bb10", WebGPU only)
	keySparsity?: number;       // Frames between keyframes
	frameId?: number;           // Frame ID within video
	videoId?: number;           // Video source ID
}
```

## Display API Interface

The `IDisplayApi` object provides methods to retrieve actual pixel data:

```typescript
interface IDisplayApi {
	display: number;            // Display handle
	videolib: IVideolib;        // Videolib instance

	// Get raw image data for a track
	getImageData(track: number, alpha: boolean, transition: boolean): Uint8Array;
	getImageDataLength(track: number, alpha: boolean, transition: boolean): number;

	// Get color correction maps (256 entries each for Y, U, V channels)
	getColourMap(track: number, transition: boolean, component: "Y" | "U" | "V"): Uint8Array;

	// Get tiled data arrays (for type="tiled")
	getTiledDataArray(track: number, alpha: boolean, transition: boolean): (Uint8Array|null)[];

	// WebGPU specific - model and tiled data
	getModelDataArray(track: number, alpha: boolean, transition: boolean): (Uint8Array|null)[];
}
```

## Data Formats

### YUV Image Data (type="yuv")

For standard YUV images, `getImageData()` returns a Uint8Array with:
- First 8 bytes: Header information
- Remaining bytes: YUV data in planar format
  - Y plane: width × height bytes
  - UV plane: (width × height) / 2 bytes (interleaved U and V)

The data may be padded to multiples of 4 for both width and height.

### Tiled Image Data (type="tiled")

For zoomed/tiled images, `getTiledDataArray()` returns an array where:
- Array length = tilesX × tilesY
- Each element is either null (tile not needed) or Uint8Array with tile data
- Tiles are ordered: row 0 left-to-right, then row 1, etc.
- Each tile has the base image dimensions (width × height)

### Generated Content (type="generated")

For generated content, additional properties are included:
- For solid colors: `generate.type = "colour"`, with `generate.yuv` (YUV color string) and optional `generate.alpha`

### Blackbird Format (type="bb10", WebGPU only)

Compressed video format with:
- `keySparsity`: Number of frames between keyframes
- `frameId`: Current frame ID
- `videoId`: Source video identifier

## Image Effects

The `imageEffects` array contains effect definitions that can include:

### Transform Effects
```javascript
{
	type: "transform",
	mask: {
		panx: number,
		pany: number,
		zoomx: number,
		zoomy: number,
		rotate: number
	}
}
```

### Crop Effects
```javascript
{
	type: "crop",
	// Crop parameters
}
```

### Film/Video Effects
```javascript
{
	type: "film" | "lut" | "vignette" | "blur" | "mosaic",
	// Effect-specific parameters
}
```

## Transitions

When transitioning between two clips, the last track will have:
- `transition`: Object describing the transition type and parameters
- `transitionProgress`: Number from 0 to 1 indicating transition completion

Common transition types:
- `dissolve`: Cross-fade between clips
- `wipe`: Various wipe patterns
- Custom transition types with specific parameters

## Rendering Process

1. **WebGL Compositor**:
   - Converts YUV data to RGB textures
   - Applies transforms (pan, zoom, rotate, flip)
   - Applies effects in sequence
   - Composites layers with alpha blending
   - Handles transitions between clips

2. **WebGPU Compositor**:
   - Decodes Blackbird-compressed frames
   - Manages reference frame caching
   - Performs GPU-accelerated decompression
   - Applies transforms and effects
   - Outputs to canvas

## Example Data Structure

```javascript
[
	{
		track: 1,
		type: "yuv",
		width: 1920,
		height: 1080,
		alpha: false,
		zoomx: 1.2,
		zoomy: 1.2,
		panx: 0.1,
		rotate: 5
	},
	{
		track: 2,
		type: "generated",
		generate: {
			type: "colour",
			yuv: "0x108080",
			alpha: 0.5
		},
		fade: 0.7
	}
]
```

## Notes

- Track numbers are 1-based for external use (internally 0-based)
- Coordinate system: pan values range from -1 to 1, with (0,0) at center
- Transforms are applied in order: scale, rotate, then translate
- Multiple tracks can reference the same underlying media
- The `sync` property (in EDL) links related tracks but is not passed to compositors