#include "media/FFmpegEncoder.h"
#include "media/FFmpegCompat.h"
#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"
#include "utils/Timer.h"
#include <stdexcept>
#include <thread>
#include <chrono>

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
	
	// For async hardware encoding, ensure all operations are complete
	if (asyncMode && usingHardware && codecCtx) {
#if HAVE_SEND_RECEIVE_API
		// Extra safety: drain any remaining packets
		int safety = 0;
		while (safety++ < 100) {
			AVPacket* pkt = av_packet_alloc();
			if (!pkt) break;
			
			int ret = avcodec_receive_packet(codecCtx, pkt);
			av_packet_free(&pkt);
			
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0) {
				break;
			}
		}
#endif
		
		// Reset async state
		framesInFlight = 0;
	}
	
	cleanup();
}

FFmpegEncoder::FFmpegEncoder(FFmpegEncoder&& other) noexcept
	: formatCtx(other.formatCtx)
	, codecCtx(other.codecCtx)
	, videoStream(other.videoStream)
	, packet(other.packet)
	, swsCtx(other.swsCtx)
	, convertedFrame(other.convertedFrame)
	, hwDeviceCtx(other.hwDeviceCtx)
	, hwFrame(other.hwFrame)
	, usingHardware(other.usingHardware)
	, config(other.config)
	, frameCount(other.frameCount)
	, pts(other.pts)
	, finalized(other.finalized)
	, asyncMode(other.asyncMode)
	, codecName(std::move(other.codecName))
	, framesInFlight(other.framesInFlight)
	, ownHwDeviceCtx(other.ownHwDeviceCtx) {
	
	other.formatCtx = nullptr;
	other.codecCtx = nullptr;
	other.videoStream = nullptr;
	other.packet = nullptr;
	other.swsCtx = nullptr;
	other.hwDeviceCtx = nullptr;
	other.hwFrame = nullptr;
	other.usingHardware = false;
	other.ownHwDeviceCtx = false;
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
		asyncMode = other.asyncMode;
		codecName = std::move(other.codecName);
		framesInFlight = other.framesInFlight;
		ownHwDeviceCtx = other.ownHwDeviceCtx;
		
		other.formatCtx = nullptr;
		other.codecCtx = nullptr;
		other.videoStream = nullptr;
		other.packet = nullptr;
		other.swsCtx = nullptr;
		other.hwDeviceCtx = nullptr;
		other.hwFrame = nullptr;
		other.usingHardware = false;
		other.ownHwDeviceCtx = false;
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
	codecName = config.codec;  // Store as member variable for async mode detection
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
					
					// Create hardware device context (not needed for VideoToolbox)
					if (hwType != HWAccelType::VideoToolbox) {
						// Use external hardware context if provided
						if (config.externalHwDeviceCtx) {
							hwDeviceCtx = av_buffer_ref(config.externalHwDeviceCtx);
							if (!hwDeviceCtx) {
								utils::Logger::error("Failed to reference external hardware context");
								codec = nullptr;
								usingHardware = false;
							} else {
								ownHwDeviceCtx = false;
								utils::Logger::info("Using external hardware context for encoder");
							}
						} else {
							// Create our own hardware context
							hwDeviceCtx = HardwareAcceleration::initializeHardwareContext(hwType, config.hwConfig.deviceIndex, "encoder");
							
							if (hwDeviceCtx) {
								ownHwDeviceCtx = true;
							} else {
								codec = nullptr;
								usingHardware = false;
							}
						}
					} else {
						// VideoToolbox doesn't need explicit device context for encoding
						hwDeviceCtx = nullptr;
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
		
		// Check if this is actually a hardware encoder by name
		if (codecName.find("nvenc") != std::string::npos ||
		    codecName.find("vaapi") != std::string::npos ||
		    codecName.find("videotoolbox") != std::string::npos ||
		    codecName.find("qsv") != std::string::npos) {
			usingHardware = true;
			utils::Logger::info("Detected hardware encoder by name: {}", codecName);
			
			// Create hardware device context if we don't have one
			// Use external hardware context if provided, otherwise create our own
			if (config.externalHwDeviceCtx) {
				hwDeviceCtx = av_buffer_ref(config.externalHwDeviceCtx);
				ownHwDeviceCtx = false;
				utils::Logger::info("Using external hardware context for encoder");
			} else if (!hwDeviceCtx) {
				HWAccelType hwType = HWAccelType::None;
				if (codecName.find("nvenc") != std::string::npos) {
					hwType = HWAccelType::NVENC;
				} else if (codecName.find("vaapi") != std::string::npos) {
					hwType = HWAccelType::VAAPI;
				} else if (codecName.find("videotoolbox") != std::string::npos) {
					hwType = HWAccelType::VideoToolbox;
				}
				
				if (hwType != HWAccelType::None && hwType != HWAccelType::VideoToolbox) {
					hwDeviceCtx = HardwareAcceleration::initializeHardwareContext(hwType, 0, "encoder");
					if (hwDeviceCtx) {
						ownHwDeviceCtx = true;
					} else {
						utils::Logger::warn("Failed to create hardware context for {}, encoder may still work", codecName);
					}
				}
			}
		}
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
	if (usingHardware) {
		HWAccelType hwType = config.hwConfig.type == HWAccelType::Auto ? 
			HardwareAcceleration::getBestAccelType() : config.hwConfig.type;
		
		if (hwType == HWAccelType::VideoToolbox || (hwType == HWAccelType::NVENC && !config.expectHardwareFrames)) {
			// VideoToolbox and NVENC (in non-passthrough mode) use software pixel format directly
			// The encoder handles GPU upload internally
			codecCtx->pix_fmt = config.pixelFormat;  // Use software format (e.g., YUV420P)
			// Set hardware device context for NVENC
			if (hwType == HWAccelType::NVENC && hwDeviceCtx) {
#if HAVE_HWDEVICE_API
				codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
#endif
			}
		} else if (hwDeviceCtx) {
			// Other hardware accelerators need device context
#if HAVE_HWDEVICE_API
			codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
#endif
			
			// Use hardware pixel format
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
			// Hardware requested but no device context - fall back to software
			codecCtx->pix_fmt = config.pixelFormat;
			usingHardware = false;
		}
	} else {
		codecCtx->pix_fmt = config.pixelFormat;
	}
	
	codecCtx->time_base = av_inv_q(config.frameRate);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
	codecCtx->framerate = config.frameRate;
#endif
	codecCtx->bit_rate = config.bitrate;
	codecCtx->gop_size = 300; // 300 frames GOP - matching ftv_toffmpeg default
	
	// Set default color properties to avoid warnings and ensure proper output
	// These will be overridden when we receive the first frame with actual color properties
	codecCtx->color_range = AVCOL_RANGE_MPEG; // Use MPEG/limited range by default
	codecCtx->color_primaries = AVCOL_PRI_BT709; // HD standard
	codecCtx->color_trc = AVCOL_TRC_BT709; // HD standard
	codecCtx->colorspace = AVCOL_SPC_BT709; // HD standard
	
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
	
	// Enable multi-threading for encoding
	// VideoToolbox doesn't handle thread_count=0 properly, so we need to set it explicitly
	if (codecName.find("videotoolbox") != std::string::npos && config.threadCount == 0) {
		// For VideoToolbox, explicitly set thread count to CPU core count
		// Apple Silicon performs best with thread count matching performance cores
		codecCtx->thread_count = std::thread::hardware_concurrency();
		// VideoToolbox uses dispatch queues internally, so we only need frame threading
		codecCtx->thread_type = FF_THREAD_FRAME;
		utils::Logger::debug("VideoToolbox encoder using {} threads", codecCtx->thread_count);
	} else {
		// Use configured thread count or auto-detect for other encoders
		codecCtx->thread_count = config.threadCount; // 0 means auto-detect optimal thread count
		codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE; // Enable both frame and slice threading
	}
	
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
	
	// Enable async mode for hardware encoders and NVENC
	// This allows batching multiple frames for better throughput
	// MUST be done BEFORE avcodec_open2
	if (usingHardware || codecName.find("nvenc") != std::string::npos) {
		asyncMode = true;
		
		// For hardware encoders, increase buffer sizes for better batching
		if (codecName.find("videotoolbox") != std::string::npos) {
			// VideoToolbox benefits from larger async depth
			av_opt_set_int(codecCtx->priv_data, "async_depth", ASYNC_QUEUE_SIZE, 0);
		} else if (codecName.find("nvenc") != std::string::npos) {
			// NVENC async settings
			av_opt_set_int(codecCtx->priv_data, "delay", 0, 0); // No B-frame delay
			av_opt_set_int(codecCtx->priv_data, "surfaces", ASYNC_QUEUE_SIZE * 2, 0);
		}
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
	
	// Log async mode status
	if (asyncMode) {
		utils::Logger::info("Async encoding enabled for {}", codecName);
	}
	
	utils::Logger::info("Encoder initialized: {}x{} @ {} fps, codec: {}, threads: {}, hardware: {}, async: {}, max_b_frames: {}",
		config.width, config.height,
		(double)config.frameRate.num / config.frameRate.den,
		codecName,
		codecCtx->thread_count == 0 ? "auto" : std::to_string(codecCtx->thread_count),
		usingHardware ? "yes" : "no",
		asyncMode ? "yes" : "no",
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
		// CRITICAL: For hardware codecs, we must call avcodec_close() before freeing
		// This ensures all GPU operations are completed and resources are released
		if (usingHardware) {
			utils::Logger::debug("Closing hardware encoder codec");
			// avcodec_close will handle cleanup of hw_frames_ctx internally
			avcodec_close(codecCtx);
			
			// Small delay to ensure GPU operations complete
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		
		avcodec_free_context(&codecCtx);
	}
	
	if (hwDeviceCtx && ownHwDeviceCtx) {
		// Only unref if we created it ourselves
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
	// Time the first 10 frame encodes
	static int encodeCount = 0;
	if (encodeCount < 10) {
		TIME_BLOCK(std::string("encode_frame_") + std::to_string(encodeCount++));
	}
	
	if (!frame || finalized) {
		return false;
	}
	
	AVFrame* frameToEncode = frame;
	
	// Copy color properties from source frame to encoder on first frame
	// This ensures the encoder uses the correct color range from the source
	static bool colorPropertiesSet = false;
	if (!colorPropertiesSet && frame->color_range != AVCOL_RANGE_UNSPECIFIED) {
		codecCtx->color_range = frame->color_range;
		codecCtx->color_primaries = frame->color_primaries;
		codecCtx->color_trc = frame->color_trc;
		codecCtx->colorspace = frame->colorspace;
		colorPropertiesSet = true;
		
		const char* rangeStr = (frame->color_range == AVCOL_RANGE_JPEG) ? "full" : "limited";
		utils::Logger::debug("Set encoder color properties from source - range: {}, primaries: {}, trc: {}, space: {}",
			rangeStr, frame->color_primaries, frame->color_trc, frame->colorspace);
	}
	
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
		
		// Copy color properties to converted frame
		convertedFrame->color_range = frame->color_range;
		convertedFrame->color_primaries = frame->color_primaries;
		convertedFrame->color_trc = frame->color_trc;
		convertedFrame->colorspace = frame->colorspace;
	}
	
	// Set frame pts - use presentation order
	// The encoder will handle DTS generation for B-frames
	frameToEncode->pts = pts++;
	
	bool result = encodeFrame(frameToEncode);
	
	// Process async queue frequently to maintain flow and prevent queue overflow
	if (asyncMode) {
		processEncodingQueue();
	}
	
	return result;
}

bool FFmpegEncoder::writeHardwareFrame(AVFrame* frame) {
	if (!frame || finalized) {
		return false;
	}
	
	if (!usingHardware) {
		utils::Logger::warn("writeHardwareFrame called but hardware encoding is not enabled");
		return writeFrame(frame);
	}
	
	// Copy color properties from source frame to encoder on first frame
	// This ensures the encoder uses the correct color range from the source
	static bool hwColorPropertiesSet = false;
	if (!hwColorPropertiesSet && frame->color_range != AVCOL_RANGE_UNSPECIFIED) {
		codecCtx->color_range = frame->color_range;
		codecCtx->color_primaries = frame->color_primaries;
		codecCtx->color_trc = frame->color_trc;
		codecCtx->colorspace = frame->colorspace;
		hwColorPropertiesSet = true;
		
		const char* rangeStr = (frame->color_range == AVCOL_RANGE_JPEG) ? "full" : "limited";
		utils::Logger::debug("Set hardware encoder color properties from source - range: {}, primaries: {}, trc: {}, space: {}",
			rangeStr, frame->color_primaries, frame->color_trc, frame->colorspace);
	}
	
	// Check if the frame is a hardware frame and if it's compatible with our encoder
	bool frameIsHardware = HardwareAcceleration::isHardwareFrame(frame);
	bool needsTransfer = false;
	
	if (frameIsHardware) {
		// Frame is hardware, but check if it's compatible with our encoder
		// Log the frame format for debugging
#if HAVE_HWDEVICE_API
		utils::Logger::debug("Input hardware frame - format: {}, has hw_frames_ctx: {}",
			av_get_pix_fmt_name((AVPixelFormat)frame->format),
			frame->hw_frames_ctx ? "yes" : "no");
#else
		utils::Logger::debug("Input hardware frame - format: {}",
			av_get_pix_fmt_name((AVPixelFormat)frame->format));
#endif
		
		// For now, assume frames from the decoder with shared context are compatible
		// In the future, we might need to check if formats match
		needsTransfer = false;
	} else {
		// Software frame needs to be uploaded to GPU
		needsTransfer = true;
	}
	
	if (needsTransfer) {
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
		// For NVENC, we don't use hw_frames_ctx, so check if it exists
		if (codecCtx->hw_frames_ctx) {
			hwFrame->hw_frames_ctx = av_buffer_ref(codecCtx->hw_frames_ctx);
			
			int ret = av_hwframe_get_buffer(codecCtx->hw_frames_ctx, hwFrame, 0);
			if (ret < 0) {
				utils::Logger::error("Failed to get hardware buffer");
				return false;
			}
		} else {
			// For NVENC, allocate a regular frame buffer
			int ret = av_frame_get_buffer(hwFrame, 32);
			if (ret < 0) {
				utils::Logger::error("Failed to allocate frame buffer for hardware upload");
				return false;
			}
		}
#else
		int ret = av_frame_get_buffer(hwFrame, 32);
		if (ret < 0) {
			utils::Logger::error("Failed to allocate frame buffer");
			return false;
		}
#endif
		
		// Copy color properties before transfer
		hwFrame->color_range = frame->color_range;
		hwFrame->color_primaries = frame->color_primaries;
		hwFrame->color_trc = frame->color_trc;
		hwFrame->colorspace = frame->colorspace;
		
		// Transfer software frame to hardware
		int transferRet;
#if HAVE_HWDEVICE_API
		transferRet = av_hwframe_transfer_data(hwFrame, frame, 0);
#else
		transferRet = -1;
#endif
		if (transferRet < 0) {
			utils::Logger::error("Failed to transfer frame to GPU");
			av_frame_unref(hwFrame);
			return false;
		}
		
		hwFrame->pts = pts++;
		bool result = encodeHardwareFrame(hwFrame);
		
		// Process async queue for hardware frames
		if (asyncMode) {
			processEncodingQueue();
		}
		
		return result;
	} else {
		// Frame is already on GPU and compatible, encode directly
		utils::Logger::debug("GPU passthrough: encoding hardware frame directly - format: {}, size: {}x{}", 
			av_get_pix_fmt_name((AVPixelFormat)frame->format), frame->width, frame->height);
		
		// Important: Don't modify the input frame, create a shallow copy for PTS
		AVFrame* encoderFrame = av_frame_alloc();
		if (!encoderFrame) {
			return false;
		}
		
		// Reference the hardware frame data without copying
		av_frame_ref(encoderFrame, frame);
		encoderFrame->pts = pts++;
		
		bool result = encodeHardwareFrame(encoderFrame);
		av_frame_free(&encoderFrame);
		
		// Process async queue for hardware frames
		if (asyncMode) {
			processEncodingQueue();
		}
		
		return result;
	}
}

bool FFmpegEncoder::encodeHardwareFrame(AVFrame* frame) {
#if HAVE_SEND_RECEIVE_API
	// Use async encoding for hardware frames
	if (asyncMode) {
		return sendFrameAsync(frame);
	}
	
	// Send frame to encoder
	int ret = avcodec_send_frame(codecCtx, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN)) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		utils::Logger::error("Error sending hardware frame to encoder: {}", errbuf);
		return false;
	}
	
	// Drain packets from encoder
	while (true) {
		ret = avcodec_receive_packet(codecCtx, packet);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			utils::Logger::error("Error receiving packet from encoder: {}", errbuf);
			return false;
		}
		
		// Rescale timestamps
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		// Write packet
		int writeRet = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (writeRet < 0) {
			utils::Logger::error("Error writing hardware packet");
			return false;
		}
		
		frameCount++;
	}
	
	if (frame == hwFrame) {
		av_frame_unref(hwFrame);
	}
	
	return true;
#else
	// Hardware encoding not supported with FFmpeg 2.x
	utils::Logger::error("Hardware encoding requires FFmpeg 3.1+");
	return false;
#endif
}

