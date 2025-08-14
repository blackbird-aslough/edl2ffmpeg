#pragma once

#include <string>
#include <vector>

namespace compositor {

struct Effect {
	enum Type {
		Brightness,
		Contrast,
		Saturation,
		Blur,
		Sharpen
	};
	
	Type type;
	float strength = 1.0f;
	std::vector<float> parameters;
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