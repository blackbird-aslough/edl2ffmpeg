#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"
#include <algorithm>
#include <filesystem>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#if HAVE_HWDEVICE_API
#include <libavutil/hwcontext.h>
#endif
}

namespace media {

namespace fs = std::filesystem;

std::vector<HWDevice> HardwareAcceleration::detectDevices() {
	std::vector<HWDevice> devices;
	
#ifdef HAVE_NVENC
	utils::Logger::debug("Detecting NVENC devices...");
	auto nvencDevices = detectNVENC();
	utils::Logger::debug("Found {} NVENC devices", nvencDevices.size());
	devices.insert(devices.end(), nvencDevices.begin(), nvencDevices.end());
#else
	utils::Logger::debug("NVENC support not compiled in");
#endif
	
#ifdef HAVE_VAAPI
	utils::Logger::debug("Detecting VAAPI devices...");
	auto vaapiDevices = detectVAAPI();
	utils::Logger::debug("Found {} VAAPI devices", vaapiDevices.size());
	devices.insert(devices.end(), vaapiDevices.begin(), vaapiDevices.end());
#else
	utils::Logger::debug("VAAPI support not compiled in");
#endif
	
#ifdef HAVE_VIDEOTOOLBOX
	utils::Logger::debug("Detecting VideoToolbox devices...");
	auto vtDevices = detectVideoToolbox();
	utils::Logger::debug("Found {} VideoToolbox devices", vtDevices.size());
	devices.insert(devices.end(), vtDevices.begin(), vtDevices.end());
#else
	utils::Logger::debug("VideoToolbox support not compiled in");
#endif
	
	utils::Logger::debug("Total hardware devices detected: {}", devices.size());
	return devices;
}

HWAccelType HardwareAcceleration::getBestAccelType() {
	utils::Logger::debug("Getting best hardware acceleration type...");
	auto devices = detectDevices();
	if (devices.empty()) {
		utils::Logger::debug("No hardware devices found");
		return HWAccelType::None;
	}
	
	// Prefer NVENC > VideoToolbox > VAAPI
	for (const auto& device : devices) {
		if (device.type == HWAccelType::NVENC) {
			utils::Logger::debug("Selected NVENC as best hardware acceleration");
			return HWAccelType::NVENC;
		}
	}
	
	for (const auto& device : devices) {
		if (device.type == HWAccelType::VideoToolbox) {
			utils::Logger::debug("Selected VideoToolbox as best hardware acceleration");
			return HWAccelType::VideoToolbox;
		}
	}
	
	for (const auto& device : devices) {
		if (device.type == HWAccelType::VAAPI) {
			utils::Logger::debug("Selected VAAPI as best hardware acceleration");
			return HWAccelType::VAAPI;
		}
	}
	
	utils::Logger::debug("No suitable hardware acceleration found");
	return HWAccelType::None;
}

AVBufferRef* HardwareAcceleration::createHWDeviceContext(HWAccelType type, int deviceIndex) {
	(void)deviceIndex; // TODO: Use deviceIndex for multi-GPU support
#if HAVE_HWDEVICE_API
	AVBufferRef* hwDeviceCtx = nullptr;
	int ret = 0;
	
	switch (type) {
	case HWAccelType::NVENC: {
#if HAVE_HWDEVICE_API
		// For CUDA, pass NULL as device to use the default GPU
		utils::Logger::debug("Creating CUDA device context...");
		// Get the hwdevice type enum value at runtime
		enum AVHWDeviceType cudaType = av_hwdevice_find_type_by_name("cuda");
		if (cudaType == AV_HWDEVICE_TYPE_NONE) {
			utils::Logger::debug("CUDA hwdevice type not found in FFmpeg");
			ret = -1;
		} else {
			ret = av_hwdevice_ctx_create(&hwDeviceCtx, cudaType, 
				nullptr, nullptr, 0);
			if (ret < 0) {
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				utils::Logger::debug("CUDA device creation failed: {} (code: {})", errbuf, ret);
			} else {
				utils::Logger::debug("CUDA device context created successfully");
			}
		}
#else
		utils::Logger::debug("Hardware device API not available in legacy FFmpeg");
		ret = -1;
#endif
		break;
	}
		
	case HWAccelType::VAAPI: {
#ifdef AV_HWDEVICE_TYPE_VAAPI
		// Try common VAAPI device paths
		std::vector<std::string> devicePaths = {
			"/dev/dri/renderD128",
			"/dev/dri/renderD129",
			"/dev/dri/card0",
			"/dev/dri/card1"
		};
		
		for (const auto& path : devicePaths) {
			if (fs::exists(path)) {
				ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI,
					path.c_str(), nullptr, 0);
				if (ret >= 0) {
					utils::Logger::info("Created VAAPI device context using: {}", path);
					break;
				}
			}
		}
#else
		ret = -1;
#endif
		break;
	}
		
	case HWAccelType::VideoToolbox:
		// VideoToolbox doesn't require explicit device context creation on macOS
		// It works directly through the codec without needing av_hwdevice_ctx_create
		utils::Logger::debug("VideoToolbox uses implicit device context - skipping explicit creation");
		return nullptr;  // Return nullptr to indicate no explicit context needed
		
	case HWAccelType::None:
		utils::Logger::debug("No hardware acceleration requested");
		return nullptr;
		
	case HWAccelType::Auto:
		utils::Logger::error("Auto hardware acceleration type should have been resolved before this point");
		return nullptr;
		
	default:
		utils::Logger::error("Unsupported hardware acceleration type");
		return nullptr;
	}
	
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, errbuf, sizeof(errbuf));
		utils::Logger::error("Failed to create hardware device context: {}", errbuf);
		return nullptr;
	}
	
	return hwDeviceCtx;