bool FFmpegEncoder::encodeFrame(AVFrame* frame) {
#if HAVE_SEND_RECEIVE_API
	// If async mode is enabled for hardware encoding, use async path
	if (asyncMode) {
		return sendFrameAsync(frame);
	}
	
	// Send frame to encoder
	int ret = avcodec_send_frame(codecCtx, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN)) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		utils::Logger::error("Error sending frame to encoder: {}", errbuf);
		return false;
	}
	
	// Drain packets from encoder
	while (true) {
		ret = avcodec_receive_packet(codecCtx, packet);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			utils::Logger::error("Error receiving packet from encoder: {}", errbuf);
			return false;
		}
		
		// Rescale timestamps
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		// Write packet
		int writeRet = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (writeRet < 0) {
			utils::Logger::error("Error writing packet");
			return false;
		}
		
		frameCount++;
	}
	
	return true;
#else
	// Use FFmpegCompat for legacy API
	if (FFmpegCompat::encodeVideoFrame(codecCtx, frame, packet)) {
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		int writeRet = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		if (writeRet < 0) {
			utils::Logger::error("Error writing packet");
			return false;
		}
		frameCount++;
		return true;
	}
	return true; // No frame ready yet
#endif
}

bool FFmpegEncoder::flushEncoder() {
#if HAVE_SEND_RECEIVE_API
	// Send flush signal to encoder
	int ret = avcodec_send_frame(codecCtx, nullptr);
	if (ret < 0 && ret != AVERROR_EOF) {
		return false;
	}
	
	// Drain remaining packets
	while (true) {
		ret = avcodec_receive_packet(codecCtx, packet);
		if (ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			return false;
		}
		
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		int writeRet = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (writeRet < 0) {
			return false;
		}
	}
	
	return true;
#else
	// Flush using legacy API
	while (FFmpegCompat::encodeVideoFrame(codecCtx, nullptr, packet)) {
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		int writeRet = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		if (writeRet < 0) {
			return false;
		}
		frameCount++;
	}
	return true;
#endif
}

