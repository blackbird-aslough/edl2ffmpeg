#pragma once

#include "media/MediaTypes.h"
#include "media/HardwareAcceleration.h"
#include <string>

namespace media {

class FFmpegEncoder {
public:
	struct Config {
		std::string codec = "libx264";
		int bitrate = 446464;  // 436Ki (436 * 1024) - matching ftv_toffmpeg default
		AVPixelFormat pixelFormat = AV_PIX_FMT_YUV420P;
		int width = 1920;
		int height = 1080;
		AVRational frameRate = {30, 1};
		std::string preset = "faster";  // matching ftv_toffmpeg default
		int crf = 23;  // Constant Rate Factor for x264/x265
		int threadCount = 0;  // 0 = auto-detect, >0 = specific count
		
		// Hardware acceleration settings
		HWConfig hwConfig;
		bool useHardwareEncoder = false;  // Enable hardware encoding
	};
	
	FFmpegEncoder(const std::string& filename, const Config& config);
	~FFmpegEncoder();
	
	// Disable copy
	FFmpegEncoder(const FFmpegEncoder&) = delete;
	FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;
	
	// Enable move
	FFmpegEncoder(FFmpegEncoder&& other) noexcept;
	FFmpegEncoder& operator=(FFmpegEncoder&& other) noexcept;
	
	bool writeFrame(AVFrame* frame);
	bool finalize();
	
	int64_t getFrameCount() const { return frameCount; }
	
private:
	void setupEncoder(const std::string& filename, const Config& config);
	void cleanup();
	bool encodeFrame(AVFrame* frame);
	bool flushEncoder();
	
	AVFormatContext* formatCtx = nullptr;
	AVCodecContext* codecCtx = nullptr;
	AVStream* videoStream = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* swsCtx = nullptr;
	AVFrame* convertedFrame = nullptr;
	
	// Hardware acceleration members
	AVBufferRef* hwDeviceCtx = nullptr;
	AVFrame* hwFrame = nullptr;
	bool usingHardware = false;
	
	Config config;
	int64_t frameCount = 0;
	int64_t pts = 0;
	bool finalized = false;
};

} // namespace media