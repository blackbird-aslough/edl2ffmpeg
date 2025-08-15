# EDL (Edit Decision List) Format Documentation

The EDL format used by videolib is a JSON-based structure that describes video editing instructions, including clips, transitions, effects, and audio tracks.

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

Each clip in the `clips` array has:

```json
{
	"in": number,         // Start time in seconds
	"out": number,        // End time in seconds
	"track": {...},       // Track definition
	"sync": number,       // Optional sync ID
	"source": {...},      // Source definition (or "sources" array)

	// Optional properties:
	"topFade": number,    // Fade in duration
	"tailFade": number,   // Fade out duration
	"motion": {...},      // Motion effects
	"transition": {...},  // Transition definition
	"channelMap": {...},  // Audio channel mapping
	"font": string        // Font specification
}
```

## Track Types

Tracks define where the clip appears:

```json
{
	"type": "video" | "audio" | "subtitle" | "caption",
	"number": number,     // Track number (1-based)
	"subtype": string,    // Optional: "transform", "effects", etc.
	"subnumber": number   // Optional: ordering for subtracks
}
```

## Source Types

Sources define what content to play:

### Media Source (Video/Audio)
```json
{
	"mediaId": string,    // Media file identifier
	"trackId": string,    // Track within media (e.g., "V1", "A1")
	"in": number,         // Start time in source
	"out": number,        // End time in source

	// Optional:
	"width": number,
	"height": number,
	"fps": number,
	"rotate": number,     // Rotation in degrees
	"flip": boolean       // Horizontal flip
}
```

### Generated Source
```json
{
	"generate": {
		"type": "black" | "colour" | "demo",
		// Additional type-specific properties
	},
	"in": number,
	"out": number
}
```

### Text Source (Subtitles/Captions)
```json
{
	"text": string,       // Text content
	"in": number,
	"out": number
}
```

### Transform Source (Pan/Zoom)
```json
{
	"controlPoints": [
		{
			"point": number,    // Time offset
			"panx": number,     // Pan X (-1 to 1)
			"pany": number,     // Pan Y (-1 to 1)
			"zoomx": number,    // Zoom X factor
			"zoomy": number,    // Zoom Y factor
			"rotate": number,   // Rotation
			"shape": number     // Shape parameter
		}
	],
	"in": number,
	"out": number
}
```

### Effects Source

Effects sources apply visual effects like brightness, contrast, or color adjustments. They can apply filters either to the entire frame or within/outside a mask region:

```json
{
	"type": string,       // Effect type (e.g., "highlight")
	"in": number,         // Start time in source
	"out": number,        // End time in source
	"insideMaskFilters": [...],   // Filters applied inside mask region
	"outsideMaskFilters": [...],  // Filters applied outside mask region
	
	// Optional mask/shape control:
	"controlPoints": [    // Define mask shape and position over time
		{
			"point": number,       // Time offset
			"panx": number,        // Pan X (-1 to 1)
			"pany": number,        // Pan Y (-1 to 1)
			"zoomx": number,       // Zoom X factor
			"zoomy": number,       // Zoom Y factor
			"rotate": number,      // Rotation in degrees
			"shape": number        // Shape parameter (1 = rectangle)
		}
	],
	"interpolation": "linear"  // How to interpolate between control points
}
```

**Note:** When applying effects to the entire frame without masking, use empty `controlPoints` or omit mask-related properties. The filters in `insideMaskFilters` will apply to the entire frame.

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

Transitions between clips:

```json
{
	"transition": {
		"source": {...},        // Second source for transition
		"sync": number,         // Sync ID for second source
		"type": string,         // Transition type
		"invert": boolean,      // Invert transition direction
		"points": number,       // Number of points
		"xsquares": number,     // Grid squares for certain transitions
		"channelMap": {...}     // Audio channel mapping
	}
}
```

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

A simple EDL with video, audio, subtitles, and zoom effect:

```json
{
	"fps": 30,
	"width": 640,
	"height": 360,
	"clips": [
		{
			"source": {
				"mediaId": "video1",
				"trackId": "V1",
				"in": 0,
				"out": 10
			},
			"in": 0,
			"out": 10,
			"track": {"type": "video", "number": 1}
		},
		{
			"source": {
				"mediaId": "video1",
				"trackId": "A1",
				"in": 0,
				"out": 10
			},
			"in": 0,
			"out": 10,
			"track": {"type": "audio", "number": 1}
		},
		{
			"source": {
				"text": "Hello World",
				"in": 0,
				"out": 5
			},
			"in": 2,
			"out": 7,
			"track": {"type": "subtitle", "number": 1},
			"textFormat": {
				"fontSize": 24,
				"halign": "middle",
				"valign": "bottom"
			}
		},
		{
			"source": {
				"controlPoints": [
					{"point": 0, "zoom": 1},
					{"point": 3, "zoom": 2}
				],
				"in": 0,
				"out": 3
			},
			"in": 5,
			"out": 8,
			"track": {
				"type": "video",
				"number": 1,
				"subtype": "transform"
			}
		}
	]
}
```

## Notes

- All times are in seconds (floating point)
- Track numbers start at 1
- Multiple clips can reference the same media source
- Clips on the same track cannot overlap in time
- The `sync` property links related clips (e.g., video and audio from same source)
- Color values use AYUV format (Alpha, Y luminance, U and V chrominance)
- Transform coordinates use normalized values (-1 to 1 for pan)

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