bool FFmpegEncoder::sendFrameAsync(AVFrame* frame) {
#if HAVE_SEND_RECEIVE_API
	// Send frame to encoder without waiting for packet
	int ret = avcodec_send_frame(codecCtx, frame);
	
	if (ret < 0 && ret != AVERROR(EAGAIN)) {
		if (ret == AVERROR_EOF) {
			return true; // Expected during flush
		}
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		if (frame) {
			utils::Logger::error("Error sending frame to encoder: {} (format: {}, size: {}x{})", 
				errbuf, av_get_pix_fmt_name((AVPixelFormat)frame->format), 
				frame->width, frame->height);
		} else {
			utils::Logger::error("Error sending frame to encoder: {}", errbuf);
		}
		return false;
	}
	
	if (ret == 0 && frame != nullptr) {
		framesInFlight++;
		
		// Try to receive packets if queue is getting full
		if (framesInFlight >= ASYNC_QUEUE_SIZE - 2) {
			receivePacketsAsync();
		}
	}
	
	return true;
#else
	// Async mode not supported with FFmpeg 2.x
	utils::Logger::error("Async encoding requires FFmpeg 3.1+");
	return false;
#endif
}

bool FFmpegEncoder::receivePacketsAsync() {
#if HAVE_SEND_RECEIVE_API && HAVE_PACKET_ALLOC_API
	bool receivedAny = false;
	
	// Try to receive multiple packets
	while (true) {
		AVPacket* pkt = av_packet_alloc();
		if (!pkt) {
			break;
		}
		
		int ret = avcodec_receive_packet(codecCtx, pkt);
		
		if (ret == AVERROR(EAGAIN)) {
			// No more packets available right now
			av_packet_free(&pkt);
			break;
		} else if (ret == AVERROR_EOF) {
			// End of stream
			av_packet_free(&pkt);
			framesInFlight = 0;
			break;
		} else if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			utils::Logger::error("Error receiving packet from encoder: {}", errbuf);
			av_packet_free(&pkt);
			break;
		}
		
		// Got a packet, write it
		av_packet_rescale_ts(pkt, codecCtx->time_base, videoStream->time_base);
		pkt->stream_index = videoStream->index;
		
		ret = av_interleaved_write_frame(formatCtx, pkt);
		av_packet_free(&pkt);
		
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			utils::Logger::error("Error writing async packet: {}", errbuf);
			break;
		}
		
		frameCount++;
		if (framesInFlight > 0) {
			framesInFlight--;
		}
		receivedAny = true;
	}
	
	return receivedAny;
