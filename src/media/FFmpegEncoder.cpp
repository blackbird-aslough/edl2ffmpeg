#include "media/FFmpegEncoder.h"
#include "media/FFmpegCompat.h"
#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"
#include <stdexcept>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#if HAVE_HWDEVICE_API
#include <libavutil/hwcontext.h>
#endif
}

namespace media {

FFmpegEncoder::FFmpegEncoder(const std::string& filename, const Config& config)
	: config(config) {
	setupEncoder(filename, config);
}

FFmpegEncoder::~FFmpegEncoder() {
	if (!finalized) {
		finalize();
	}
	cleanup();
}

FFmpegEncoder::FFmpegEncoder(FFmpegEncoder&& other) noexcept
	: formatCtx(other.formatCtx)
	, codecCtx(other.codecCtx)
	, videoStream(other.videoStream)
	, packet(other.packet)
	, swsCtx(other.swsCtx)
	, hwDeviceCtx(other.hwDeviceCtx)
	, hwFrame(other.hwFrame)
	, usingHardware(other.usingHardware)
	, convertedFrame(other.convertedFrame)
	, config(other.config)
	, frameCount(other.frameCount)
	, pts(other.pts)
	, finalized(other.finalized) {
	
	other.formatCtx = nullptr;
	other.codecCtx = nullptr;
	other.videoStream = nullptr;
	other.packet = nullptr;
	other.swsCtx = nullptr;
	other.hwDeviceCtx = nullptr;
	other.hwFrame = nullptr;
	other.usingHardware = false;
	other.convertedFrame = nullptr;
}

FFmpegEncoder& FFmpegEncoder::operator=(FFmpegEncoder&& other) noexcept {
	if (this != &other) {
		cleanup();
		
		formatCtx = other.formatCtx;
		codecCtx = other.codecCtx;
		videoStream = other.videoStream;
		packet = other.packet;
		swsCtx = other.swsCtx;
		hwDeviceCtx = other.hwDeviceCtx;
		hwFrame = other.hwFrame;
		usingHardware = other.usingHardware;
		convertedFrame = other.convertedFrame;
		config = other.config;
		frameCount = other.frameCount;
		pts = other.pts;
		finalized = other.finalized;
		
		other.formatCtx = nullptr;
		other.codecCtx = nullptr;
		other.videoStream = nullptr;
		other.packet = nullptr;
		other.swsCtx = nullptr;
		other.hwDeviceCtx = nullptr;
		other.hwFrame = nullptr;
		other.usingHardware = false;
		other.convertedFrame = nullptr;
	}
	return *this;
}

void FFmpegEncoder::setupEncoder(const std::string& filename, const Config& config) {
	int ret;
	
	// Allocate output format context
	ret = avformat_alloc_output_context2(&formatCtx, nullptr, nullptr, filename.c_str());
	if (ret < 0 || !formatCtx) {
		throw std::runtime_error("Failed to allocate output context");
	}
	
	// Determine codec to use
	const AVCodec* codec = nullptr;
	std::string codecName = config.codec;
	AVCodecID codecId = AV_CODEC_ID_NONE;
	
	// Map codec names to IDs
	if (config.codec == "libx264" || config.codec == "h264") {
		codecId = AV_CODEC_ID_H264;
	} else if (config.codec == "libx265" || config.codec == "hevc") {
		codecId = AV_CODEC_ID_HEVC;
	}
	
	// Try hardware encoder if requested
	if (config.useHardwareEncoder && codecId != AV_CODEC_ID_NONE) {
		HWAccelType hwType = config.hwConfig.type;
		if (hwType == HWAccelType::Auto) {
			hwType = HardwareAcceleration::getBestAccelType();
		}
		
		if (hwType != HWAccelType::None) {
			std::string hwCodecName = HardwareAcceleration::getHWEncoderName(codecId, hwType);
			if (!hwCodecName.empty()) {
				codec = avcodec_find_encoder_by_name(hwCodecName.c_str());
				if (codec) {
					codecName = hwCodecName;
					usingHardware = true;
					utils::Logger::info("Using hardware encoder: {}", hwCodecName);
					
					// Create hardware device context
					// Suppress FFmpeg error messages during hardware setup
					int oldLogLevel = av_log_get_level();
					av_log_set_level(AV_LOG_ERROR);
					
					hwDeviceCtx = HardwareAcceleration::createHWDeviceContext(hwType, config.hwConfig.deviceIndex);
					
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
	}
	
	// Fall back to software encoder
	if (!codec) {
		codec = avcodec_find_encoder_by_name(config.codec.c_str());
		if (!codec && codecId != AV_CODEC_ID_NONE) {
			codec = avcodec_find_encoder(codecId);
		}
		
		if (!codec) {
			throw std::runtime_error("Codec not found: " + config.codec);
		}
		codecName = config.codec;
	}
	
	// Create new video stream
	videoStream = avformat_new_stream(formatCtx, nullptr);
	if (!videoStream) {
		throw std::runtime_error("Failed to create video stream");
	}
	
	// Allocate codec context
	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx) {
		throw std::runtime_error("Failed to allocate codec context");
	}
	
	// Set codec parameters
	codecCtx->codec_id = codec->id;
	codecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	codecCtx->width = config.width;
	codecCtx->height = config.height;
	
	// Set pixel format based on hardware acceleration
	if (usingHardware && hwDeviceCtx) {
		// Set hardware device context
#if HAVE_HWDEVICE_API
		codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
#endif
		
		// Use hardware pixel format
		HWAccelType hwType = config.hwConfig.type == HWAccelType::Auto ? 
			HardwareAcceleration::getBestAccelType() : config.hwConfig.type;
		AVPixelFormat hwPixFmt = HardwareAcceleration::getHWPixelFormat(hwType);
		
		// For hardware encoders, we need to set up frames context
		// This allows direct GPU-to-GPU transfer
#if HAVE_HWDEVICE_API
		AVBufferRef* hwFramesRef = av_hwframe_ctx_alloc(hwDeviceCtx);
		if (hwFramesRef) {
			AVHWFramesContext* hwFramesCtx = (AVHWFramesContext*)hwFramesRef->data;
			hwFramesCtx->format = hwPixFmt;
			hwFramesCtx->sw_format = config.pixelFormat;
			hwFramesCtx->width = config.width;
			hwFramesCtx->height = config.height;
			hwFramesCtx->initial_pool_size = 20;
			
			if (av_hwframe_ctx_init(hwFramesRef) >= 0) {
				codecCtx->hw_frames_ctx = hwFramesRef;
			} else {
				av_buffer_unref(&hwFramesRef);
			}
		}
#endif
		
		codecCtx->pix_fmt = hwPixFmt;
#if HAVE_HWDEVICE_API
		codecCtx->sw_pix_fmt = config.pixelFormat;
#endif
	} else {
		codecCtx->pix_fmt = config.pixelFormat;
	}
	
	codecCtx->time_base = av_inv_q(config.frameRate);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
	codecCtx->framerate = config.frameRate;
#endif
	codecCtx->bit_rate = config.bitrate;
	codecCtx->gop_size = 300; // 300 frames GOP - matching ftv_toffmpeg default
	
	// Configure B-frames based on encoder type
	// VideoToolbox with hardware frames has PTS/DTS synchronization issues with B-frames
	// when using GPU passthrough mode, so we disable B-frames for hardware encoding
	if (codecName.find("videotoolbox") != std::string::npos && usingHardware) {
		// Disable B-frames for hardware VideoToolbox to avoid PTS/DTS ordering issues
		// in GPU passthrough mode. Software VideoToolbox can still use B-frames.
		codecCtx->max_b_frames = 0;
		utils::Logger::debug("Disabling B-frames for hardware VideoToolbox encoder to ensure PTS/DTS compatibility");
	} else {
		// Don't explicitly set max_b_frames - let FFmpeg use its defaults to match ftv_toffmpeg
		// The reference encoder (ftv_toffmpeg) doesn't set this, allowing FFmpeg to choose
		// codecCtx->max_b_frames = 2;  // Commented out to match reference behavior
		utils::Logger::debug("Using FFmpeg default B-frame settings for encoder: {}", codecName);
		
		// For software VideoToolbox, ensure proper B-frame configuration
		if (codecName.find("videotoolbox") != std::string::npos) {
			// Still don't set these - let FFmpeg handle it
			// codecCtx->has_b_frames = 2;
			// codecCtx->delay = codecCtx->max_b_frames;
		}
	}
	
	// Set aspect ratio for libx264/libx265 - matching ftv_toffmpeg behavior
	// ftv_toffmpeg sets -aspect width:height for these codecs
	if (codecName == "libx264" || codecName == "libx265") {
		// Set display aspect ratio (DAR) as width:height
		// The sample aspect ratio (SAR) should be calculated from DAR and resolution
		// DAR = SAR * width/height, so SAR = DAR * height/width
		// For 1920x1080 with 16:9 DAR, SAR should be 1:1 (square pixels)
		codecCtx->sample_aspect_ratio = av_make_q(1, 1);
	} else {
		// For other codecs, use default (unset) aspect ratio
		codecCtx->sample_aspect_ratio = av_make_q(0, 1);
	}
	
	// Set stream time base
	videoStream->time_base = codecCtx->time_base;
	
	// Enable multi-threading for encoding - matching ftv_toffmpeg which uses -threads 0
	// Use configured thread count or auto-detect
	codecCtx->thread_count = config.threadCount; // 0 means auto-detect optimal thread count
	// Don't set thread_type - let FFmpeg use its defaults to match ftv_toffmpeg
	// codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE; // Commented out to match reference
	
	// Set codec-specific options
	if (codecName == "libx264" || codecName == "libx265") {
		av_opt_set(codecCtx->priv_data, "preset", config.preset.c_str(), 0);
		
		// Use CRF mode only if explicitly requested (crf >= 0 and bitrate <= 0)
		// Otherwise use bitrate mode to match ftv_toffmpeg behavior
		if (config.crf >= 0 && config.bitrate <= 0) {
			// CRF mode - constant quality
			av_opt_set_int(codecCtx->priv_data, "crf", config.crf, 0);
			codecCtx->bit_rate = 0; // Disable bitrate when using CRF
		} else {
			// Bitrate mode - set bitrate tolerance (-bt option in ftv_toffmpeg)
			codecCtx->bit_rate_tolerance = config.bitrate;
		}
	} else if (codecName.find("nvenc") != std::string::npos) {
		// NVENC specific options
		av_opt_set(codecCtx->priv_data, "preset", "p4", 0); // Balanced preset
		av_opt_set(codecCtx->priv_data, "rc", "vbr", 0); // Variable bitrate
		av_opt_set(codecCtx->priv_data, "spatial-aq", "1", 0); // Spatial AQ
		av_opt_set(codecCtx->priv_data, "temporal-aq", "1", 0); // Temporal AQ
		av_opt_set(codecCtx->priv_data, "lookahead", "32", 0); // Look-ahead frames
		
		if (config.crf >= 0 && config.bitrate <= 0) {
			av_opt_set(codecCtx->priv_data, "rc", "constqp", 0);
			av_opt_set_int(codecCtx->priv_data, "qp", config.crf, 0);
			codecCtx->bit_rate = 0;
		}
	} else if (codecName.find("vaapi") != std::string::npos) {
		// VAAPI specific options
		av_opt_set_int(codecCtx->priv_data, "quality", 25, 0); // Quality level
		av_opt_set(codecCtx->priv_data, "rc_mode", "VBR", 0); // Variable bitrate
		
		if (config.crf >= 0 && config.bitrate <= 0) {
			av_opt_set(codecCtx->priv_data, "rc_mode", "CQP", 0);
			av_opt_set_int(codecCtx->priv_data, "qp", config.crf, 0);
			codecCtx->bit_rate = 0;
		}
	} else if (codecName.find("videotoolbox") != std::string::npos) {
		// VideoToolbox specific options
		av_opt_set(codecCtx->priv_data, "profile", "main", 0);
		av_opt_set_int(codecCtx->priv_data, "allow_sw", 1, 0); // Allow software fallback
		
		if (config.crf >= 0 && config.bitrate <= 0) {
			// VideoToolbox uses quality scale 0.0-1.0 (lower is better)
			double quality = 1.0 - (config.crf / 51.0); // Convert CRF to quality
			av_opt_set_double(codecCtx->priv_data, "quality", quality, 0);
			codecCtx->bit_rate = 0;
		}
	}
	
	// Some formats want stream headers to be separate
	if (formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 60, 100)
		codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		codecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif
	}
	
	// Log actual B-frames setting before opening codec
	utils::Logger::debug("Before avcodec_open2 - max_b_frames: {}", codecCtx->max_b_frames);
	
	// Open codec
	ret = avcodec_open2(codecCtx, codec, nullptr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		throw std::runtime_error("Failed to open codec: " + std::string(errbuf));
	}
	
	// Copy codec parameters to stream
	ret = FFmpegCompat::copyCodecParametersToStream(videoStream, codecCtx);
	if (ret < 0) {
		throw std::runtime_error("Failed to copy codec parameters");
	}
	
	// Open output file
	if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&formatCtx->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			throw std::runtime_error("Failed to open output file");
		}
	}
	
