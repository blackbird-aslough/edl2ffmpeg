#pragma once

#include "compositor/CompositorInstruction.h"
#include "media/MediaTypes.h"
#include "utils/FrameBuffer.h"
#include <memory>

namespace compositor {

class FrameCompositor {
public:
	FrameCompositor(int width, int height, AVPixelFormat format);
	~FrameCompositor();
	
	// Process single frame with instruction
	std::shared_ptr<AVFrame> processFrame(
		const std::shared_ptr<AVFrame>& input,
		const CompositorInstruction& instruction
	);
	
	// Generate a color frame
	std::shared_ptr<AVFrame> generateColorFrame(
		float r, float g, float b
	);
	
private:
	void applyTransform(AVFrame* frame, const CompositorInstruction& instruction);
	void applyFade(AVFrame* frame, float fade);
	void applyEffects(AVFrame* frame, const std::vector<Effect>& effects);
	void applyBrightness(AVFrame* frame, float strength);
	void applyBrightnessLinear(AVFrame* frame, const std::vector<LinearMapping>& mapping);
	void applyBrightnessLUT(AVFrame* frame, const uint8_t* lut);
	void applyContrast(AVFrame* frame, float strength);
	void fillWithColor(AVFrame* frame, float r, float g, float b);
	float linearInterpolate(float input, const std::vector<LinearMapping>& mapping);
	void buildBrightnessLUT(uint8_t* lut, const std::vector<LinearMapping>& mapping);
	
	int width;
	int height;
	AVPixelFormat format;
	utils::FrameBufferPool outputPool;
	SwsContext* swsCtx = nullptr;
	
	// Temporary buffers for effects
	std::unique_ptr<uint8_t[]> tempBuffer;
	size_t tempBufferSize = 0;
};

} // namespace compositor