#else
	(void)type;
	(void)deviceIndex;
	utils::Logger::info("Hardware device context creation not supported in legacy FFmpeg");
	return nullptr;
#endif
}

AVPixelFormat HardwareAcceleration::getHWPixelFormat(HWAccelType type) {
	switch (type) {
	case HWAccelType::NVENC: {
		// Try to find CUDA pixel format at runtime
		AVPixelFormat fmt = av_get_pix_fmt("cuda");
		if (fmt != AV_PIX_FMT_NONE) {
			return fmt;
		}
		utils::Logger::debug("CUDA pixel format not found, trying fallback formats");
		// Fallback to other possible formats
		fmt = av_get_pix_fmt("nv12");  // NVENC typically uses NV12
		return fmt != AV_PIX_FMT_NONE ? fmt : AV_PIX_FMT_YUV420P;
	}
	case HWAccelType::VAAPI: {
#ifdef AV_PIX_FMT_VAAPI
		return AV_PIX_FMT_VAAPI;
#else
		AVPixelFormat fmt = av_get_pix_fmt("vaapi");
		return fmt != AV_PIX_FMT_NONE ? fmt : AV_PIX_FMT_NONE;
#endif
	}
	case HWAccelType::VideoToolbox: {
#ifdef AV_PIX_FMT_VIDEOTOOLBOX
		return AV_PIX_FMT_VIDEOTOOLBOX;
#else
		AVPixelFormat fmt = av_get_pix_fmt("videotoolbox_vld");
		return fmt != AV_PIX_FMT_NONE ? fmt : AV_PIX_FMT_NONE;
#endif
	}
	default:
		return AV_PIX_FMT_NONE;
	}
}

std::string HardwareAcceleration::getHWDecoderName(AVCodecID codecId, HWAccelType type) {
	switch (type) {
	case HWAccelType::NVENC:
		switch (codecId) {
		case AV_CODEC_ID_H264: return "h264_cuvid";
		case AV_CODEC_ID_HEVC: return "hevc_cuvid";
		case AV_CODEC_ID_VP9: return "vp9_cuvid";
#ifdef AV_CODEC_ID_AV1
		case AV_CODEC_ID_AV1: return "av1_cuvid";
#endif
		default: return "";
		}
		
	case HWAccelType::VAAPI:
		// VAAPI uses standard decoders with hwaccel
		return "";
		
	case HWAccelType::VideoToolbox:
		// VideoToolbox uses standard decoders with hwaccel
		return "";
		
	default:
		return "";
	}
}

