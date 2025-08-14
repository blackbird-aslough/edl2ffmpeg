#include "media/FFmpegDecoder.h"
#include "media/FFmpegCompat.h"
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
	const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
#else
	const AVCodec* codec = avcodec_find_decoder(stream->codec->codec_id);
#endif
	
	if (!codec) {
		throw std::runtime_error("Codec not found");
	}
	
	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx) {
		throw std::runtime_error("Failed to allocate codec context");
	}
	
	int ret = FFmpegCompat::copyCodecParameters(codecCtx, stream);
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
	while (true) {
		int ret = av_read_frame(formatCtx, packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF) {
				// Try to flush decoder
				if (FFmpegCompat::decodeVideoFrame(codecCtx, frame, nullptr)) {
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
		
		if (FFmpegCompat::decodeVideoFrame(codecCtx, frame, packet)) {
			av_packet_unref(packet);
			currentFrameNumber++;
			return true;
		}
		
		av_packet_unref(packet);
		// Continue to next packet if decoding failed or no frame ready
		continue;
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