	// Write file header
	ret = avformat_write_header(formatCtx, nullptr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		throw std::runtime_error("Failed to write header: " + std::string(errbuf));
	}
	
	// Allocate packet
	packet = FFmpegCompat::allocPacket();
	if (!packet) {
		throw std::runtime_error("Failed to allocate packet");
	}
	
	// Allocate frame for conversion if needed
	convertedFrame = av_frame_alloc();
	if (!convertedFrame) {
		throw std::runtime_error("Failed to allocate conversion frame");
	}
	
	convertedFrame->format = config.pixelFormat;
	convertedFrame->width = config.width;
	convertedFrame->height = config.height;
	
	ret = av_frame_get_buffer(convertedFrame, 32);
	if (ret < 0) {
		throw std::runtime_error("Failed to allocate conversion frame buffer");
	}
	
	utils::Logger::info("Encoder initialized: {}x{} @ {} fps, codec: {}, threads: {}, hardware: {}, max_b_frames: {}",
		config.width, config.height,
		(double)config.frameRate.num / config.frameRate.den,
		codecName,
		codecCtx->thread_count == 0 ? "auto" : std::to_string(codecCtx->thread_count),
		usingHardware ? "yes" : "no",
		codecCtx->max_b_frames);
}

void FFmpegEncoder::cleanup() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	
	if (convertedFrame) {
		av_frame_free(&convertedFrame);
	}
	
	if (hwFrame) {
		av_frame_free(&hwFrame);
	}
	
	if (packet) {
		FFmpegCompat::freePacket(&packet);
	}
	
	if (codecCtx) {
		avcodec_free_context(&codecCtx);
	}
	
	if (hwDeviceCtx) {
		av_buffer_unref(&hwDeviceCtx);
	}
	
	if (formatCtx) {
		if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) {
			avio_closep(&formatCtx->pb);
		}
		avformat_free_context(formatCtx);
		formatCtx = nullptr;
	}
}

