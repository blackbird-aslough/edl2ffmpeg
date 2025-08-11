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
	
	if (width > 0 && height > 0 && format != AV_PIX_FMT_NONE) {
		initializePool();
	}
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

void FrameBufferPool::initializePool() {
	std::lock_guard<std::mutex> lock(poolMutex);
	
	// Pre-allocate frames
	for (size_t i = 0; i < poolSize / 2; ++i) {
		availableFrames.push(createFrame());
	}
	
	Logger::debug("Frame buffer pool initialized: {}x{}, format: {}, pre-allocated: {}",
		width, height, format, poolSize / 2);
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
	
	totalAllocated++;
	
	// Use custom deleter to return frame to pool
	return std::shared_ptr<AVFrame>(frame,
		[this](AVFrame* f) {
			if (f) {
				returnFrame(f);
			}
		});
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
	
	// Allocate new frame if pool is empty
	if (totalAllocated < poolSize * 2) {
		return createFrame();
	}
	
	// Pool exhausted, create frame anyway but log warning
	Logger::warn("Frame buffer pool exhausted, allocating beyond limit");
	return createFrame();
}

void FrameBufferPool::returnFrame(AVFrame* frame) {
	if (!frame) {
		return;
	}
	
	std::lock_guard<std::mutex> lock(poolMutex);
	
	// Only return to pool if we haven't exceeded the maximum
	if (availableFrames.size() < poolSize) {
		// Clear frame references but keep the buffer allocated
		av_frame_unref(frame);
		
		// Re-setup frame parameters
		frame->format = format;
		frame->width = width;
		frame->height = height;
		
		// Note: We're storing a raw pointer in shared_ptr with custom deleter
		// This is a simplified approach - in production, we'd need more careful management
		availableFrames.push(std::shared_ptr<AVFrame>(frame,
			[](AVFrame* f) {
				// Don't delete - will be managed by pool
			}));
	} else {
		// Pool is full, free the frame
		av_frame_free(&frame);
		totalAllocated--;
	}
}

} // namespace utils