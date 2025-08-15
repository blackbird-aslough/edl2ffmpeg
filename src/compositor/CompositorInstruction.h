#pragma once

#include <string>
#include <vector>

namespace compositor {

// Linear mapping for transfer functions (brightness, etc.)
struct LinearMapping {
	float src = 0.0f;   // Input value (0.0 to 1.0)
	float dst = 0.0f;   // Output value (0.0 to 1.0)
};

struct Effect {
	enum Type {
		Brightness,
		Contrast,
		Saturation,
		Blur,
		Sharpen
	};
	
	Type type;
	float strength = 1.0f;  // For simple effects (backward compatibility)
	std::vector<float> parameters;
	
	// For linear transfer function effects
	std::vector<LinearMapping> linearMapping;  // Transfer function
	bool useLinearMapping = false;             // Whether to use linear mapping
};

struct TransitionInfo {
	enum Type {
		None,
		Dissolve,
		Wipe,
		Slide
	};
	
	Type type = None;
	float duration = 0.0f;
	float progress = 0.0f;  // 0.0 to 1.0
};

struct CompositorInstruction {
	enum Type {
		DrawFrame,
		GenerateColor,
		NoOp,
		Transition
	};
	
	Type type = DrawFrame;
	
	// Source information
	int trackNumber = 0;
	std::string uri;
	int64_t sourceFrameNumber = 0;
	
	// Transform parameters
	float panX = 0.0f;      // -1 to 1
	float panY = 0.0f;      // -1 to 1
	float zoomX = 1.0f;     // zoom factors
	float zoomY = 1.0f;     // zoom factors
	float rotation = 0.0f;  // degrees
	bool flip = false;
	
	// Effects
	float fade = 1.0f;      // 0-1 (opacity)
	std::vector<Effect> effects;
	
	// Transition (if applicable)
	TransitionInfo transition;
	
	// For color generation
	struct {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
	} color;
};

} // namespace compositor