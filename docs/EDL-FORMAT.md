# EDL (Edit Decision List) Format Documentation

The EDL format used by edl2ffmpeg is a JSON-based structure that describes video editing instructions, including clips, transitions, effects, and audio tracks. This format is based on the Blackbird publishing EDL format.

## Basic Structure

```json
{
	"fps": number,        // Frame rate (e.g., 30)
	"width": number,      // Video width in pixels
	"height": number,     // Video height in pixels
	"clips": [...]        // Array of clip objects
}
```

## Clip Structure

Each clip in the `clips` array must have:

```json
{
	"in": number,         // Start time in seconds (required)
	"out": number,        // End time in seconds (required) 
	"track": {...},       // Track definition (required)
	"source": {...}       // Source definition (or "sources" array) (required)
}
```

Optional clip properties:
- `sync`: Sync ID to link related clips
- `topFade`: Fade in duration (seconds)
- `tailFade`: Fade out duration (seconds)
- `topFadeYUV`: Fade in color (6-digit hex YUV, video only)
- `tailFadeYUV`: Fade out color (6-digit hex YUV, video only)
- `motion`: Motion control object
- `transition`: Transition definition
- `channelMap`: Audio channel mapping (audio only)
- `font`/`fonts`: Font specification (video only)
- `textFormat`: Text formatting (subtitle/burnin only)

## Track Types

Tracks define where the clip appears:

```json
{
	"type": string,       // Required: "video", "audio", "subtitle", "burnin", "caption"
	"number": number,     // Required: Track number (positive integer, 1-based)
	"subtype": string,    // Optional: "effects", "transform", "colour", "level", "pan"
	"subnumber": number   // Optional: Ordering for effects subtracks (default: 1)
}
```

**Notes:**
- `caption` tracks are ignored by the parser
- `subtype` defines the type of effect/transform track:
  - Video subtypes: `"effects"`, `"transform"`, `"colour"`
  - Audio subtypes: `"level"`, `"pan"`
  - Subtitle/burnin subtypes: `"transform"`
- `subnumber` is only used for `"effects"` subtracks to determine application order
- If `subnumber` is specified, `subtype` must also be specified

## Source Types

Sources define what content to play. Each source must have `in` and `out` times, plus one of `uri`, `location`, or `generate`:

### Media Source (uri)
```json
{
	"uri": string,        // Path/URI to media file
	"in": number,         // Start time in source (seconds, >= 0)
	"out": number,        // End time in source (seconds, > in)
	
	// Optional:
	"width": number,      // Source width
	"height": number,     // Source height  
	"fps": number,        // Source frame rate
	"speed": number,      // Speed factor (> 0)
	"gamma": number,      // Gamma correction (video only, >= 0)
	"trackId": string,    // Track within media ("V1", "A1", etc.)
	"audiomix": "avg"     // Mix all audio channels (audio only)
}
```

**Notes:**
- URIs without a scheme are treated as local files relative to the EDL location
- `trackId` format: "V1" for video track 1, "A1" for audio track 1, etc.
- `audiomix: "avg"` mixes all audio channels; cannot be used with `trackId`

### Location Source
```json
{
	"location": {
		"id": string,         // Location identifier
		"type": string,       // Location type
		// Additional location-specific properties
	},
	"in": number,
	"out": number
}
```

### Generated Source
```json
{
	"generate": {
		"type": string,       // "black", "colour", "demo", "testpattern"
		// Additional type-specific properties
	},
	"in": number,
	"out": number,
	"width": number,      // Required for generated sources
	"height": number      // Required for generated sources
}
```

### Text Source (Subtitles/Burnin)
```json
{
	"text": string,       // Text content (or null for gap)
	"in": number,
	"out": number
}
```

### Transform Source (Pan/Zoom/Transform Tracks)
```json
{
	"in": number,
	"out": number,
	"controlPoints": [    // Optional for transform tracks
		{
			"point": number,    // Time offset from start
			"panx": number,     // Pan X (-1 to 1)
			"pany": number,     // Pan Y (-1 to 1)
			"zoomx": number,    // Zoom X factor
			"zoomy": number,    // Zoom Y factor
			"rotate": number,   // Rotation in degrees
			"shape": number     // Shape parameter
		}
	]
	// Additional properties passed through as-is
}
```

### Colour Source (Colour Correction Tracks)
```json
{
	"in": number,
	"out": number,
	"filters": [...],     // Required: Array of filter objects
	"signedUV": boolean   // Optional, currently unused
}
```

### Effects Source (Effects Subtracks)

For clips on effects subtracks, the source contains effect-specific properties:

```json
{
	"in": number,         // Start time (can be negative)
	"out": number,        // End time (can be negative if reversed)
	// Effect-specific properties vary by effect type
}
```

**Note:** The parser removes `in` and `out` from the source and passes the rest as the effect descriptor.

