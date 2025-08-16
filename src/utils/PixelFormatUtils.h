#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace utils {

class PixelFormatUtils {
public:
	/**
	 * Check if the format is a YUV format
	 */
	static bool isYUVFormat(AVPixelFormat format) {
		return format == AV_PIX_FMT_YUV420P ||
			   format == AV_PIX_FMT_YUV422P ||
			   format == AV_PIX_FMT_YUV444P ||
			   format == AV_PIX_FMT_YUV420P10LE ||
			   format == AV_PIX_FMT_YUV422P10LE ||
			   format == AV_PIX_FMT_YUV444P10LE ||
			   format == AV_PIX_FMT_NV12 ||
			   format == AV_PIX_FMT_NV21;
	}
	
	/**
	 * Check if the format is an RGB format
	 */
	static bool isRGBFormat(AVPixelFormat format) {
		return format == AV_PIX_FMT_RGB24 ||
			   format == AV_PIX_FMT_BGR24 ||
			   format == AV_PIX_FMT_RGBA ||
			   format == AV_PIX_FMT_BGRA ||
			   format == AV_PIX_FMT_RGB565 ||
			   format == AV_PIX_FMT_RGB555;
	}
	
	/**
	 * Check if the format is a planar YUV format
	 */
	static bool isPlanarYUVFormat(AVPixelFormat format) {
		return format == AV_PIX_FMT_YUV420P ||
			   format == AV_PIX_FMT_YUV422P ||
			   format == AV_PIX_FMT_YUV444P ||
			   format == AV_PIX_FMT_YUV420P10LE ||
			   format == AV_PIX_FMT_YUV422P10LE ||
			   format == AV_PIX_FMT_YUV444P10LE;
	}
	
	/**
	 * Get the chroma subsampling for YUV formats
	 * Returns: 0 for 4:4:4, 1 for 4:2:2, 2 for 4:2:0
	 */
	static int getChromaSubsampling(AVPixelFormat format) {
		switch (format) {
		case AV_PIX_FMT_YUV444P:
		case AV_PIX_FMT_YUV444P10LE:
			return 0;
		case AV_PIX_FMT_YUV422P:
		case AV_PIX_FMT_YUV422P10LE:
			return 1;
		case AV_PIX_FMT_YUV420P:
		case AV_PIX_FMT_YUV420P10LE:
		case AV_PIX_FMT_NV12:
		case AV_PIX_FMT_NV21:
			return 2;
		default:
			return -1;
		}
	}
};

} // namespace utils