bool FFmpegEncoder::writeFrame(AVFrame* frame) {
	if (!frame || finalized) {
		return false;
	}
	
	AVFrame* frameToEncode = frame;
	
	// Convert pixel format if necessary
	if (frame->format != config.pixelFormat ||
		frame->width != config.width ||
		frame->height != config.height) {
		
		if (!swsCtx) {
			swsCtx = sws_getContext(
				frame->width, frame->height, (AVPixelFormat)frame->format,
				config.width, config.height, config.pixelFormat,
				SWS_BILINEAR, nullptr, nullptr, nullptr);
			
			if (!swsCtx) {
				utils::Logger::error("Failed to create scaling context");
				return false;
			}
		}
		
		int ret = av_frame_make_writable(convertedFrame);
		if (ret < 0) {
			return false;
		}
		
		sws_scale(swsCtx,
			frame->data, frame->linesize, 0, frame->height,
			convertedFrame->data, convertedFrame->linesize);
		
		frameToEncode = convertedFrame;
	}
	
	// Set frame pts - use presentation order
	// The encoder will handle DTS generation for B-frames
	frameToEncode->pts = pts++;
	
	return encodeFrame(frameToEncode);
}

bool FFmpegEncoder::writeHardwareFrame(AVFrame* frame) {
	if (!frame || finalized) {
		return false;
	}
	
	if (!usingHardware) {
		utils::Logger::warn("writeHardwareFrame called but hardware encoding is not enabled");
		return writeFrame(frame);
	}
	
	// For hardware frames, we can encode directly without transfer
	// Check if the frame is actually a hardware frame
	if (!HardwareAcceleration::isHardwareFrame(frame)) {
		// Need to upload to GPU
		if (!hwFrame) {
			hwFrame = av_frame_alloc();
			if (!hwFrame) {
				return false;
			}
		}
		
		// Set the hardware frame format
		hwFrame->format = HardwareAcceleration::getHWPixelFormat(
			config.hwConfig.type == HWAccelType::Auto ? 
			HardwareAcceleration::getBestAccelType() : config.hwConfig.type
		);
		hwFrame->width = frame->width;
		hwFrame->height = frame->height;
#if HAVE_HWDEVICE_API
		hwFrame->hw_frames_ctx = av_buffer_ref(codecCtx->hw_frames_ctx);
		
		int ret = av_hwframe_get_buffer(codecCtx->hw_frames_ctx, hwFrame, 0);
#else
		int ret = -1;
#endif
		if (ret < 0) {
			utils::Logger::error("Failed to get hardware buffer");
			return false;
		}
		
		// Transfer software frame to hardware
#if HAVE_HWDEVICE_API
		ret = av_hwframe_transfer_data(hwFrame, frame, 0);
#else
		ret = -1;
#endif
		if (ret < 0) {
			utils::Logger::error("Failed to transfer frame to GPU");
			av_frame_unref(hwFrame);
			return false;
		}
		
		hwFrame->pts = pts++;
		return encodeHardwareFrame(hwFrame);
	} else {
		// Frame is already on GPU, encode directly
		frame->pts = pts++;
		return encodeHardwareFrame(frame);
	}
}