std::string HardwareAcceleration::getHWEncoderName(AVCodecID codecId, HWAccelType type) {
	switch (type) {
	case HWAccelType::NVENC:
		switch (codecId) {
		case AV_CODEC_ID_H264: return "h264_nvenc";
		case AV_CODEC_ID_HEVC: return "hevc_nvenc";
#ifdef AV_CODEC_ID_AV1
		case AV_CODEC_ID_AV1: return "av1_nvenc";
#endif
		default: return "";
		}
		
	case HWAccelType::VAAPI:
		switch (codecId) {
		case AV_CODEC_ID_H264: return "h264_vaapi";
		case AV_CODEC_ID_HEVC: return "hevc_vaapi";
		case AV_CODEC_ID_VP8: return "vp8_vaapi";
		case AV_CODEC_ID_VP9: return "vp9_vaapi";
#ifdef AV_CODEC_ID_AV1
		case AV_CODEC_ID_AV1: return "av1_vaapi";
#endif
		default: return "";
		}
		
	case HWAccelType::VideoToolbox:
		switch (codecId) {
		case AV_CODEC_ID_H264: return "h264_videotoolbox";
		case AV_CODEC_ID_HEVC: return "hevc_videotoolbox";
		default: return "";
		}
		
	default:
		return "";
	}
}

