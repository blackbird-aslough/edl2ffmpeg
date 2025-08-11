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
```json
{
	"type": string,       // Effect type (e.g., "highlight")
	"in": number,
	"out": number,
	"insideMaskFilters": [...],   // Filters inside mask
	"outsideMaskFilters": [...]   // Filters outside mask
}
```

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

Filters modify color/brightness:

```json
{
	"type": "brightness" | "saturation" | "white" | "y" | "u" | "v",

	// For constant filters:
	"linear": [
		{"src": number, "dst": number}
	],

	// For white filter:
	"u": number,
	"v": number,

	// For varying filters:
	"controlPoints": [...]
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

## TypeScript Types

Full TypeScript type definitions are available in `types/edl.d.ts`.