bool FFmpegEncoder::encodeHardwareFrame(AVFrame* frame) {
	// Encode hardware frame directly
	if (FFmpegCompat::encodeVideoFrame(codecCtx, frame, packet)) {
		// Rescale timestamps
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		// Write packet
		int ret = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (ret < 0) {
			utils::Logger::error("Error writing hardware packet");
			return false;
		}
		
		frameCount++;
	}
	
	if (frame == hwFrame) {
		av_frame_unref(hwFrame);
	}
	
	return true;
}

bool FFmpegEncoder::encodeFrame(AVFrame* frame) {
	if (FFmpegCompat::encodeVideoFrame(codecCtx, frame, packet)) {
		// Rescale timestamps
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		// Write packet
		int ret = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (ret < 0) {
			utils::Logger::error("Error writing packet");
			return false;
		}
		
		frameCount++;
	}
	
	return true;
}

bool FFmpegEncoder::flushEncoder() {
	// Flush encoder by sending null frame repeatedly
	while (FFmpegCompat::encodeVideoFrame(codecCtx, nullptr, packet)) {
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		int ret = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (ret < 0) {
			return false;
		}
	}
	
	return true;
}

bool FFmpegEncoder::finalize() {
	if (finalized) {
		return true;
	}
	
	// Flush encoder
	flushEncoder();
	
	// Write trailer
	int ret = av_write_trailer(formatCtx);
	if (ret < 0) {
		utils::Logger::error("Failed to write trailer");
		return false;
	}
	
	finalized = true;
	utils::Logger::info("Encoder finalized: {} frames written", frameCount);
	return true;
}

} // namespace media