bool HardwareAcceleration::isHardwareFrame(const AVFrame* frame) {
	if (!frame) return false;
	
	// In newer FFmpeg versions, hardware pixel formats might not be defined as macros
	// So we check dynamically using av_get_pix_fmt
	static AVPixelFormat cuda_fmt = av_get_pix_fmt("cuda");
	static AVPixelFormat vaapi_fmt = av_get_pix_fmt("vaapi");
	static AVPixelFormat videotoolbox_fmt = av_get_pix_fmt("videotoolbox");
	static AVPixelFormat qsv_fmt = av_get_pix_fmt("qsv");
	static AVPixelFormat vulkan_fmt = av_get_pix_fmt("vulkan");
	
	// Debug: Check what format we're getting
	static bool debugPrinted = false;
	if (!debugPrinted) {
		utils::Logger::debug("isHardwareFrame: checking format {} (cuda={}, vaapi={}, videotoolbox={})", 
			frame->format, cuda_fmt, vaapi_fmt, videotoolbox_fmt);
		debugPrinted = true;
	}
	
	// Check if the frame format matches any known hardware format
	if (frame->format == cuda_fmt && cuda_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (frame->format == vaapi_fmt && vaapi_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (frame->format == videotoolbox_fmt && videotoolbox_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (frame->format == qsv_fmt && qsv_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (frame->format == vulkan_fmt && vulkan_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	
	// Also check if hw_frames_ctx is present - this is a strong indicator of hardware frame
	if (frame->hw_frames_ctx) {
		return true;
	}
	
	return false;
}

bool HardwareAcceleration::isHardwarePixelFormat(AVPixelFormat format) {
	// Use the same dynamic approach as isHardwareFrame
	static AVPixelFormat cuda_fmt = av_get_pix_fmt("cuda");
	static AVPixelFormat vaapi_fmt = av_get_pix_fmt("vaapi");
	static AVPixelFormat videotoolbox_fmt = av_get_pix_fmt("videotoolbox");
	static AVPixelFormat qsv_fmt = av_get_pix_fmt("qsv");
	static AVPixelFormat vulkan_fmt = av_get_pix_fmt("vulkan");
	
	// Check if the format matches any known hardware format
	if (format == cuda_fmt && cuda_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (format == vaapi_fmt && vaapi_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (format == videotoolbox_fmt && videotoolbox_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (format == qsv_fmt && qsv_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	if (format == vulkan_fmt && vulkan_fmt != AV_PIX_FMT_NONE) {
		return true;
	}
	
	return false;
}

int HardwareAcceleration::transferHWFrameToSW(AVFrame* hwFrame, AVFrame* swFrame) {
#if HAVE_HWDEVICE_API
	if (!hwFrame || !swFrame) {
		return AVERROR(EINVAL);
	}
	
	// Transfer data from GPU to CPU
	return av_hwframe_transfer_data(swFrame, hwFrame, 0);
#else
	(void)hwFrame;
	(void)swFrame;
	return AVERROR(ENOSYS);
#endif
}

int HardwareAcceleration::transferSWFrameToHW(AVFrame* swFrame, AVFrame* hwFrame, AVBufferRef* hwDeviceCtx) {
#if HAVE_HWDEVICE_API
	if (!swFrame || !hwFrame || !hwDeviceCtx) {
		return AVERROR(EINVAL);
	}
	
	// Ensure hardware frame has proper hw_frames_ctx
	if (!hwFrame->hw_frames_ctx) {
		AVBufferRef* hwFrameCtx = av_hwframe_ctx_alloc(hwDeviceCtx);
		if (!hwFrameCtx) {
			return AVERROR(ENOMEM);
		}
		
		AVHWFramesContext* framesCtx = (AVHWFramesContext*)hwFrameCtx->data;
		framesCtx->format = getHWPixelFormat(HWAccelType::Auto); // Will be set properly
		framesCtx->sw_format = (AVPixelFormat)swFrame->format;
		framesCtx->width = swFrame->width;
		framesCtx->height = swFrame->height;
		
		int ret = av_hwframe_ctx_init(hwFrameCtx);
		if (ret < 0) {
			av_buffer_unref(&hwFrameCtx);
			return ret;
		}
		
		hwFrame->hw_frames_ctx = hwFrameCtx;
	}
	
	// Allocate buffer for the hardware frame if not already allocated
	if (!hwFrame->buf[0]) {
		int ret = av_hwframe_get_buffer(hwFrame->hw_frames_ctx, hwFrame, 0);
		if (ret < 0) {
			return ret;
		}
	}
	
	// Transfer data from CPU to GPU
	return av_hwframe_transfer_data(hwFrame, swFrame, 0);
#else
	(void)swFrame;
	(void)hwFrame;
	(void)hwDeviceCtx;
	return AVERROR(ENOSYS);
#endif
}

std::string HardwareAcceleration::hwAccelTypeToString(HWAccelType type) {
	switch (type) {
	case HWAccelType::None: return "none";
	case HWAccelType::NVENC: return "nvenc";
	case HWAccelType::VAAPI: return "vaapi";
	case HWAccelType::VideoToolbox: return "videotoolbox";
	case HWAccelType::Auto: return "auto";
	default: return "unknown";
	}
}

HWAccelType HardwareAcceleration::stringToHWAccelType(const std::string& str) {
	std::string lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	
	if (lower == "none") return HWAccelType::None;
	if (lower == "nvenc" || lower == "cuda") return HWAccelType::NVENC;
	if (lower == "vaapi") return HWAccelType::VAAPI;
	if (lower == "videotoolbox" || lower == "vt") return HWAccelType::VideoToolbox;
	if (lower == "auto") return HWAccelType::Auto;
	
	return HWAccelType::None;
}

AVBufferRef* HardwareAcceleration::initializeHardwareContext(HWAccelType type, int deviceIndex, const std::string& purpose) {
	// Suppress FFmpeg error messages during hardware setup
	int oldLogLevel = av_log_get_level();
	av_log_set_level(AV_LOG_ERROR);
	
	AVBufferRef* hwDeviceCtx = createHWDeviceContext(type, deviceIndex);
	
	// Restore log level
	av_log_set_level(oldLogLevel);
	
	if (hwDeviceCtx) {
		utils::Logger::info("Initialized hardware {} context: {}", purpose, hwAccelTypeToString(type));
	} else {
		utils::Logger::warn("Failed to create hardware {} context, falling back to software", purpose);
	}
	
	return hwDeviceCtx;
}

// Platform-specific detection implementations

std::vector<HWDevice> HardwareAcceleration::detectNVENC() {
	std::vector<HWDevice> devices;
	
#ifdef HAVE_NVENC
	// Suppress FFmpeg error messages during detection
	int oldLogLevel = av_log_get_level();
	av_log_set_level(AV_LOG_QUIET);
	
	// Try to create CUDA contexts for each GPU
	for (int i = 0; i < 8; i++) { // Check up to 8 GPUs
		AVBufferRef* testCtx = nullptr;
#if HAVE_HWDEVICE_API
		// Get the hwdevice type enum value at runtime
		enum AVHWDeviceType cudaType = av_hwdevice_find_type_by_name("cuda");
		if (cudaType == AV_HWDEVICE_TYPE_NONE) {
			break; // CUDA not available in this FFmpeg build
		}
		
		// For CUDA, use nullptr for default device or "cuda:N" format for specific device
		std::string deviceName = (i == 0) ? "" : ("cuda:" + std::to_string(i));
		int ret = av_hwdevice_ctx_create(&testCtx, cudaType,
			deviceName.empty() ? nullptr : deviceName.c_str(), nullptr, 0);
#else
		int ret = -1; // Not supported in legacy FFmpeg
#endif
		
		if (ret >= 0) {
			HWDevice device;
			device.type = HWAccelType::NVENC;
			device.name = "NVIDIA GPU " + std::to_string(i);
			device.index = i;
			devices.push_back(device);
			av_buffer_unref(&testCtx);
		} else {
			break; // No more GPUs
		}
	}
	
	// Restore original log level
	av_log_set_level(oldLogLevel);
#endif
	
	return devices;
}

std::vector<HWDevice> HardwareAcceleration::detectVAAPI() {
	std::vector<HWDevice> devices;
	
#ifdef HAVE_VAAPI
	// Suppress FFmpeg error messages during detection
	int oldLogLevel = av_log_get_level();
	av_log_set_level(AV_LOG_QUIET);
	
	// Check common VAAPI device paths
	std::vector<std::string> devicePaths = {
		"/dev/dri/renderD128",
		"/dev/dri/renderD129",
		"/dev/dri/card0",
		"/dev/dri/card1"
	};
	
	int index = 0;
	for (const auto& path : devicePaths) {
		if (fs::exists(path)) {
			AVBufferRef* testCtx = nullptr;
#if HAVE_HWDEVICE_API && defined(AV_HWDEVICE_TYPE_VAAPI)
			int ret = av_hwdevice_ctx_create(&testCtx, AV_HWDEVICE_TYPE_VAAPI,
				path.c_str(), nullptr, 0);
#else
			int ret = -1; // Not supported in legacy FFmpeg
#endif
			
			if (ret >= 0) {
				HWDevice device;
				device.type = HWAccelType::VAAPI;
				device.name = "VAAPI Device";
				device.devicePath = path;
				device.index = index++;
				devices.push_back(device);
				av_buffer_unref(&testCtx);
			}
		}
	}
	
	// Restore original log level
	av_log_set_level(oldLogLevel);
#endif
	
	return devices;
}

std::vector<HWDevice> HardwareAcceleration::detectVideoToolbox() {
	std::vector<HWDevice> devices;
	
#ifdef HAVE_VIDEOTOOLBOX
	// VideoToolbox doesn't have multiple devices
	AVBufferRef* testCtx = nullptr;
#if HAVE_HWDEVICE_API && defined(AV_HWDEVICE_TYPE_VIDEOTOOLBOX)
	int ret = av_hwdevice_ctx_create(&testCtx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
		nullptr, nullptr, 0);
#else
	int ret = -1; // Not supported in legacy FFmpeg
#endif
	
	if (ret >= 0) {
		HWDevice device;
		device.type = HWAccelType::VideoToolbox;
		device.name = "VideoToolbox";
		device.index = 0;
		devices.push_back(device);
		av_buffer_unref(&testCtx);
	}
#endif
	
	return devices;
}

} // namespace media