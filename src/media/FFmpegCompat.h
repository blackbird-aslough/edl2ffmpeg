#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media {

// Version detection macros
#define FFMPEG_MAJOR_VERSION LIBAVCODEC_VERSION_MAJOR

// Key version thresholds for API changes (only define if not already set by CMake)
#ifndef HAVE_SEND_RECEIVE_API
#define HAVE_SEND_RECEIVE_API (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100))
#endif
#ifndef HAVE_PACKET_ALLOC_API
#define HAVE_PACKET_ALLOC_API (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100)) 
#endif
#ifndef HAVE_CODECPAR_API
#define HAVE_CODECPAR_API (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100))
#endif
#ifndef HAVE_HWDEVICE_API
// Hardware device API was added in FFmpeg 3.2 (libavcodec 57.60.100)
#define HAVE_HWDEVICE_API (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 60, 100))
#endif

/**
 * Compatibility wrapper for video frame decoding
 * Uses new send/receive API on FFmpeg 3.1+ or old decode API on earlier versions
 */
class FFmpegCompat {
public:
	/**
	 * Decode a video packet into a frame
	 * @param codecCtx The codec context
	 * @param frame The frame to decode into
	 * @param packet The packet to decode (can be nullptr for flushing)
	 * @return true if frame was decoded successfully, false if no frame available or error
	 */
	static bool decodeVideoFrame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
	
	/**
	 * Encode a video frame into a packet
	 * @param codecCtx The codec context
	 * @param frame The frame to encode (can be nullptr for flushing)
	 * @param packet The packet to encode into
	 * @return true if packet was encoded successfully, false if no packet available or error
	 */
	static bool encodeVideoFrame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
	
	/**
	 * Allocate a new packet with compatibility for different FFmpeg versions
	 * @return New allocated packet, or nullptr on failure
	 */
	static AVPacket* allocPacket();
	
	/**
	 * Free a packet allocated with allocPacket
	 * @param packet Packet to free (can be nullptr)
	 */
	static void freePacket(AVPacket** packet);
	
	/**
	 * Copy codec parameters to codec context with version compatibility
	 * @param codecCtx Destination codec context
	 * @param stream Stream containing codec information
	 * @return 0 on success, negative on error
	 */
	static int copyCodecParameters(AVCodecContext* codecCtx, AVStream* stream);
	
	/**
	 * Copy codec context parameters to stream with version compatibility
	 * @param stream Destination stream
	 * @param codecCtx Source codec context
	 * @return 0 on success, negative on error
	 */
	static int copyCodecParametersToStream(AVStream* stream, AVCodecContext* codecCtx);
	
	/**
	 * Get codec context from stream with version compatibility
	 * For FFmpeg 3.1+: creates context from codecpar
	 * For FFmpeg 2.x: returns stream->codec directly
	 * @param stream The stream
	 * @return Codec context, or nullptr on failure
	 */
	static AVCodecContext* getCodecContext(AVStream* stream);
	
	/**
	 * Flush decoder buffers
	 * @param codecCtx Codec context to flush
	 */
	static void flushBuffers(AVCodecContext* codecCtx);

private:
	// Helper functions for different API versions
	static bool decodeVideoFrameNew(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
	static bool decodeVideoFrameOld(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
	static bool encodeVideoFrameNew(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
	static bool encodeVideoFrameOld(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* packet);
};

} // namespace media