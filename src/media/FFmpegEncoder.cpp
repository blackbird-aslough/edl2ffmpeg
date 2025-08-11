#include "media/FFmpegEncoder.h"
#include "utils/Logger.h"
#include <stdexcept>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
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
	
	// Find encoder
	const AVCodec* codec = avcodec_find_encoder_by_name(config.codec.c_str());
	if (!codec) {
		// Try to find by ID if name fails
		if (config.codec == "libx264") {
			codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		} else if (config.codec == "libx265") {
			codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
		}
		
		if (!codec) {
			throw std::runtime_error("Codec not found: " + config.codec);
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
	codecCtx->pix_fmt = config.pixelFormat;
	codecCtx->time_base = av_inv_q(config.frameRate);
	codecCtx->framerate = config.frameRate;
	codecCtx->bit_rate = config.bitrate;
	codecCtx->gop_size = config.frameRate.num / config.frameRate.den * 2; // 2 second GOP
	codecCtx->max_b_frames = 2;
	
	// Set stream time base
	videoStream->time_base = codecCtx->time_base;
	
	// Set codec-specific options
	if (config.codec == "libx264" || config.codec == "libx265") {
		av_opt_set(codecCtx->priv_data, "preset", config.preset.c_str(), 0);
		av_opt_set_int(codecCtx->priv_data, "crf", config.crf, 0);
	}
	
	// Some formats want stream headers to be separate
	if (formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
		codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	
	// Open codec
	ret = avcodec_open2(codecCtx, codec, nullptr);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		throw std::runtime_error("Failed to open codec: " + std::string(errbuf));
	}
	
	// Copy codec parameters to stream
	ret = avcodec_parameters_from_context(videoStream->codecpar, codecCtx);
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
	packet = av_packet_alloc();
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
	
	utils::Logger::info("Encoder initialized: {}x{} @ {} fps, codec: {}",
		config.width, config.height,
		(double)config.frameRate.num / config.frameRate.den,
		config.codec);
}

void FFmpegEncoder::cleanup() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	
	if (convertedFrame) {
		av_frame_free(&convertedFrame);
	}
	
	if (packet) {
		av_packet_free(&packet);
	}
	
	if (codecCtx) {
		avcodec_free_context(&codecCtx);
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
	
	// Set frame pts
	frameToEncode->pts = pts++;
	
	return encodeFrame(frameToEncode);
}

bool FFmpegEncoder::encodeFrame(AVFrame* frame) {
	int ret = avcodec_send_frame(codecCtx, frame);
	if (ret < 0) {
		utils::Logger::error("Error sending frame to encoder");
		return false;
	}
	
	while (ret >= 0) {
		ret = avcodec_receive_packet(codecCtx, packet);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		} else if (ret < 0) {
			utils::Logger::error("Error receiving packet from encoder");
			return false;
		}
		
		// Rescale timestamps
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		// Write packet
		ret = av_interleaved_write_frame(formatCtx, packet);
		av_packet_unref(packet);
		
		if (ret < 0) {
			utils::Logger::error("Error writing packet");
			return false;
		}
	}
	
	frameCount++;
	return true;
}

bool FFmpegEncoder::flushEncoder() {
	// Send flush signal
	int ret = avcodec_send_frame(codecCtx, nullptr);
	if (ret < 0) {
		return false;
	}
	
	// Receive remaining packets
	while (true) {
		ret = avcodec_receive_packet(codecCtx, packet);
		if (ret == AVERROR_EOF) {
			break;
		} else if (ret < 0) {
			return false;
		}
		
		av_packet_rescale_ts(packet, codecCtx->time_base, videoStream->time_base);
		packet->stream_index = videoStream->index;
		
		ret = av_interleaved_write_frame(formatCtx, packet);
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