#include "compositor/FrameCompositor.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace compositor {

FrameCompositor::FrameCompositor(int width, int height, AVPixelFormat format)
	: width(width)
	, height(height)
	, format(format)
	, outputPool(width, height, format) {
	
	// Allocate temporary buffer for effects processing
	tempBufferSize = av_image_get_buffer_size(format, width, height, 32);
	tempBuffer = std::make_unique<uint8_t[]>(tempBufferSize);
	
	utils::Logger::info("Frame compositor initialized: {}x{}, format: {}",
		width, height, format);
}

FrameCompositor::~FrameCompositor() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
	}
}

std::shared_ptr<AVFrame> FrameCompositor::processFrame(
	const std::shared_ptr<AVFrame>& input,
	const CompositorInstruction& instruction) {
	
	if (!input) {
		// Generate black frame if no input
		return generateColorFrame(0.0f, 0.0f, 0.0f);
	}
	
	// Get output frame from pool
	auto output = outputPool.getFrame();
	
	// For initial implementation, just copy/scale the input to output
	// Later: implement transforms, effects, etc.
	
	if (input->width != width || input->height != height ||
		input->format != format) {
		
		// Need to scale/convert
		if (!swsCtx) {
			swsCtx = sws_getContext(
				input->width, input->height, (AVPixelFormat)input->format,
				width, height, format,
				SWS_BILINEAR, nullptr, nullptr, nullptr);
			
			if (!swsCtx) {
				utils::Logger::error("Failed to create scaling context");
				return output;
			}
		}
		
		sws_scale(swsCtx,
			input->data, input->linesize, 0, input->height,
			output->data, output->linesize);
	} else {
		// Direct copy
		av_frame_copy(output.get(), input.get());
	}
	
	// Apply transformations and effects
	if (instruction.type == CompositorInstruction::DrawFrame) {
		// Apply fade
		if (instruction.fade < 1.0f) {
			applyFade(output.get(), instruction.fade);
		}
		
		// Apply effects
		if (!instruction.effects.empty()) {
			applyEffects(output.get(), instruction.effects);
		}
		
		// TODO: Apply geometric transforms (pan, zoom, rotation)
		// For now, these are placeholders
		if (std::abs(instruction.panX) > 0.001f ||
			std::abs(instruction.panY) > 0.001f ||
			std::abs(instruction.zoomX - 1.0f) > 0.001f ||
			std::abs(instruction.zoomY - 1.0f) > 0.001f ||
			std::abs(instruction.rotation) > 0.001f) {
			// applyTransform(output.get(), instruction);
			utils::Logger::debug("Transform requested but not yet implemented");
		}
	}
	
	return output;
}

std::shared_ptr<AVFrame> FrameCompositor::generateColorFrame(
	float r, float g, float b) {
	
	auto frame = outputPool.getFrame();
	fillWithColor(frame.get(), r, g, b);
	return frame;
}

void FrameCompositor::fillWithColor(AVFrame* frame, float r, float g, float b) {
	// Convert RGB to YUV for YUV formats
	if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV422P ||
		format == AV_PIX_FMT_YUV444P) {
		
		// Simple RGB to YUV conversion
		int y = static_cast<int>(0.299f * r * 255 + 0.587f * g * 255 + 0.114f * b * 255);
		int u = static_cast<int>(-0.147f * r * 255 - 0.289f * g * 255 + 0.436f * b * 255 + 128);
		int v = static_cast<int>(0.615f * r * 255 - 0.515f * g * 255 - 0.100f * b * 255 + 128);
		
		y = std::max(0, std::min(255, y));
		u = std::max(0, std::min(255, u));
		v = std::max(0, std::min(255, v));
		
		// Fill Y plane
		for (int row = 0; row < frame->height; ++row) {
			std::memset(frame->data[0] + row * frame->linesize[0], y, frame->width);
		}
		
		// Fill U and V planes (handle different subsampling)
		int chromaHeight = frame->height;
		int chromaWidth = frame->width;
		
		if (format == AV_PIX_FMT_YUV420P) {
			chromaHeight /= 2;
			chromaWidth /= 2;
		} else if (format == AV_PIX_FMT_YUV422P) {
			chromaWidth /= 2;
		}
		
		for (int row = 0; row < chromaHeight; ++row) {
			std::memset(frame->data[1] + row * frame->linesize[1], u, chromaWidth);
			std::memset(frame->data[2] + row * frame->linesize[2], v, chromaWidth);
		}
	} else if (format == AV_PIX_FMT_RGB24 || format == AV_PIX_FMT_BGR24) {
		// RGB format
		uint8_t rgb[3];
		if (format == AV_PIX_FMT_RGB24) {
			rgb[0] = static_cast<uint8_t>(r * 255);
			rgb[1] = static_cast<uint8_t>(g * 255);
			rgb[2] = static_cast<uint8_t>(b * 255);
		} else {
			rgb[0] = static_cast<uint8_t>(b * 255);
			rgb[1] = static_cast<uint8_t>(g * 255);
			rgb[2] = static_cast<uint8_t>(r * 255);
		}
		
		for (int row = 0; row < frame->height; ++row) {
			uint8_t* dst = frame->data[0] + row * frame->linesize[0];
			for (int col = 0; col < frame->width; ++col) {
				std::memcpy(dst + col * 3, rgb, 3);
			}
		}
	}
}

