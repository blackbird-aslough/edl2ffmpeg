#include "utils/FrameBuffer.h"
#include "utils/Logger.h"
#include <stdexcept>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace utils {

// Custom deleter that returns frames to the pool
class FramePoolDeleter {
public:
	FramePoolDeleter(FrameBufferPool* pool) : pool_(pool) {}
	
	void operator()(AVFrame* frame) const {
		if (frame && pool_) {
			pool_->returnFrame(frame);
		}
	}
	
private:
	FrameBufferPool* pool_;
};

FrameBufferPool::FrameBufferPool(int width, int height, AVPixelFormat format,
	size_t poolSize)
	: width(width)
	, height(height)
	, format(format)
	, poolSize(poolSize) {
	
	// Pre-allocate some frames to avoid initial allocation overhead
	if (width > 0 && height > 0 && format != AV_PIX_FMT_NONE) {
		std::lock_guard<std::mutex> lock(poolMutex);
		for (size_t i = 0; i < std::min(poolSize / 2, size_t(5)); ++i) {
			availableFrames.push(createFrame());
		}
		Logger::debug("Frame buffer pool initialized: {}x{}, format: {}, pre-allocated: {}",
			width, height, format, availableFrames.size());
	}
}

FrameBufferPool::~FrameBufferPool() {
	std::lock_guard<std::mutex> lock(poolMutex);
	// Clean up all frames in the pool
	while (!availableFrames.empty()) {
		auto frame = availableFrames.front();
		availableFrames.pop();
		if (frame) {
			av_frame_free(&frame);
		}
	}
	Logger::debug("Frame buffer pool destroyed, cleaned up {} frames", totalAllocated);
}

FrameBufferPool::FrameBufferPool(FrameBufferPool&& other) noexcept {
	std::lock_guard<std::mutex> lock(other.poolMutex);
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

FrameBufferPool& FrameBufferPool::operator=(FrameBufferPool&& other) noexcept {
	if (this != &other) {
		std::lock_guard<std::mutex> lock1(poolMutex);
		std::lock_guard<std::mutex> lock2(other.poolMutex);
		
		// Clean up existing frames
		while (!availableFrames.empty()) {
			auto frame = availableFrames.front();
			availableFrames.pop();
			if (frame) {
				av_frame_free(&frame);
			}
		}
		
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

AVFrame* FrameBufferPool::createFrame() {
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
	return frame;
}

std::shared_ptr<AVFrame> FrameBufferPool::getFrame() {
	std::lock_guard<std::mutex> lock(poolMutex);
	
	AVFrame* frame = nullptr;
	
	if (!availableFrames.empty()) {
		// Reuse frame from pool
		frame = availableFrames.front();
		availableFrames.pop();
		
		// Make frame writable and reset
		av_frame_make_writable(frame);
	} else {
		// Create new frame if pool is empty
		frame = createFrame();
		
		if (totalAllocated > poolSize * 2) {
			static size_t warnCount = 0;
			if (warnCount++ < 5) { // Only warn first 5 times
				Logger::warn("Frame buffer pool: allocated {} frames (pool size: {})",
					totalAllocated, poolSize);
			}
		}
	}
	
	// Return frame with custom deleter that returns it to the pool
	return std::shared_ptr<AVFrame>(frame, FramePoolDeleter(this));
}

void FrameBufferPool::returnFrame(AVFrame* frame) {
	if (!frame) {
		return;
	}
	
	std::lock_guard<std::mutex> lock(poolMutex);
	
	// Only return to pool if we haven't exceeded the maximum
	if (availableFrames.size() < poolSize) {
		// Don't call av_frame_unref as it releases the buffer
		// Just reset the essential metadata for reuse
		frame->pts = 0;
		frame->pkt_dts = 0;
		frame->duration = 0;
		frame->flags = 0;
		frame->pict_type = AV_PICTURE_TYPE_NONE;
		frame->sample_aspect_ratio.num = 0;
		frame->sample_aspect_ratio.den = 1;
		frame->crop_top = 0;
		frame->crop_bottom = 0;
		frame->crop_left = 0;
		frame->crop_right = 0;
		
		// Return to pool
		availableFrames.push(frame);
	} else {
		// Pool is full, free the frame
		av_frame_free(&frame);
		totalAllocated--;
	}
}

} // namespace utils