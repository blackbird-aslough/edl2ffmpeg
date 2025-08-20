#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <map>
#include <stdexcept>

namespace edl {

// Exceptions for EDL parsing errors
class InvalidEdlException : public std::runtime_error {
public:
	explicit InvalidEdlException(const std::string& message)
		: std::runtime_error("Invalid EDL: " + message) {}
};

struct Motion {
	float panX = 0.0f;     // -1 to 1
	float panY = 0.0f;     // -1 to 1
	float zoomX = 1.0f;    // zoom factor
	float zoomY = 1.0f;    // zoom factor
	float rotation = 0.0f; // degrees
	double offset = 0.0;   // Motion offset in seconds
	double duration = 0.0; // Motion duration in seconds
};

struct Transition {
	std::string type;      // "dissolve", "wipe", etc.
	double duration = 0.0;
	std::map<std::string, std::variant<bool, int, double, std::string>> parameters;
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

// Media source (from file/URI)
struct MediaSource {
	std::string uri;       // URI/path to the media file (publishing EDL format)
	double in = 0.0;       // Source timecode in seconds
	double out = 0.0;      // Source timecode in seconds
	
	// Optional
	std::string trackId;   // "V1", "A1", etc.
	int width = 0;
	int height = 0;
	int fps = 0;
	float speed = 1.0f;    // Speed factor
	float gamma = 1.0f;    // Gamma correction
	std::string audiomix;  // Audio mix mode ("avg" etc.)
};

// Generate source (for black frames, test patterns, etc.)
struct GenerateSource {
	enum Type {
		Black,
		Colour,
		TestPattern,
		Demo
	};
	
	Type type = Black;
	double in = 0.0;       // Start time
	double out = 0.0;      // End time
	int width = 1920;      // Required for generated sources
	int height = 1080;     // Required for generated sources
	
	// Type-specific parameters
	std::map<std::string, std::variant<int, float, std::string>> parameters;
};

// Location source (reference to external location)
struct LocationSource {
	std::string id;        // Location identifier
	std::string type;      // Location type
	double in = 0.0;
	double out = 0.0;
	std::map<std::string, std::variant<int, float, std::string>> parameters;
};

// Effect source (for effects tracks)
struct EffectSource {
	std::string type;                           // "brightness", "contrast", "highlight", etc.
	double in = 0.0;                           // Start time
	double out = 0.0;                          // End time
	
	// Effect-specific fields (stored as generic data for flexibility)
	// Common fields: value (for brightness/contrast), filters, controlPoints
	std::map<std::string, std::variant<double, std::string, std::vector<Filter>, std::vector<ShapeControlPoint>>> data;
};

// Transform source (for transform/pan/level tracks)
struct TransformSource {
	double in = 0.0;
	double out = 0.0;
	std::vector<ShapeControlPoint> controlPoints;
};

// Subtitle source
struct SubtitleSource {
	std::string text;
	double in = 0.0;
	double out = 0.0;
};

// Use variant to support all source types
using Source = std::variant<MediaSource, GenerateSource, LocationSource, EffectSource, TransformSource, SubtitleSource>;

struct Track {
	enum Type { Video, Audio, Subtitle, Caption, Burnin };
	Type type = Video;
	int number = 1;
	std::string subtype;   // "transform", "effects", "colour", "pan", "level", etc.
	int subnumber = 1;     // Default to 1 per reference
};

// Text formatting for subtitles/burnin
struct TextFormat {
	std::string font = "";
	int fontSize = 24;
	std::string halign = "middle";  // "left", "middle", "right"
	std::string valign = "bottom";  // "top", "middle", "bottom"
	std::string textAYUV = "FFFFFF";
	std::string backAYUV = "000000";
};

// Simple effect for inline clip effects (backward compatibility)
struct SimpleEffect {
	std::string type;       // "brightness", "contrast", etc.
	float strength = 1.0f;  // Simple strength value
};

// Null clip for track alignment
struct NullClip {
	double duration = 0.0;
	
	explicit NullClip(double d) : duration(d) {}
};

struct Clip {
	double in = 0.0;       // Timeline position in seconds
	double out = 0.0;      // Timeline position in seconds
	Track track;
	
	// Either single source or multiple sources (but we only support single element arrays for now)
	std::optional<Source> source;
	std::vector<Source> sources;
	
	// Optional fields
	float topFade = 0.0f;
	float tailFade = 0.0f;
	std::string topFadeYUV;   // YUV color for fade in
	std::string tailFadeYUV;  // YUV color for fade out
	Motion motion;
	std::optional<Transition> transition;
	std::optional<TextFormat> textFormat;
	std::map<int, double> channelMap;  // Audio channel mapping
	int sync = 0;             // Sync group

	
	// Internal use
	bool isNullClip = false;  // True if this is a null clip for alignment
};

struct EDL {
	int fps = 30;
	int width = 1920;
	int height = 1080;
	std::vector<Clip> clips;
	
	// Track management (internal use)
	std::map<std::string, std::vector<Clip>> tracks;  // Organized by track key
	std::map<std::string, std::string> fxAppliesTo;   // Maps fx tracks to their parent tracks
};

} // namespace edl