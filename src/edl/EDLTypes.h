#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace edl {

struct Motion {
	float panX = 0.0f;     // -1 to 1
	float panY = 0.0f;     // -1 to 1
	float zoomX = 1.0f;    // zoom factor
	float zoomY = 1.0f;    // zoom factor
	float rotation = 0.0f; // degrees
};

struct Transition {
	std::string type;      // "dissolve", "wipe", etc.
	double duration = 0.0;
};

// Linear mapping for transfer functions
struct LinearMapping {
	float src = 0.0f;   // Input value (0.0 to 1.0)
	float dst = 0.0f;   // Output value (0.0 to 1.0)
};

// Bezier curve control for interpolation
struct BezierCurve {
	double srcTime = 0.0;
	double dstTime = 0.0;
};

// Control point for filters
struct FilterControlPoint {
	double point = 0.0;                        // Time offset in seconds
	std::vector<LinearMapping> linear;         // Linear transfer function
	std::optional<BezierCurve> bezier;        // Optional bezier curve
};

// Filter definition
struct Filter {
	std::string type;                          // "brightness", "saturation", etc.
	std::vector<FilterControlPoint> controlPoints;
};

// Shape control point for masks
struct ShapeControlPoint {
	double point = 0.0;      // Time offset
	float panx = 0.0f;       // Pan X (-1 to 1)
	float pany = 0.0f;       // Pan Y (-1 to 1)
	float zoomx = 1.0f;      // Zoom X factor
	float zoomy = 1.0f;      // Zoom Y factor
	float rotate = 0.0f;     // Rotation in degrees
	float shape = 1.0f;      // Shape parameter (1 = rectangle)
};

// Media source (existing)
struct MediaSource {
	std::string uri;       // URI/path to the media file (publishing EDL format)
	std::string trackId;   // "V1", "A1", etc.
	double in = 0.0;       // Source timecode in seconds
	double out = 0.0;      // Source timecode in seconds
	
	// Optional
	int width = 0;
	int height = 0;
	int fps = 0;
	float rotation = 0.0f;
	bool flip = false;
};

// Effect source (new)
struct EffectSource {
	std::string type;                           // "highlight", etc.
	double in = 0.0;                           // Start time
	double out = 0.0;                          // End time
	std::vector<Filter> insideMaskFilters;     // Filters inside mask
	std::vector<Filter> outsideMaskFilters;    // Filters outside mask
	std::vector<ShapeControlPoint> controlPoints; // Mask shape control
	std::string interpolation = "linear";       // Interpolation type
};

// Use variant to support both media and effect sources
using Source = std::variant<MediaSource, EffectSource>;

struct Track {
	enum Type { Video, Audio, Subtitle, Caption };
	Type type = Video;
	int number = 1;
	std::string subtype;   // "transform", "effects", etc.
	int subnumber = 0;
};

// Simple effect for inline clip effects (backward compatibility)
struct SimpleEffect {
	std::string type;       // "brightness", "contrast", etc.
	float strength = 1.0f;  // Simple strength value
};

struct Clip {
	double in = 0.0;       // Timeline position in seconds
	double out = 0.0;      // Timeline position in seconds
	Track track;
	Source source;
	
	// Optional
	float topFade = 0.0f;
	float tailFade = 0.0f;
	Motion motion;
	Transition transition;
	std::vector<SimpleEffect> effects;  // Simple inline effects (backward compat)
};

struct EDL {
	int fps = 30;
	int width = 1920;
	int height = 1080;
	std::vector<Clip> clips;
};

} // namespace edl