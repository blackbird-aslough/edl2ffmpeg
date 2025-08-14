#include "FFmpegCompat.h"
#include "../utils/Logger.h"

namespace media {

bool FFmpegCompat::decodeVideoFrame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if HAVE_SEND_RECEIVE_API
	return decodeVideoFrameNew(codecCtx, frame, packet);
#else
	return decodeVideoFrameOld(codecCtx, frame, packet);
#endif
}

bool FFmpegCompat::encodeVideoFrame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if HAVE_SEND_RECEIVE_API
	return encodeVideoFrameNew(codecCtx, frame, packet);
#else
	return encodeVideoFrameOld(codecCtx, frame, packet);
#endif
}

AVPacket* FFmpegCompat::allocPacket() {
#if HAVE_PACKET_ALLOC_API
	return av_packet_alloc();
#else
	AVPacket* packet = new AVPacket();
	av_init_packet(packet);
	packet->data = nullptr;
	packet->size = 0;
	return packet;
#endif
}

void FFmpegCompat::freePacket(AVPacket** packet) {
	if (!packet || !*packet) {
		return;
	}
	
#if HAVE_PACKET_ALLOC_API
	av_packet_free(packet);
#else
	av_free_packet(*packet);
	delete *packet;
	*packet = nullptr;
#endif
}

int FFmpegCompat::copyCodecParameters(AVCodecContext* codecCtx, AVStream* stream) {
#if HAVE_CODECPAR_API
	return avcodec_parameters_to_context(codecCtx, stream->codecpar);
#else
	return avcodec_copy_context(codecCtx, stream->codec);
#endif
}

int FFmpegCompat::copyCodecParametersToStream(AVStream* stream, AVCodecContext* codecCtx) {
#if HAVE_CODECPAR_API
	return avcodec_parameters_from_context(stream->codecpar, codecCtx);
#else
	return avcodec_copy_context(stream->codec, codecCtx);
#endif
}

AVCodecContext* FFmpegCompat::getCodecContext(AVStream* stream) {
#if HAVE_CODECPAR_API
	AVCodecContext* codecCtx = avcodec_alloc_context3(nullptr);
	if (!codecCtx) {
		return nullptr;
	}
	
	int ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
	if (ret < 0) {
		avcodec_free_context(&codecCtx);
		return nullptr;
	}
	
	return codecCtx;
#else
	// For older FFmpeg, stream->codec is directly available
	return stream->codec;
#endif
}

void FFmpegCompat::flushBuffers(AVCodecContext* codecCtx) {
	avcodec_flush_buffers(codecCtx);
}

// New API implementations (FFmpeg 3.1+)
bool FFmpegCompat::decodeVideoFrameNew(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if HAVE_SEND_RECEIVE_API
	int ret;
	
	// Send packet to decoder
	ret = avcodec_send_packet(codecCtx, packet);
	if (ret < 0) {
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
			// This is expected in some cases
			return false;
		}
		utils::Logger::error("Error sending packet to decoder");
		return false;
	}
	
	// Receive frame from decoder
	ret = avcodec_receive_frame(codecCtx, frame);
	if (ret == AVERROR(EAGAIN)) {
		// Need more packets
		return false;
	} else if (ret == AVERROR_EOF) {
		// End of stream
		return false;
	} else if (ret < 0) {
		utils::Logger::error("Error receiving frame from decoder");
		return false;
	}
	
	return true;
#else
	return false; // Should not be called
#endif
}

bool FFmpegCompat::encodeVideoFrameNew(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if HAVE_SEND_RECEIVE_API
	int ret;
	
	// Send frame to encoder
	ret = avcodec_send_frame(codecCtx, frame);
	if (ret < 0) {
		if (ret == AVERROR_EOF) {
			// This is expected when flushing
			return false;
		}
		utils::Logger::error("Error sending frame to encoder");
		return false;
	}
	
	// Receive packet from encoder
	ret = avcodec_receive_packet(codecCtx, packet);
	if (ret == AVERROR(EAGAIN)) {
		// Need more frames
		return false;
	} else if (ret == AVERROR_EOF) {
		// End of stream
		return false;
	} else if (ret < 0) {
		utils::Logger::error("Error receiving packet from encoder");
		return false;
	}
	
	return true;
#else
	return false; // Should not be called
#endif
}

// Old API implementations (FFmpeg 2.x)
bool FFmpegCompat::decodeVideoFrameOld(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if !HAVE_SEND_RECEIVE_API
	int frameFinished = 0;
	int ret = avcodec_decode_video2(codecCtx, frame, &frameFinished, packet);
	
	if (ret < 0) {
		if (packet && packet->data == nullptr) {
			// End of stream error is acceptable
			utils::Logger::debug("Video decode failed at end of stream");
			return false;
		} else {
			utils::Logger::error("Video decode failed");
			return false;
		}
	}
	
	return frameFinished != 0;
#else
	return false; // Should not be called
#endif
}

bool FFmpegCompat::encodeVideoFrameOld(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet) {
#if !HAVE_SEND_RECEIVE_API
	int gotPacket = 0;
	int ret = avcodec_encode_video2(codecCtx, packet, frame, &gotPacket);
	
	if (ret < 0) {
		utils::Logger::error("Video encode failed");
		return false;
	}
	
	return gotPacket != 0;
#else
	return false; // Should not be called
#endif
}

} // namespace media