## Audio Control Points

For audio level and panning:

```json
{
	"controlPoints": [
		{
			"point": number,      // Time offset
			"pan": number,        // Pan position (-1 to 1)
			"db": "-Infinity" | number,  // Volume in decibels
			"bezier": [...]       // Optional bezier curves
		}
	]
}
```

## Filters

Filters modify color/brightness and are typically used within effect sources:

### Brightness Filter

The brightness filter uses a linear transfer function to map input luminance to output luminance:

```json
{
	"type": "brightness",
	"controlPoints": [
		{
			"point": number,       // Time offset (0.0 = start of effect)
			"linear": [
				{"src": 0.0, "dst": number},  // Maps black input to output level
				{"src": 1.0, "dst": number}   // Maps white input to output level
			]
		}
	]
}
```

**Brightness mapping examples:**
- Normal (no change): `[{"src": 0.0, "dst": 0.0}, {"src": 1.0, "dst": 1.0}]`
- 50% brightness increase: `[{"src": 0.0, "dst": 0.5}, {"src": 1.0, "dst": 1.0}]`
- 20% brightness increase: `[{"src": 0.0, "dst": 0.2}, {"src": 1.0, "dst": 1.0}]`
- Inverted: `[{"src": 0.0, "dst": 1.0}, {"src": 1.0, "dst": 0.0}]`
- 50% dimmer: `[{"src": 0.0, "dst": 0.0}, {"src": 1.0, "dst": 0.5}]`

### Other Filters

```json
{
	"type": "saturation" | "white" | "y" | "u" | "v",

	// For white filter:
	"u": number,
	"v": number,

	// For varying filters:
	"controlPoints": [...],

	// For constant filters:
	"linear": [
		{"src": number, "dst": number}
	]
}
```

## Text Formatting

For subtitle/caption tracks:

```json
{
	"textFormat": {
		"font": string,
		"fontSize": number,
		"textAYUV": string,     // Text color in AYUV format
		"backAYUV": string,     // Background color
		"leftMargin": number,
		"rightMargin": number,
		"topMargin": number,
		"bottomMargin": number,
		"halign": "left" | "middle" | "right",
		"valign": "top" | "middle" | "bottom",
		"background": "ragged" | "rectangle" | "wide",
		"lineBreaks": "auto" | "manual"
	}
}
```

## Transitions

Transitions are specified within the clip they transition from:

```json
{
	"transition": {
		"source": {...} or "sources": [...],  // Source(s) for transition
		"type": string,         // Transition type (video only)
		"invert": boolean,      // Invert transition (video only)
		"points": number,       // Number of points (video only)
		"xsquares": number,     // Grid squares (video only)
		"channelMap": {...},    // Audio channel mapping
		"sync": number,         // Sync ID
		"font": string or       // Font specification
		"fonts": [...]          // Multiple fonts
	}
}
```

**Note:** Transitions create additional tracks internally (e.g., "transition video", "audio transition 0")

## Motion Effects

Timing control for effects:

```json
{
	"motion": {
		"offset": number,       // Time offset
		"duration": number,     // Effect duration
		"bezier": [            // Bezier curve control
			{
				"srcTime": number,
				"dstTime": number
			}
		]
	}
}
```

## Brightness Effect Example

A complete example applying a 50% brightness increase to a video clip:

```json
{
	"fps": 30,
	"width": 1920,
	"height": 1080,
	"clips": [
		{
			"in": 0.0,
			"out": 5.0,
			"track": {
				"type": "video",
				"number": 1,
				"subtype": "effects",
				"subnumber": 1
			},
			"source": {
				"type": "highlight",
				"in": 0.0,
				"out": 5.0,
				"insideMaskFilters": [
					{
						"type": "brightness",
						"controlPoints": [
							{
								"point": 0.0,
								"linear": [
									{"src": 0.0, "dst": 0.5},
									{"src": 1.0, "dst": 1.0}
								]
							}
						]
					}
				],
				"outsideMaskFilters": [],
				"interpolation": "linear"
			}
		},
		{
			"in": 0.0,
			"out": 5.0,
			"track": {
				"type": "video",
				"number": 1
			},
			"source": {
				"uri": "video.mp4",
				"trackId": "V1",
				"in": 0.0,
				"out": 5.0
			}
		}
	]
}
```

## Complete Example

A comprehensive EDL with video, audio, effects, and transforms:

