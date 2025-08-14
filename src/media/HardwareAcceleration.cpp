#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"
#include <algorithm>
#include <filesystem>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

namespace media {

namespace fs = std::filesystem;

std::vector<HWDevice> HardwareAcceleration::detectDevices() {
	std::vector<HWDevice> devices;
	
#ifdef HAVE_NVENC
	auto nvencDevices = detectNVENC();
	devices.insert(devices.end(), nvencDevices.begin(), nvencDevices.end());
#endif
	
#ifdef HAVE_VAAPI
	auto vaapiDevices = detectVAAPI();
	devices.insert(devices.end(), vaapiDevices.begin(), vaapiDevices.end());
#endif
	
#ifdef HAVE_VIDEOTOOLBOX
	auto vtDevices = detectVideoToolbox();
	devices.insert(devices.end(), vtDevices.begin(), vtDevices.end());
#endif
	
	return devices;
}

HWAccelType HardwareAcceleration::getBestAccelType() {
	auto devices = detectDevices();
	if (devices.empty()) {
		return HWAccelType::None;
	}
	
	// Prefer NVENC > VideoToolbox > VAAPI
	for (const auto& device : devices) {
		if (device.type == HWAccelType::NVENC) {
			return HWAccelType::NVENC;
		}
	}
	
	for (const auto& device : devices) {
		if (device.type == HWAccelType::VideoToolbox) {
			return HWAccelType::VideoToolbox;
		}
	}
	
	for (const auto& device : devices) {
		if (device.type == HWAccelType::VAAPI) {
			return HWAccelType::VAAPI;
		}
	}
	
	return HWAccelType::None;
}

AVBufferRef* HardwareAcceleration::createHWDeviceContext(HWAccelType type, int deviceIndex) {
	AVBufferRef* hwDeviceCtx = nullptr;
	int ret = 0;
	
	switch (type) {
	case HWAccelType::NVENC:
		ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, 
			std::to_string(deviceIndex).c_str(), nullptr, 0);
		break;
		
	case HWAccelType::VAAPI: {
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
		break;
	}
		
	case HWAccelType::VideoToolbox:
		ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
			nullptr, nullptr, 0);
		break;
		
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
}

AVPixelFormat HardwareAcceleration::getHWPixelFormat(HWAccelType type) {
	switch (type) {
	case HWAccelType::NVENC:
		return AV_PIX_FMT_CUDA;
	case HWAccelType::VAAPI:
		return AV_PIX_FMT_VAAPI;
	case HWAccelType::VideoToolbox:
		return AV_PIX_FMT_VIDEOTOOLBOX;
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
		case AV_CODEC_ID_AV1: return "av1_cuvid";
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
		case AV_CODEC_ID_AV1: return "av1_nvenc";
		default: return "";
		}
		
	case HWAccelType::VAAPI:
		switch (codecId) {
		case AV_CODEC_ID_H264: return "h264_vaapi";
		case AV_CODEC_ID_HEVC: return "hevc_vaapi";
		case AV_CODEC_ID_VP8: return "vp8_vaapi";
		case AV_CODEC_ID_VP9: return "vp9_vaapi";
		case AV_CODEC_ID_AV1: return "av1_vaapi";
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
	
	switch (frame->format) {
	case AV_PIX_FMT_CUDA:
	case AV_PIX_FMT_VAAPI:
	case AV_PIX_FMT_VIDEOTOOLBOX:
	case AV_PIX_FMT_QSV:
	case AV_PIX_FMT_VULKAN:
		return true;
	default:
		return false;
	}
}

bool HardwareAcceleration::isHardwarePixelFormat(AVPixelFormat format) {
	switch (format) {
	case AV_PIX_FMT_CUDA:
	case AV_PIX_FMT_VAAPI:
	case AV_PIX_FMT_VIDEOTOOLBOX:
	case AV_PIX_FMT_QSV:
	case AV_PIX_FMT_VULKAN:
		return true;
	default:
		return false;
	}
}

int HardwareAcceleration::transferHWFrameToSW(AVFrame* hwFrame, AVFrame* swFrame) {
	if (!hwFrame || !swFrame) {
		return AVERROR(EINVAL);
	}
	
	// Transfer data from GPU to CPU
	return av_hwframe_transfer_data(swFrame, hwFrame, 0);
}

int HardwareAcceleration::transferSWFrameToHW(AVFrame* swFrame, AVFrame* hwFrame, AVBufferRef* hwDeviceCtx) {
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
	
	// Transfer data from CPU to GPU
	return av_hwframe_transfer_data(hwFrame, swFrame, 0);
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
		int ret = av_hwdevice_ctx_create(&testCtx, AV_HWDEVICE_TYPE_CUDA,
			std::to_string(i).c_str(), nullptr, 0);
		
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
			int ret = av_hwdevice_ctx_create(&testCtx, AV_HWDEVICE_TYPE_VAAPI,
				path.c_str(), nullptr, 0);
			
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
	int ret = av_hwdevice_ctx_create(&testCtx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
		nullptr, nullptr, 0);
	
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