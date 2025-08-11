#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <memory>
#include <functional>

namespace media {

// Custom deleter for AVFrame
struct AVFrameDeleter {
	void operator()(AVFrame* frame) const {
		if (frame) {
			av_frame_free(&frame);
		}
	}
};

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

// Helper to create a managed AVFrame
inline AVFramePtr makeAVFrame() {
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		throw std::runtime_error("Failed to allocate AVFrame");
	}
	return AVFramePtr(frame);
}

} // namespace media