#include "utils/FrameBuffer.h"
#include "utils/Logger.h"
#include <stdexcept>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace utils {

FrameBufferPool::FrameBufferPool(int width, int height, AVPixelFormat format,
	size_t poolSize)
	: width(width)
	, height(height)
	, format(format)
	, poolSize(poolSize) {
	
	// Don't pre-allocate frames - create them on demand
	Logger::debug("Frame buffer pool initialized: {}x{}, format: {}",
		width, height, format);
}

FrameBufferPool::~FrameBufferPool() {
	// Frames will be automatically cleaned up by shared_ptr destructors
}

FrameBufferPool::FrameBufferPool(FrameBufferPool&& other) noexcept
	: width(other.width)
	, height(other.height)
	, format(other.format)
	, poolSize(other.poolSize)
	, availableFrames(std::move(other.availableFrames))
	, totalAllocated(other.totalAllocated) {
	
	other.width = 0;
	other.height = 0;
	other.format = AV_PIX_FMT_NONE;
	other.totalAllocated = 0;
}

FrameBufferPool& FrameBufferPool::operator=(FrameBufferPool&& other) noexcept {
	if (this != &other) {
		width = other.width;
		height = other.height;
		format = other.format;
		poolSize = other.poolSize;
		availableFrames = std::move(other.availableFrames);
		totalAllocated = other.totalAllocated;
		
		other.width = 0;
		other.height = 0;
		other.format = AV_PIX_FMT_NONE;
		other.totalAllocated = 0;
	}
	return *this;
}

std::shared_ptr<AVFrame> FrameBufferPool::createFrame() {
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		throw std::runtime_error("Failed to allocate frame");
	}
	
	frame->format = format;
	frame->width = width;
	frame->height = height;
	
	int ret = av_frame_get_buffer(frame, 32); // 32-byte alignment for SIMD
	if (ret < 0) {
		av_frame_free(&frame);
		throw std::runtime_error("Failed to allocate frame buffer");
	}
	
	// Use the AVFrameDeleter from MediaTypes.h
	return std::shared_ptr<AVFrame>(frame, media::AVFrameDeleter());
}

std::shared_ptr<AVFrame> FrameBufferPool::getFrame() {
	std::lock_guard<std::mutex> lock(poolMutex);
	
	if (!availableFrames.empty()) {
		auto frame = availableFrames.front();
		availableFrames.pop();
		
		// Make frame writable
		av_frame_make_writable(frame.get());
		
		return frame;
	}
	
	// Create new frame
	totalAllocated++;
	if (totalAllocated > poolSize * 2) {
		Logger::warn("Frame buffer pool: allocated {} frames (pool size: {})",
			totalAllocated, poolSize);
	}
	
	return createFrame();
}

} // namespace utils