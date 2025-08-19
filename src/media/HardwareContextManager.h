#pragma once

#include "media/HardwareAcceleration.h"
#include <memory>
#include <mutex>

extern "C" {
#include <libavutil/buffer.h>
}

namespace media {

/**
 * Manages a shared hardware device context for both decoding and encoding.
 * This ensures that frames can be passed between decoder and encoder without
 * unnecessary CPU transfers when using GPU acceleration.
 */
class HardwareContextManager {
public:
	/**
	 * Get the singleton instance of the hardware context manager
	 */
	static HardwareContextManager& getInstance();
	
	/**
	 * Initialize the hardware context with the specified configuration
	 * Must be called before getSharedContext()
	 * @param config Hardware configuration
	 * @return true if initialization succeeded, false otherwise
	 */
	bool initialize(const HWConfig& config);
	
	/**
	 * Get the shared hardware device context
	 * @return Hardware device context, or nullptr if not initialized
	 */
	AVBufferRef* getSharedContext() const { return hwDeviceCtx; }
	
	/**
	 * Get the hardware acceleration type being used
	 * @return Hardware acceleration type
	 */
	HWAccelType getHWType() const { return currentType; }
	
	/**
	 * Check if the manager has been initialized
	 * @return true if initialized, false otherwise
	 */
	bool isInitialized() const { return hwDeviceCtx != nullptr; }
	
	/**
	 * Reset the manager, releasing the hardware context
	 */
	void reset();
	
	// Destructor
	~HardwareContextManager();
	
private:
	// Private constructor for singleton
	HardwareContextManager() = default;
	
	// Delete copy constructor and assignment operator
	HardwareContextManager(const HardwareContextManager&) = delete;
	HardwareContextManager& operator=(const HardwareContextManager&) = delete;
	
	AVBufferRef* hwDeviceCtx = nullptr;
	HWAccelType currentType = HWAccelType::None;
	mutable std::mutex mutex;
};

} // namespace media