#else
	// Async mode not supported with FFmpeg 2.x
	return false;
#endif
}

void FFmpegEncoder::processEncodingQueue() {
	// Process the encoding queue - called periodically
	if (asyncMode && framesInFlight > 0) {
		receivePacketsAsync();
	}
}

bool FFmpegEncoder::finalize() {
	if (finalized) {
		return true;
	}
	
	// Flush encoder
#if HAVE_SEND_RECEIVE_API && HAVE_PACKET_ALLOC_API
	if (asyncMode) {
		// For async mode, first process any remaining frames in the queue
		int flushAttempts = 0;
		while (framesInFlight > 0 && flushAttempts < 100) {
			bool received = receivePacketsAsync();
			if (!received) {
				// No packets received, avoid spinning
				break;
			}
			flushAttempts++;
		}
		
		// Send flush signal to encoder
		int ret = avcodec_send_frame(codecCtx, nullptr);
		if (ret < 0 && ret != AVERROR_EOF) {
			utils::Logger::warn("Failed to send flush frame to encoder");
		}
		
		// Drain all remaining packets
		int maxIterations = 1000;  // Prevent infinite loop
		int iterations = 0;
		bool done = false;
		
		while (!done && iterations < maxIterations) {
			AVPacket* pkt = av_packet_alloc();
			if (!pkt) {
				break;
			}
			
			ret = avcodec_receive_packet(codecCtx, pkt);
			if (ret == AVERROR(EAGAIN)) {
				// No more packets available, we're done
				av_packet_free(&pkt);
				done = true;
			} else if (ret == AVERROR_EOF || ret < 0) {
				// End of stream or error
				done = true;
				av_packet_free(&pkt);
			} else {
				// Successfully received a packet
				av_packet_rescale_ts(pkt, codecCtx->time_base, videoStream->time_base);
				pkt->stream_index = videoStream->index;
				
				if (av_interleaved_write_frame(formatCtx, pkt) < 0) {
					utils::Logger::error("Error writing packet during flush");
				}
				frameCount++;
				av_packet_free(&pkt);
			}
			
			iterations++;
		}
		
		if (iterations >= maxIterations) {
			utils::Logger::warn("Async flush timeout after {} iterations", iterations);
		}
	} else {
		// For sync mode, use the traditional flush
		flushEncoder();
	}
#else
	// Use legacy flush for FFmpeg 2.x
	flushEncoder();
#endif
	
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