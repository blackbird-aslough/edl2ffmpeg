#pragma once

#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>
#include <optional>

namespace test {

class EDLGenerator {
public:
	// Initialize with seed for reproducibility
	explicit EDLGenerator(unsigned int seed = std::random_device{}());
	
	// Configuration methods (fluent interface)
	EDLGenerator& withClips(int min, int max);
	EDLGenerator& withDuration(double seconds);
	EDLGenerator& withResolution(int width, int height);
	EDLGenerator& withFrameRate(int fps);
	EDLGenerator& withImplementedEffects();
	EDLGenerator& withEffects(const std::vector<std::string>& effectTypes);
	EDLGenerator& withFades(bool enable);
	EDLGenerator& withTransitions(bool enable);
	EDLGenerator& withVideoFile(const std::string& path);
	EDLGenerator& withMaxComplexity(int level);  // 1=simple, 5=complex
	
	// Generate the EDL
	nlohmann::json generate();
	
	// Generate specific EDL patterns for testing
	nlohmann::json generateSingleClip();
	nlohmann::json generateMultipleClips(int count);
	nlohmann::json generateWithEffect(const std::string& effectType, float strength);
	nlohmann::json generateWithFades(float topFade, float tailFade);
	nlohmann::json generateSequentialClips(int count, double clipDuration);
	
private:
	std::mt19937 rng_;
	
	// Configuration
	int minClips_ = 1;
	int maxClips_ = 1;
	double totalDuration_ = 5.0;
	int width_ = 1920;
	int height_ = 1080;
	int fps_ = 30;
	bool enableFades_ = false;
	bool enableTransitions_ = false;
	std::vector<std::string> effectTypes_;
	std::string videoFile_ = "fixtures/test_bars_1080p_30fps_10s.mp4";
	int maxComplexity_ = 3;
	
	// Helper methods
	double randomDouble(double min, double max);
	int randomInt(int min, int max);
	bool randomBool(double probability = 0.5);
	
	// Generate clip components
	nlohmann::json generateClip(double startTime, double endTime, int trackNumber);
	nlohmann::json generateMediaSource(double inPoint, double outPoint);
	nlohmann::json generateEffectSource(const std::string& type);
	nlohmann::json generateTrack(int number, const std::string& type = "video");
	nlohmann::json generateEffect(const std::string& type);
	nlohmann::json generateTransition(const std::string& type, double duration);
	nlohmann::json generateMotion();
	
	// Validate and adjust parameters
	void validateParameters();
};

// Predefined test EDL templates
namespace templates {
	// Basic single clip template
	nlohmann::json basicSingleClip(const std::string& videoFile, double duration);
	
	// Clip with brightness effect
	nlohmann::json clipWithBrightness(const std::string& videoFile, float brightness);
	
	// Clip with contrast effect
	nlohmann::json clipWithContrast(const std::string& videoFile, float contrast);
	
	// Clip with fade in/out
	nlohmann::json clipWithFades(const std::string& videoFile, float fadeIn, float fadeOut);
	
	// Multiple sequential clips
	nlohmann::json sequentialClips(const std::string& videoFile, int count, double clipDuration);
	
	// Complex EDL with multiple effects
	nlohmann::json complexEDL(const std::string& videoFile);
}

} // namespace test