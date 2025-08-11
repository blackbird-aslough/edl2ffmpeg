#pragma once

#include "media/MediaTypes.h"
#include <queue>
#include <mutex>
#include <memory>

namespace utils {

class FrameBufferPool {
public:
	FrameBufferPool() = default;
	FrameBufferPool(int width, int height, AVPixelFormat format,
		size_t poolSize = 10);
	
	~FrameBufferPool();
	
	// Move operations
	FrameBufferPool(FrameBufferPool&& other) noexcept;
	FrameBufferPool& operator=(FrameBufferPool&& other) noexcept;
	
	// Delete copy operations
	FrameBufferPool(const FrameBufferPool&) = delete;
	FrameBufferPool& operator=(const FrameBufferPool&) = delete;
	
	std::shared_ptr<AVFrame> getFrame();
	
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	AVPixelFormat getFormat() const { return format; }
	
private:
	std::shared_ptr<AVFrame> createFrame();
	
	int width = 0;
	int height = 0;
	AVPixelFormat format = AV_PIX_FMT_NONE;
	size_t poolSize = 10;
	
	std::queue<std::shared_ptr<AVFrame>> availableFrames;
	std::mutex poolMutex;
	size_t totalAllocated = 0;
};

} // namespace utils