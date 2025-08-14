#include "media/FFmpegDecoder.h"
#include "media/FFmpegCompat.h"
#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"
#include <stdexcept>
#include <algorithm>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
}

namespace media {

FFmpegDecoder::FFmpegDecoder(const std::string& filename) 
	: decoderConfig{} {
	openFile(filename);
	findVideoStream();
	setupDecoder();
}

FFmpegDecoder::FFmpegDecoder(const std::string& filename, const Config& config) 
	: decoderConfig(config) {
	openFile(filename);
	findVideoStream();
	setupDecoder();
}

FFmpegDecoder::~FFmpegDecoder() {
	cleanup();
}

FFmpegDecoder::FFmpegDecoder(FFmpegDecoder&& other) noexcept
	: formatCtx(other.formatCtx)
	, codecCtx(other.codecCtx)
	, packet(other.packet)
	, swsCtx(other.swsCtx)
	, videoStreamIndex(other.videoStreamIndex)
	, width(other.width)
	, height(other.height)
	, pixelFormat(other.pixelFormat)
	, frameRate(other.frameRate)
	, timeBase(other.timeBase)
	, totalFrames(other.totalFrames)
	, currentFrameNumber(other.currentFrameNumber)
	, framePool(std::move(other.framePool)) {
	
	other.formatCtx = nullptr;
	other.codecCtx = nullptr;
	other.packet = nullptr;
	other.swsCtx = nullptr;
}

FFmpegDecoder& FFmpegDecoder::operator=(FFmpegDecoder&& other) noexcept {
	if (this != &other) {
		cleanup();
		
		formatCtx = other.formatCtx;
		codecCtx = other.codecCtx;
		packet = other.packet;
		swsCtx = other.swsCtx;
		videoStreamIndex = other.videoStreamIndex;
		width = other.width;
		height = other.height;
		pixelFormat = other.pixelFormat;
		frameRate = other.frameRate;
		timeBase = other.timeBase;
		totalFrames = other.totalFrames;
		currentFrameNumber = other.currentFrameNumber;
		framePool = std::move(other.framePool);
		
		other.formatCtx = nullptr;
		other.codecCtx = nullptr;
		other.packet = nullptr;
		other.swsCtx = nullptr;
	}
	return *this;
}

void FFmpegDecoder::openFile(const std::string& filename) {
	int ret = avformat_open_input(&formatCtx, filename.c_str(), nullptr, nullptr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		throw std::runtime_error("Failed to open input file: " + std::string(errbuf));
	}
	
	ret = avformat_find_stream_info(formatCtx, nullptr);
	if (ret < 0) {
		throw std::runtime_error("Failed to find stream info");
	}
	
	packet = FFmpegCompat::allocPacket();
	if (!packet) {
		throw std::runtime_error("Failed to allocate packet");
	}
}

void FFmpegDecoder::findVideoStream() {
	for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
#if HAVE_CODECPAR_API
		if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
#else
		if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
#endif
			videoStreamIndex = i;
			break;
		}
	}
	
	if (videoStreamIndex == -1) {
		throw std::runtime_error("No video stream found");
	}
	
	AVStream* stream = formatCtx->streams[videoStreamIndex];
	timeBase = stream->time_base;
	frameRate = av_guess_frame_rate(formatCtx, stream, nullptr);
	
	// Calculate total frames
	if (stream->nb_frames > 0) {
		totalFrames = stream->nb_frames;
	} else if (stream->duration != AV_NOPTS_VALUE) {
		totalFrames = av_rescale_q(stream->duration, timeBase, av_inv_q(frameRate));
	} else if (formatCtx->duration != AV_NOPTS_VALUE) {
		totalFrames = formatCtx->duration * frameRate.num / (frameRate.den * AV_TIME_BASE);
	}
}