void FrameCompositor::applyFade(AVFrame* frame, float fade) {
	if (fade >= 1.0f) {
		return;
	}
	
	// Apply fade to Y plane (luminance)
	if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV422P ||
		format == AV_PIX_FMT_YUV444P) {
		
		for (int row = 0; row < frame->height; ++row) {
			uint8_t* data = frame->data[0] + row * frame->linesize[0];
			for (int col = 0; col < frame->width; ++col) {
				data[col] = static_cast<uint8_t>(data[col] * fade);
			}
		}
		
		// Optionally fade chroma planes towards neutral (128)
		// This creates a more natural fade to black
		int chromaHeight = frame->height;
		int chromaWidth = frame->width;
		
		if (format == AV_PIX_FMT_YUV420P) {
			chromaHeight /= 2;
			chromaWidth /= 2;
		} else if (format == AV_PIX_FMT_YUV422P) {
			chromaWidth /= 2;
		}
		
		for (int plane = 1; plane <= 2; ++plane) {
			for (int row = 0; row < chromaHeight; ++row) {
				uint8_t* data = frame->data[plane] + row * frame->linesize[plane];
				for (int col = 0; col < chromaWidth; ++col) {
					int value = data[col];
					value = 128 + static_cast<int>((value - 128) * fade);
					data[col] = static_cast<uint8_t>(std::max(0, std::min(255, value)));
				}
			}
		}
	}
}

void FrameCompositor::applyEffects(AVFrame* frame, const std::vector<Effect>& effects) {
	for (const auto& effect : effects) {
		switch (effect.type) {
			case Effect::Brightness:
				applyBrightness(frame, effect.strength);
				break;
			case Effect::Contrast:
				applyContrast(frame, effect.strength);
				break;
			case Effect::Saturation:
			case Effect::Blur:
			case Effect::Sharpen:
				// TODO: Implement these effects
				utils::Logger::debug("Effect not yet implemented");
				break;
		}
	}
}

void FrameCompositor::applyBrightness(AVFrame* frame, float strength) {
	// Brightness adjustment: add/subtract from luminance
	// strength: 0.5 = -50% brightness, 1.0 = normal, 1.5 = +50% brightness
	
	if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV422P ||
		format == AV_PIX_FMT_YUV444P) {
		
		int adjustment = static_cast<int>((strength - 1.0f) * 255);
		
		for (int row = 0; row < frame->height; ++row) {
			uint8_t* data = frame->data[0] + row * frame->linesize[0];
			for (int col = 0; col < frame->width; ++col) {
				int value = data[col] + adjustment;
				data[col] = static_cast<uint8_t>(std::max(0, std::min(255, value)));
			}
		}
	}
}

void FrameCompositor::applyContrast(AVFrame* frame, float strength) {
	// Contrast adjustment: scale distance from middle gray
	// strength: 0.5 = low contrast, 1.0 = normal, 1.5 = high contrast
	
	if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV422P ||
		format == AV_PIX_FMT_YUV444P) {
		
		const int midpoint = 128;
		
		for (int row = 0; row < frame->height; ++row) {
			uint8_t* data = frame->data[0] + row * frame->linesize[0];
			for (int col = 0; col < frame->width; ++col) {
				int value = data[col];
				value = midpoint + static_cast<int>((value - midpoint) * strength);
				data[col] = static_cast<uint8_t>(std::max(0, std::min(255, value)));
			}
		}
	}
}

void FrameCompositor::applyTransform(AVFrame* frame,
	const CompositorInstruction& instruction) {
	// TODO: Implement geometric transformations
	// This would involve:
	// 1. Creating transformation matrix from pan/zoom/rotation
	// 2. Applying inverse transform to sample from source pixels
	// 3. Using interpolation for sub-pixel accuracy
	// 4. Handling borders (black, repeat, mirror, etc.)
	
	// For now, this is a placeholder
	utils::Logger::debug("Geometric transform not yet implemented");
}

} // namespace compositor