```json
{
	"fps": 30,
	"width": 1920,
	"height": 1080,
	"clips": [
		{
			"in": 0,
			"out": 10,
			"track": {"type": "video", "number": 1},
			"source": {
				"uri": "media/video1.mp4",
				"in": 5,
				"out": 15,
				"trackId": "V1"
			},
			"topFade": 1.0,
			"tailFade": 0.5,
			"topFadeYUV": "808080",
			"tailFadeYUV": "000000"
		},
		{
			"in": 0,
			"out": 10,
			"track": {"type": "audio", "number": 1},
			"source": {
				"uri": "media/video1.mp4",
				"in": 5,
				"out": 15,
				"trackId": "A1"
			},
			"topFade": 1.0,
			"tailFade": 0.5
		},
		{
			"in": 2,
			"out": 8,
			"track": {"type": "subtitle", "number": 1},
			"source": {
				"text": "Example Subtitle Text",
				"in": 0,
				"out": 6
			},
			"textFormat": {
				"font": "Arial",
				"fontSize": 24,
				"halign": "middle",
				"valign": "bottom"
			}
		},
		{
			"in": 0,
			"out": 10,
			"track": {
				"type": "video",
				"number": 1,
				"subtype": "transform"
			},
			"source": {
				"in": 0,
				"out": 10,
				"controlPoints": [
					{
						"point": 0,
						"panx": 0,
						"pany": 0,
						"zoomx": 1.0,
						"zoomy": 1.0,
						"rotate": 0,
						"shape": 1
					},
					{
						"point": 5,
						"panx": 0.5,
						"pany": 0,
						"zoomx": 2.0,
						"zoomy": 2.0,
						"rotate": 45,
						"shape": 1
					}
				]
			}
		},
		{
			"in": 0,
			"out": 10,
			"track": {
				"type": "video",
				"number": 1,
				"subtype": "effects",
				"subnumber": 1
			},
			"source": {
				"in": 0,
				"out": 10,
				"type": "brightness",
				"strength": 1.5
			}
		},
		{
			"in": 0,
			"out": 5,
			"track": {
				"type": "audio",
				"number": 1,
				"subtype": "level"
			},
			"source": {
				"in": 0,
				"out": 5,
				"controlPoints": [
					{"point": 0, "db": 0},
					{"point": 2.5, "db": -6},
					{"point": 5, "db": "-Infinity"}
				]
			}
		}
	]
}
```

## Notes

### Key Implementation Details (from reference parser):

- All times are in seconds (floating point)
- Track numbers start at 1
- In point must be before out point (except for reversible intervals in effects)
- Clips on the same track cannot overlap in time
- The `sync` property links related clips (e.g., video and audio from same source)
- Color values use 6-digit hex YUV format
- Transform coordinates use normalized values (-1 to 1 for pan)
- FPS must evenly divide into the quantum rate (typically values like 24, 25, 30, 50, 60)
- Caption tracks are silently ignored
- Effects tracks are internally renamed to fx0, fx1, etc. based on subnumber order

### Track Key Mapping:

The parser converts track definitions to internal track keys:
- Video track 1 → "video"
- Video track 1, transform → "videopanzoomlevel"  
- Video track 1, colour → "colour"
- Video track 1, effects → "fx0", "fx1", etc. (ordered by subnumber)
- Video track 2+ → "overlay0", "overlay1", etc.
- Audio track 1 → "audio"
- Audio track 1, level → "audiolevel"
- Audio track 1, pan → "audiopan"
- Audio track 2+ → "audio1", "audio2", etc.
- Subtitle track N → "subtitle0", "subtitle1", etc.
- Burnin track N → "burnin0", "burnin1", etc.

### Parser Validation:

The parser enforces:
- Required keys must be present
- Data types must match expected types
- No unknown keys in strictly defined objects
- Frame alignment and duration consistency
- Source interval validity
- Track continuity (gaps are filled with null clips)

## Implementation Status in edl2ffmpeg

**Currently Implemented:**
- Basic video/audio clip playback
- Fade in/out transitions
- Simple brightness and contrast adjustments (applied directly to clips)

### Working Brightness Example for Current Implementation

The current edl2ffmpeg uses a simplified effects model where brightness is applied directly to clips:

```json
{
	"fps": 30,
	"width": 1920,
	"height": 1080,
	"clips": [
		{
			"in": 0.0,
			"out": 5.0,
			"track": {
				"type": "video",
				"number": 1
			},
			"source": {
				"uri": "Test_Live_1906_High.mxf",
				"trackId": "V1",
				"in": 0.0,
				"out": 5.0
			},
			"effects": [
				{
					"type": "brightness",
					"strength": 1.5  // 1.0 = normal, 1.5 = +50% brightness
				}
			]
		}
	]
}
```

**Note:** The current implementation uses a `strength` value where:
- 1.0 = normal brightness
- 1.5 = +50% brightness 
- 0.5 = -50% brightness

This is different from the linear transfer function model described above, which will be implemented in future releases.

**Not Yet Implemented:**
- Effects sources with type "highlight"
- Linear transfer function brightness model
- Mask-based filtering (insideMaskFilters/outsideMaskFilters)
- Complex control point interpolation
- Most transition types
- Text/subtitle tracks

## TypeScript Types

Full TypeScript type definitions are available in `types/edl.d.ts`.