void FFmpegDecoder::setupDecoder() {
	AVStream* stream = formatCtx->streams[videoStreamIndex];
#if HAVE_CODECPAR_API
	AVCodecID codecId = stream->codecpar->codec_id;
#else
	AVCodecID codecId = stream->codec->codec_id;
#endif
	
	const AVCodec* codec = nullptr;
	std::string codecName;
	
	// Try hardware decoder if requested
	if (decoderConfig.useHardwareDecoder) {
		HWAccelType hwType = decoderConfig.hwConfig.type;
		if (hwType == HWAccelType::Auto) {
			hwType = HardwareAcceleration::getBestAccelType();
		}
		
		if (hwType != HWAccelType::None) {
			std::string hwDecoderName = HardwareAcceleration::getHWDecoderName(codecId, hwType);
			
			// For NVENC, use specific CUVID decoders
			if (!hwDecoderName.empty()) {
				codec = avcodec_find_decoder_by_name(hwDecoderName.c_str());
				if (codec) {
					codecName = hwDecoderName;
					usingHardware = true;
					utils::Logger::info("Using hardware decoder: {}", hwDecoderName);
				}
			} else if (hwType == HWAccelType::VAAPI || hwType == HWAccelType::VideoToolbox) {
				// VAAPI and VideoToolbox use standard decoders with hwaccel
				codec = avcodec_find_decoder(codecId);
				if (codec) {
					// Will set up hardware context after allocating codec context
					usingHardware = true;
					codecName = codec->name;
					utils::Logger::info("Using hardware acceleration with decoder: {}", codecName);
				}
			}
			
			// Create hardware device context
			if (usingHardware) {
				// Suppress FFmpeg error messages during hardware setup
				int oldLogLevel = av_log_get_level();
				av_log_set_level(AV_LOG_ERROR);
				
				hwDeviceCtx = HardwareAcceleration::createHWDeviceContext(hwType, decoderConfig.hwConfig.deviceIndex);
				
				// Restore log level
				av_log_set_level(oldLogLevel);
				
				if (!hwDeviceCtx) {
					utils::Logger::warn("Failed to create hardware device context, falling back to software");
					codec = nullptr;
					usingHardware = false;
				}
			}
		}
	}
	
	// Fall back to software decoder
	if (!codec) {
		codec = avcodec_find_decoder(codecId);
		if (!codec) {
			throw std::runtime_error("Codec not found");
		}
		codecName = codec->name;
	}
	
	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx) {
		throw std::runtime_error("Failed to allocate codec context");
	}
	
	int ret = FFmpegCompat::copyCodecParameters(codecCtx, stream);
	if (ret < 0) {
		throw std::runtime_error("Failed to copy codec parameters");
	}
	
	// Set up hardware acceleration context if needed
	if (usingHardware && hwDeviceCtx) {
		codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
		
		// For VAAPI and VideoToolbox, we need to get the hardware pixel format
		if (codecName.find("cuvid") == std::string::npos) {
			// Not using CUVID, need to find the hardware config
			for (int i = 0;; i++) {
				const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
				if (!config) {
					break;
				}
				
				if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
					HWAccelType hwType = decoderConfig.hwConfig.type == HWAccelType::Auto ? 
						HardwareAcceleration::getBestAccelType() : decoderConfig.hwConfig.type;
					AVPixelFormat expectedFormat = HardwareAcceleration::getHWPixelFormat(hwType);
					
					if (config->pix_fmt == expectedFormat) {
						codecCtx->pix_fmt = config->pix_fmt;
						break;
					}
				}
			}
		}
	}
	
	// Enable multi-threading for decoding
	// Use configured thread count or auto-detect
	codecCtx->thread_count = decoderConfig.threadCount; // 0 means auto-detect optimal thread count
	codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE; // Enable both frame and slice threading
	
	ret = avcodec_open2(codecCtx, codec, nullptr);
	if (ret < 0) {
		throw std::runtime_error("Failed to open codec");
	}
	
	width = codecCtx->width;
	height = codecCtx->height;
	pixelFormat = codecCtx->pix_fmt;
	
	// Initialize frame pool with software pixel format for hardware decoding
	// Hardware frames will be transferred to software frames in this format
	AVPixelFormat poolFormat = pixelFormat;
	if (usingHardware && HardwareAcceleration::isHardwarePixelFormat(pixelFormat)) {
		// Use YUV420P as the software format for frame pool
		poolFormat = AV_PIX_FMT_YUV420P;
		utils::Logger::debug("Hardware decoder using {} format, frame pool using software format {}", 
			av_get_pix_fmt_name(pixelFormat), av_get_pix_fmt_name(poolFormat));
	}
	
	// Initialize frame pool
	framePool = utils::FrameBufferPool(width, height, poolFormat);
	
	utils::Logger::info("Decoder initialized: {}x{} @ {} fps, threads: {}, hardware: {}",
		width, height, (double)frameRate.num / frameRate.den,
		codecCtx->thread_count == 0 ? "auto" : std::to_string(codecCtx->thread_count),
		usingHardware ? "yes" : "no");
}

