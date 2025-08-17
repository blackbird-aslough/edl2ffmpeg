#pragma once

#include "media/MediaTypes.h"
#include "media/HardwareAcceleration.h"
#include "utils/FrameBuffer.h"
#include <string>
#include <memory>

namespace media {

class FFmpegDecoder {
public:
	struct Config {
		int threadCount = 0;  // 0 = auto-detect, >0 = specific count
		
		// Hardware acceleration settings
		HWConfig hwConfig;
		bool useHardwareDecoder = false;  // Enable hardware decoding
		bool keepHardwareFrames = false;  // Keep frames on GPU (for passthrough)
	};
	
	FFmpegDecoder(const std::string& filename);
	FFmpegDecoder(const std::string& filename, const Config& config);
	~FFmpegDecoder();
	
	// Disable copy
	FFmpegDecoder(const FFmpegDecoder&) = delete;
	FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;
	
	// Enable move
	FFmpegDecoder(FFmpegDecoder&& other) noexcept;
	FFmpegDecoder& operator=(FFmpegDecoder&& other) noexcept;
	
	// Seek to specific frame number
	bool seekToFrame(int64_t frameNumber);
	
	// Get decoded frame
	std::shared_ptr<AVFrame> getFrame(int64_t frameNumber);
	
	// Get decoded frame without transfer to CPU (for GPU passthrough)
	std::shared_ptr<AVFrame> getHardwareFrame(int64_t frameNumber);
	
	// Media properties
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	AVPixelFormat getPixelFormat() const { return pixelFormat; }
	AVRational getFrameRate() const { return frameRate; }
	int64_t getTotalFrames() const { return totalFrames; }
	bool isUsingHardware() const { return usingHardware; }
	
private:
	void openFile(const std::string& filename);
	void findVideoStream();
	void setupDecoder();
	void cleanup();
	bool decodeNextFrame(AVFrame* frame);
	bool decodeNextHardwareFrame(AVFrame* frame);
	int64_t ptsToFrameNumber(int64_t pts) const;
	int64_t frameNumberToPts(int64_t frameNumber) const;
	
	AVFormatContext* formatCtx = nullptr;
	AVCodecContext* codecCtx = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* swsCtx = nullptr;
	
	// Hardware acceleration members
	AVBufferRef* hwDeviceCtx = nullptr;
	bool usingHardware = false;
	
	// SwsContext for format conversion (hardware decoding)
	int lastSrcFormat = -1;
	int lastDstFormat = -1;
	int lastWidth = 0;
	int lastHeight = 0;
	
	int videoStreamIndex = -1;
	int width = 0;
	int height = 0;
	AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
	AVRational frameRate = {0, 1};
	AVRational timeBase = {0, 1};
	int64_t totalFrames = 0;
	int64_t currentFrameNumber = -1;
	
	Config decoderConfig;
	utils::FrameBufferPool framePool;
};

} // namespace media