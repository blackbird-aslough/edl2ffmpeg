#include "EDLGenerator.h"
#include <algorithm>
#include <cmath>

namespace test {

EDLGenerator::EDLGenerator(unsigned int seed) : rng_(seed) {
	// Set default implemented effects
	effectTypes_ = {"brightness", "contrast"};
}

EDLGenerator& EDLGenerator::withClips(int min, int max) {
	minClips_ = min;
	maxClips_ = max;
	return *this;
}

EDLGenerator& EDLGenerator::withDuration(double seconds) {
	totalDuration_ = seconds;
	return *this;
}

EDLGenerator& EDLGenerator::withResolution(int width, int height) {
	width_ = width;
	height_ = height;
	return *this;
}

EDLGenerator& EDLGenerator::withFrameRate(int fps) {
	fps_ = fps;
	return *this;
}

EDLGenerator& EDLGenerator::withImplementedEffects() {
	effectTypes_ = {"brightness", "contrast"};
	return *this;
}

EDLGenerator& EDLGenerator::withEffects(const std::vector<std::string>& effectTypes) {
	effectTypes_ = effectTypes;
	return *this;
}

EDLGenerator& EDLGenerator::withFades(bool enable) {
	enableFades_ = enable;
	return *this;
}

EDLGenerator& EDLGenerator::withTransitions(bool enable) {
	enableTransitions_ = enable;
	return *this;
}

EDLGenerator& EDLGenerator::withVideoFile(const std::string& path) {
	videoFile_ = path;
	return *this;
}

EDLGenerator& EDLGenerator::withMaxComplexity(int level) {
	maxComplexity_ = std::clamp(level, 1, 5);
	return *this;
}

nlohmann::json EDLGenerator::generate() {
	validateParameters();
	
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	// Generate clips
	int numClips = randomInt(minClips_, maxClips_);
	double clipDuration = totalDuration_ / numClips;
	
	nlohmann::json clips = nlohmann::json::array();
	
	for (int i = 0; i < numClips; i++) {
		double startTime = i * clipDuration;
		double endTime = (i + 1) * clipDuration;
		
		auto clip = generateClip(startTime, endTime, 1);
		clips.push_back(clip);
	}
	
	edl["clips"] = clips;
	
	return edl;
}

nlohmann::json EDLGenerator::generateSingleClip() {
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	auto clip = generateClip(0, totalDuration_, 1);
	edl["clips"] = nlohmann::json::array({clip});
	
	return edl;
}

nlohmann::json EDLGenerator::generateMultipleClips(int count) {
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	double clipDuration = totalDuration_ / count;
	nlohmann::json clips = nlohmann::json::array();
	
	for (int i = 0; i < count; i++) {
		double startTime = i * clipDuration;
		double endTime = (i + 1) * clipDuration;
		
		auto clip = generateClip(startTime, endTime, 1);
		clips.push_back(clip);
	}
	
	edl["clips"] = clips;
	return edl;
}

nlohmann::json EDLGenerator::generateWithEffect(const std::string& effectType, float strength) {
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	auto clip = generateClip(0, totalDuration_, 1);
	
	// Add effect
	nlohmann::json effect;
	effect["type"] = effectType;
	effect["strength"] = strength;
	clip["effects"] = nlohmann::json::array({effect});
	
	edl["clips"] = nlohmann::json::array({clip});
	return edl;
}

nlohmann::json EDLGenerator::generateWithFades(float topFade, float tailFade) {
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	auto clip = generateClip(0, totalDuration_, 1);
	clip["topFade"] = topFade;
	clip["tailFade"] = tailFade;
	
	edl["clips"] = nlohmann::json::array({clip});
	return edl;
}

nlohmann::json EDLGenerator::generateSequentialClips(int count, double clipDuration) {
	nlohmann::json edl;
	edl["fps"] = fps_;
	edl["width"] = width_;
	edl["height"] = height_;
	
	nlohmann::json clips = nlohmann::json::array();
	
	for (int i = 0; i < count; i++) {
		double startTime = i * clipDuration;
		double endTime = (i + 1) * clipDuration;
		
		auto clip = generateClip(startTime, endTime, 1);
		clips.push_back(clip);
	}
	
	edl["clips"] = clips;
	return edl;
}

double EDLGenerator::randomDouble(double min, double max) {
	std::uniform_real_distribution<double> dist(min, max);
	return dist(rng_);
}

int EDLGenerator::randomInt(int min, int max) {
	std::uniform_int_distribution<int> dist(min, max);
	return dist(rng_);
}

bool EDLGenerator::randomBool(double probability) {
	std::bernoulli_distribution dist(probability);
	return dist(rng_);
}

nlohmann::json EDLGenerator::generateClip(double startTime, double endTime, int trackNumber) {
	nlohmann::json clip;
	
	clip["in"] = startTime;
	clip["out"] = endTime;
	clip["track"] = generateTrack(trackNumber);
	
	// Add media source
	double duration = endTime - startTime;
	clip["source"] = generateMediaSource(0, duration);
	
	// Randomly add effects based on complexity
	if (maxComplexity_ >= 2 && !effectTypes_.empty() && randomBool(0.3)) {
		nlohmann::json effects = nlohmann::json::array();
		
		// Add 1-2 effects
		int numEffects = randomInt(1, std::min(2, static_cast<int>(effectTypes_.size())));
		for (int i = 0; i < numEffects; i++) {
			int effectIndex = randomInt(0, effectTypes_.size() - 1);
			effects.push_back(generateEffect(effectTypes_[effectIndex]));
		}
		
		clip["effects"] = effects;
	}
	
	// Add fades if enabled
	if (enableFades_ && randomBool(0.4)) {
		if (randomBool(0.5)) {
			clip["topFade"] = randomDouble(0.5, 2.0);
		}
		if (randomBool(0.5)) {
			clip["tailFade"] = randomDouble(0.5, 2.0);
		}
	}
	
	// Add motion if complex enough
	if (maxComplexity_ >= 4 && randomBool(0.2)) {
		clip["motion"] = generateMotion();
	}
	
	return clip;
}

nlohmann::json EDLGenerator::generateMediaSource(double inPoint, double outPoint) {
	nlohmann::json source;
	source["uri"] = videoFile_;
	source["trackId"] = "V1";
	source["in"] = inPoint;
	source["out"] = outPoint;
	return source;
}

nlohmann::json EDLGenerator::generateEffectSource(const std::string& type) {
	nlohmann::json source;
	source["type"] = type;
	
	if (type == "brightness" || type == "contrast") {
		source["data"]["value"] = randomDouble(0.5, 1.5);
	}
	
	return source;
}

nlohmann::json EDLGenerator::generateTrack(int number, const std::string& type) {
	nlohmann::json track;
	track["type"] = type;
	track["number"] = number;
	return track;
}

nlohmann::json EDLGenerator::generateEffect(const std::string& type) {
	nlohmann::json effect;
	effect["type"] = type;
	
	if (type == "brightness") {
		effect["strength"] = randomDouble(0.5, 1.5);
	} else if (type == "contrast") {
		effect["strength"] = randomDouble(0.5, 1.5);
	} else if (type == "saturation") {
		effect["strength"] = randomDouble(0.0, 2.0);
	}
	
	return effect;
}

nlohmann::json EDLGenerator::generateTransition(const std::string& type, double duration) {
	nlohmann::json transition;
	transition["type"] = type;
	transition["duration"] = duration;
	return transition;
}

nlohmann::json EDLGenerator::generateMotion() {
	nlohmann::json motion;
	
	// Small random motion
	motion["panX"] = randomDouble(-0.2, 0.2);
	motion["panY"] = randomDouble(-0.2, 0.2);
	motion["zoomX"] = randomDouble(0.8, 1.2);
	motion["zoomY"] = randomDouble(0.8, 1.2);
	motion["rotation"] = randomDouble(-10, 10);
	
	return motion;
}

void EDLGenerator::validateParameters() {
	if (minClips_ < 1) minClips_ = 1;
	if (maxClips_ < minClips_) maxClips_ = minClips_;
	if (totalDuration_ <= 0) totalDuration_ = 5.0;
	if (width_ <= 0) width_ = 1920;
	if (height_ <= 0) height_ = 1080;
	if (fps_ <= 0) fps_ = 30;
}

// Template implementations
namespace templates {

nlohmann::json basicSingleClip(const std::string& videoFile, double duration) {
	nlohmann::json edl;
	edl["fps"] = 30;
	edl["width"] = 1920;
	edl["height"] = 1080;
	
	nlohmann::json clip;
	clip["in"] = 0;
	clip["out"] = duration;
	clip["track"]["type"] = "video";
	clip["track"]["number"] = 1;
	clip["source"]["uri"] = videoFile;
	clip["source"]["trackId"] = "V1";
	clip["source"]["in"] = 0;
	clip["source"]["out"] = duration;
	
	edl["clips"] = nlohmann::json::array({clip});
	return edl;
}

nlohmann::json clipWithBrightness(const std::string& videoFile, float brightness) {
	auto edl = basicSingleClip(videoFile, 5.0);
	
	nlohmann::json effect;
	effect["type"] = "brightness";
	effect["strength"] = brightness;
	
	edl["clips"][0]["effects"] = nlohmann::json::array({effect});
	return edl;
}

nlohmann::json clipWithContrast(const std::string& videoFile, float contrast) {
	auto edl = basicSingleClip(videoFile, 5.0);
	
	nlohmann::json effect;
	effect["type"] = "contrast";
	effect["strength"] = contrast;
	
	edl["clips"][0]["effects"] = nlohmann::json::array({effect});
	return edl;
}

nlohmann::json clipWithFades(const std::string& videoFile, float fadeIn, float fadeOut) {
	auto edl = basicSingleClip(videoFile, 5.0);
	
	edl["clips"][0]["topFade"] = fadeIn;
	edl["clips"][0]["tailFade"] = fadeOut;
	
	return edl;
}

nlohmann::json sequentialClips(const std::string& videoFile, int count, double clipDuration) {
	nlohmann::json edl;
	edl["fps"] = 30;
	edl["width"] = 1920;
	edl["height"] = 1080;
	
	nlohmann::json clips = nlohmann::json::array();
	
	for (int i = 0; i < count; i++) {
		nlohmann::json clip;
		clip["in"] = i * clipDuration;
		clip["out"] = (i + 1) * clipDuration;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = videoFile;
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = i * clipDuration;
		clip["source"]["out"] = (i + 1) * clipDuration;
		
		clips.push_back(clip);
	}
	
	edl["clips"] = clips;
	return edl;
}

nlohmann::json complexEDL(const std::string& videoFile) {
	nlohmann::json edl;
	edl["fps"] = 30;
	edl["width"] = 1920;
	edl["height"] = 1080;
	
	nlohmann::json clips = nlohmann::json::array();
	
	// Clip 1: Fade in with brightness
	nlohmann::json clip1;
	clip1["in"] = 0;
	clip1["out"] = 3;
	clip1["track"]["type"] = "video";
	clip1["track"]["number"] = 1;
	clip1["source"]["uri"] = videoFile;
	clip1["source"]["trackId"] = "V1";
	clip1["source"]["in"] = 0;
	clip1["source"]["out"] = 3;
	clip1["topFade"] = 1.0;
	clip1["effects"] = nlohmann::json::array({
		{{"type", "brightness"}, {"strength", 1.2}}
	});
	clips.push_back(clip1);
	
	// Clip 2: Contrast adjustment
	nlohmann::json clip2;
	clip2["in"] = 3;
	clip2["out"] = 6;
	clip2["track"]["type"] = "video";
	clip2["track"]["number"] = 1;
	clip2["source"]["uri"] = videoFile;
	clip2["source"]["trackId"] = "V1";
	clip2["source"]["in"] = 3;
	clip2["source"]["out"] = 6;
	clip2["effects"] = nlohmann::json::array({
		{{"type", "contrast"}, {"strength", 1.5}}
	});
	clips.push_back(clip2);
	
	// Clip 3: Fade out with both effects
	nlohmann::json clip3;
	clip3["in"] = 6;
	clip3["out"] = 10;
	clip3["track"]["type"] = "video";
	clip3["track"]["number"] = 1;
	clip3["source"]["uri"] = videoFile;
	clip3["source"]["trackId"] = "V1";
	clip3["source"]["in"] = 6;
	clip3["source"]["out"] = 10;
	clip3["tailFade"] = 1.5;
	clip3["effects"] = nlohmann::json::array({
		{{"type", "brightness"}, {"strength", 0.8}},
		{{"type", "contrast"}, {"strength", 0.7}}
	});
	clips.push_back(clip3);
	
	edl["clips"] = clips;
	return edl;
}

} // namespace templates
} // namespace test