void FFmpegDecoder::cleanup() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	
	if (codecCtx) {
		avcodec_free_context(&codecCtx);
	}
	
	if (hwDeviceCtx) {
		av_buffer_unref(&hwDeviceCtx);
	}
	
	if (packet) {
		FFmpegCompat::freePacket(&packet);
	}
	
	if (formatCtx) {
		avformat_close_input(&formatCtx);
	}
}

bool FFmpegDecoder::seekToFrame(int64_t frameNumber) {
	if (frameNumber < 0 || frameNumber >= totalFrames) {
		return false;
	}
	
	// If we're already at the exact frame, no need to seek
	if (currentFrameNumber == frameNumber) {
		return true;
	}
	
	// Check if we need to seek backward or if we're too far ahead
	// Only seek if we need to go backward or if we're more than 60 frames ahead
	// (seeking forward through 60+ frames is slower than seeking)
	if (currentFrameNumber > frameNumber || currentFrameNumber < frameNumber - 60) {
		// Seek to a keyframe before the target
		int64_t targetPts = frameNumberToPts(frameNumber);
		int ret = av_seek_frame(formatCtx, videoStreamIndex, targetPts,
			AVSEEK_FLAG_BACKWARD);
		
		if (ret < 0) {
			utils::Logger::error("Failed to seek to frame {}", frameNumber);
			return false;
		}
		
		// Flush codec buffers to clear decoder state
		FFmpegCompat::flushBuffers(codecCtx);
		
		// Clear any cached packets
		av_packet_unref(packet);
		
		// Reset frame position
		currentFrameNumber = -1;
	}
	
	// Decode frames until we reach the target
	auto tempFrame = makeAVFrame();
	while (currentFrameNumber < frameNumber - 1) {
		if (!decodeNextFrame(tempFrame.get())) {
			return false;
		}
	}
	
	return true;
}

std::shared_ptr<AVFrame> FFmpegDecoder::getFrame(int64_t frameNumber) {
	if (!seekToFrame(frameNumber)) {
		return nullptr;
	}
	
	auto frame = framePool.getFrame();
	if (!decodeNextFrame(frame.get())) {
		return nullptr;
	}
	
	return frame;
}

bool FFmpegDecoder::decodeNextFrame(AVFrame* frame) {
	// Use a temporary frame for hardware decoding
	AVFrame* decodedFrame = frame;
	AVFrame* hwFrame = nullptr;
	
	if (usingHardware) {
		hwFrame = av_frame_alloc();
		if (!hwFrame) {
			return false;
		}
		decodedFrame = hwFrame;
	}
	
	bool success = false;
	
	while (true) {
		int ret = av_read_frame(formatCtx, packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF) {
				// Try to flush decoder
				if (FFmpegCompat::decodeVideoFrame(codecCtx, decodedFrame, nullptr)) {
					currentFrameNumber++;
					success = true;
					break;
				}
			}
			break;
		}
		
		if (packet->stream_index != videoStreamIndex) {
			av_packet_unref(packet);
			continue;
		}
		
		if (FFmpegCompat::decodeVideoFrame(codecCtx, decodedFrame, packet)) {
			av_packet_unref(packet);
			currentFrameNumber++;
			success = true;
			break;
		}
		
		av_packet_unref(packet);
		// Continue to next packet if decoding failed or no frame ready
		continue;
	}
	
	// If using hardware decoding, transfer the frame to software
	if (success && usingHardware && hwFrame) {
		if (HardwareAcceleration::isHardwareFrame(hwFrame)) {
			int transferRet = HardwareAcceleration::transferHWFrameToSW(hwFrame, frame);
			if (transferRet < 0) {
				utils::Logger::error("Failed to transfer hardware frame to software");
				success = false;
			}
		} else {
			// Frame is already in software format (can happen with some decoders)
			av_frame_copy(frame, hwFrame);
			av_frame_copy_props(frame, hwFrame);
		}
	}
	
	if (hwFrame) {
		av_frame_free(&hwFrame);
	}
	
	return success;
}

int64_t FFmpegDecoder::ptsToFrameNumber(int64_t pts) const {
	if (pts == AV_NOPTS_VALUE) {
		return 0;
	}
	return av_rescale_q(pts, timeBase, av_inv_q(frameRate));
}

int64_t FFmpegDecoder::frameNumberToPts(int64_t frameNumber) const {
	return av_rescale_q(frameNumber, av_inv_q(frameRate), timeBase);
}

} // namespace media