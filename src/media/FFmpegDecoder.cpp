#include "media/FFmpegDecoder.h"
#include "utils/Logger.h"
#include <stdexcept>
#include <algorithm>

extern "C" {
#include <libavutil/imgutils.h>
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
	
	packet = av_packet_alloc();
	if (!packet) {
		throw std::runtime_error("Failed to allocate packet");
	}
}

void FFmpegDecoder::findVideoStream() {
	for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
		if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
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
	const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
	
	if (!codec) {
		throw std::runtime_error("Codec not found");
	}
	
	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx) {
		throw std::runtime_error("Failed to allocate codec context");
	}
	
	int ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
	if (ret < 0) {
		throw std::runtime_error("Failed to copy codec parameters");
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
	
	// Initialize frame pool
	framePool = utils::FrameBufferPool(width, height, pixelFormat);
	
	utils::Logger::info("Decoder initialized: {}x{} @ {} fps, threads: {}",
		width, height, (double)frameRate.num / frameRate.den,
		codecCtx->thread_count == 0 ? "auto" : std::to_string(codecCtx->thread_count));
}

void FFmpegDecoder::cleanup() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	
	if (codecCtx) {
		avcodec_free_context(&codecCtx);
	}
	
	if (packet) {
		av_packet_free(&packet);
	}
	
	if (formatCtx) {
		avformat_close_input(&formatCtx);
	}
}

bool FFmpegDecoder::seekToFrame(int64_t frameNumber) {
	if (frameNumber < 0 || frameNumber >= totalFrames) {
		return false;
	}
	
	// If we're already at or past this frame, we might need to seek backwards
	if (currentFrameNumber >= frameNumber) {
		// Seek to a keyframe before the target
		int64_t targetPts = frameNumberToPts(frameNumber);
		int ret = av_seek_frame(formatCtx, videoStreamIndex, targetPts,
			AVSEEK_FLAG_BACKWARD);
		
		if (ret < 0) {
			utils::Logger::error("Failed to seek to frame {}", frameNumber);
			return false;
		}
		
		avcodec_flush_buffers(codecCtx);
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
	while (true) {
		int ret = av_read_frame(formatCtx, packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF) {
				// Try to flush decoder
				avcodec_send_packet(codecCtx, nullptr);
				ret = avcodec_receive_frame(codecCtx, frame);
				if (ret == 0) {
					currentFrameNumber++;
					return true;
				}
			}
			return false;
		}
		
		if (packet->stream_index != videoStreamIndex) {
			av_packet_unref(packet);
			continue;
		}
		
		ret = avcodec_send_packet(codecCtx, packet);
		av_packet_unref(packet);
		
		if (ret < 0) {
			utils::Logger::error("Error sending packet to decoder");
			return false;
		}
		
		ret = avcodec_receive_frame(codecCtx, frame);
		if (ret == AVERROR(EAGAIN)) {
			continue;
		} else if (ret < 0) {
			utils::Logger::error("Error receiving frame from decoder");
			return false;
		}
		
		currentFrameNumber++;
		return true;
	}
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