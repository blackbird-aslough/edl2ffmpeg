#include "media/HardwareContextManager.h"
#include "utils/Logger.h"

namespace media {

HardwareContextManager& HardwareContextManager::getInstance() {
	static HardwareContextManager instance;
	return instance;
}

bool HardwareContextManager::initialize(const HWConfig& config) {
	std::lock_guard<std::mutex> lock(mutex);
	
	// If already initialized, check if it's the same configuration
	if (hwDeviceCtx) {
		if (currentType == config.type) {
			utils::Logger::debug("Hardware context already initialized with type: {}", 
				HardwareAcceleration::hwAccelTypeToString(currentType));
			return true;
		} else {
			utils::Logger::warn("Hardware context already initialized with different type. Resetting.");
			reset();
		}
	}
	
	// Determine the hardware type to use
	HWAccelType typeToUse = config.type;
	if (typeToUse == HWAccelType::Auto) {
		typeToUse = HardwareAcceleration::getBestAccelType();
		if (typeToUse == HWAccelType::None) {
			utils::Logger::info("No hardware acceleration available");
			return false;
		}
	}
	
	// Create the hardware device context
	hwDeviceCtx = HardwareAcceleration::createHWDeviceContext(typeToUse, config.deviceIndex);
	if (!hwDeviceCtx) {
		utils::Logger::error("Failed to create hardware device context for type: {}", 
			HardwareAcceleration::hwAccelTypeToString(typeToUse));
		if (config.allowFallback) {
			utils::Logger::info("Hardware acceleration initialization failed, falling back to software");
			return false;
		}
		return false;
	}
	
	currentType = typeToUse;
	utils::Logger::info("Shared hardware context initialized successfully - type: {}, device: {}", 
		HardwareAcceleration::hwAccelTypeToString(currentType), config.deviceIndex);
	
	return true;
}

void HardwareContextManager::reset() {
	std::lock_guard<std::mutex> lock(mutex);
	
	if (hwDeviceCtx) {
		av_buffer_unref(&hwDeviceCtx);
		hwDeviceCtx = nullptr;
	}
	currentType = HWAccelType::None;
}

HardwareContextManager::~HardwareContextManager() {
	reset();
}

} // namespace media