#pragma once

#include <string>
#include <vector>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#if HAVE_HWDEVICE_API
#include <libavutil/hwcontext.h>
#endif
}

namespace media {

/**
 * Hardware acceleration types supported by the system
 */
enum class HWAccelType {
	None,
	NVENC,      // NVIDIA NVENC/NVDEC (CUDA)
	VAAPI,      // Intel/AMD VAAPI
	VideoToolbox, // macOS VideoToolbox
	Auto        // Auto-detect best available
};

/**
 * Hardware device information
 */
struct HWDevice {
	HWAccelType type;
	std::string name;
	std::string devicePath; // For VAAPI: /dev/dri/renderD128, empty for others
	int index;              // Device index for multi-GPU systems
};

/**
 * Hardware acceleration configuration
 */
struct HWConfig {
	HWAccelType type = HWAccelType::Auto;
	int deviceIndex = 0;    // GPU index for multi-GPU systems
	bool allowFallback = true; // Fall back to software if HW fails
};

/**
 * Hardware acceleration helper class
 * Handles detection, initialization, and management of hardware acceleration
 */
class HardwareAcceleration {
public:
	/**
	 * Detect available hardware acceleration devices
	 * @return List of available hardware devices
	 */
	static std::vector<HWDevice> detectDevices();
	
	/**
	 * Get the best available hardware acceleration type
	 * @return Best available acceleration type, or None if no HW accel available
	 */
	static HWAccelType getBestAccelType();
	
	/**
	 * Create hardware device context for decoding
	 * @param type Hardware acceleration type
	 * @param deviceIndex Device index for multi-GPU systems
	 * @return Hardware device context, or nullptr on failure
	 */
	static AVBufferRef* createHWDeviceContext(HWAccelType type, int deviceIndex = 0);
	
	/**
	 * Get hardware pixel format for the given acceleration type
	 * @param type Hardware acceleration type
	 * @return Hardware pixel format
	 */
	static AVPixelFormat getHWPixelFormat(HWAccelType type);
	
	/**
	 * Get hardware decoder name for codec
	 * @param codecId Codec ID (e.g., AV_CODEC_ID_H264)
	 * @param type Hardware acceleration type
	 * @return Hardware decoder name, or empty string if not available
	 */
	static std::string getHWDecoderName(AVCodecID codecId, HWAccelType type);
	
	/**
	 * Get hardware encoder name for codec
	 * @param codecId Codec ID (e.g., AV_CODEC_ID_H264)
	 * @param type Hardware acceleration type
	 * @return Hardware encoder name, or empty string if not available
	 */
	static std::string getHWEncoderName(AVCodecID codecId, HWAccelType type);
	
	/**
	 * Check if a frame is a hardware frame
	 * @param frame AVFrame to check
	 * @return true if frame is in GPU memory
	 */
	static bool isHardwareFrame(const AVFrame* frame);
	
	/**
	 * Check if a pixel format is a hardware pixel format
	 * @param format Pixel format to check
	 * @return true if format is a hardware format
	 */
	static bool isHardwarePixelFormat(AVPixelFormat format);
	
	/**
	 * Transfer hardware frame to system memory
	 * @param hwFrame Hardware frame
	 * @param swFrame Software frame to receive the data
	 * @return 0 on success, negative on error
	 */
	static int transferHWFrameToSW(AVFrame* hwFrame, AVFrame* swFrame);
	
	/**
	 * Transfer software frame to hardware memory
	 * @param swFrame Software frame
	 * @param hwFrame Hardware frame to receive the data
	 * @param hwDeviceCtx Hardware device context
	 * @return 0 on success, negative on error
	 */
	static int transferSWFrameToHW(AVFrame* swFrame, AVFrame* hwFrame, AVBufferRef* hwDeviceCtx);
	
	/**
	 * Convert HWAccelType to string
	 */
	static std::string hwAccelTypeToString(HWAccelType type);
	
	/**
	 * Convert string to HWAccelType
	 */
	static HWAccelType stringToHWAccelType(const std::string& str);
	
private:
	// Platform-specific detection functions
	static std::vector<HWDevice> detectNVENC();
	static std::vector<HWDevice> detectVAAPI();
	static std::vector<HWDevice> detectVideoToolbox();
};

} // namespace media