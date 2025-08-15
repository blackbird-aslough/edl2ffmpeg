#include "compositor/InstructionGenerator.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <variant>

namespace compositor {

InstructionGenerator::InstructionGenerator(const edl::EDL& edl)
	: edl(edl)
	, totalFrames(0)
	, frameDuration(1.0 / edl.fps) {
	
	// Calculate total frames from the last clip's out point
	double maxTime = 0.0;
	for (const auto& clip : edl.clips) {
		maxTime = std::max(maxTime, clip.out);
	}
	
	totalFrames = timeToFrame(maxTime);
	
	utils::Logger::info("Instruction generator initialized: {} total frames @ {} fps",
		totalFrames, edl.fps);
}

InstructionGenerator::Iterator::Iterator(InstructionGenerator* generator, int frameNumber)
	: generator(generator)
	, frameNumber(frameNumber) {
}

CompositorInstruction InstructionGenerator::Iterator::operator*() const {
	if (!currentValid) {
		current = generator->getInstructionForFrame(frameNumber);
		currentValid = true;
	}
	return current;
}

InstructionGenerator::Iterator& InstructionGenerator::Iterator::operator++() {
	++frameNumber;
	currentValid = false;
	return *this;
}

bool InstructionGenerator::Iterator::operator!=(const Iterator& other) const {
	return frameNumber != other.frameNumber;
}

InstructionGenerator::Iterator InstructionGenerator::begin() {
	return Iterator(this, 0);
}

InstructionGenerator::Iterator InstructionGenerator::end() {
	return Iterator(this, totalFrames);
}

CompositorInstruction InstructionGenerator::getInstructionForFrame(int frameNumber) {
	// Find the clip that should be displayed at this frame
	const edl::Clip* clip = findClipAtFrame(frameNumber);
	
	if (!clip) {
		// No clip at this frame, return a NoOp or black frame
		CompositorInstruction instruction;
		instruction.type = CompositorInstruction::GenerateColor;
		instruction.color.r = 0.0f;
		instruction.color.g = 0.0f;
		instruction.color.b = 0.0f;
		return instruction;
	}
	
	auto instruction = createInstruction(*clip, frameNumber);
	
	// Look for effect clips on the same track
	const edl::Clip* effectClip = findEffectClipAtFrame(frameNumber, clip->track.number);
	if (effectClip) {
		applyEffectClip(instruction, *effectClip, frameNumber);
	}
	
	return instruction;
}

CompositorInstruction InstructionGenerator::createInstruction(const edl::Clip& clip,
	int frameNumber) {
	
	CompositorInstruction instruction;
	instruction.type = CompositorInstruction::DrawFrame;
	instruction.trackNumber = clip.track.number;
	
	// Get URI from MediaSource if available
	if (std::holds_alternative<edl::MediaSource>(clip.source)) {
		const auto& mediaSource = std::get<edl::MediaSource>(clip.source);
		instruction.uri = mediaSource.uri;
		instruction.flip = mediaSource.flip;
	} else {
		// Effect source - no URI
		instruction.uri = "";
	}
	
	// Calculate source frame number
	instruction.sourceFrameNumber = getSourceFrameNumber(clip, frameNumber);
	
	// Apply motion parameters
	instruction.panX = clip.motion.panX;
	instruction.panY = clip.motion.panY;
	instruction.zoomX = clip.motion.zoomX;
	instruction.zoomY = clip.motion.zoomY;
	instruction.rotation = clip.motion.rotation;
	
	// Calculate fade
	double frameTime = frameToTime(frameNumber);
	double clipDuration = clip.out - clip.in;
	double positionInClip = frameTime - clip.in;
	
	instruction.fade = 1.0f;
	
	// Apply top fade
	if (clip.topFade > 0 && positionInClip < clip.topFade) {
		instruction.fade = static_cast<float>(positionInClip / clip.topFade);
	}
	
	// Apply tail fade
	double tailStart = clipDuration - clip.tailFade;
	if (clip.tailFade > 0 && positionInClip > tailStart) {
		float tailFade = static_cast<float>((clipDuration - positionInClip) / clip.tailFade);
		instruction.fade = std::min(instruction.fade, tailFade);
	}
	
	// Handle transition
	if (!clip.transition.type.empty() && clip.transition.duration > 0) {
		if (positionInClip < clip.transition.duration) {
			instruction.transition.duration = static_cast<float>(clip.transition.duration);
			instruction.transition.progress = static_cast<float>(positionInClip / clip.transition.duration);
			
			if (clip.transition.type == "dissolve") {
				instruction.transition.type = TransitionInfo::Dissolve;
			} else if (clip.transition.type == "wipe") {
				instruction.transition.type = TransitionInfo::Wipe;
			} else if (clip.transition.type == "slide") {
				instruction.transition.type = TransitionInfo::Slide;
			}
		}
	}
	
	// Handle simple inline effects (backward compatibility)
	for (const auto& effect : clip.effects) {
		Effect compEffect;
		if (effect.type == "brightness") {
			compEffect.type = Effect::Brightness;
			compEffect.strength = effect.strength;
		} else if (effect.type == "contrast") {
			compEffect.type = Effect::Contrast;
			compEffect.strength = effect.strength;
		} else if (effect.type == "saturation") {
			compEffect.type = Effect::Saturation;
			compEffect.strength = effect.strength;
		}
		instruction.effects.push_back(compEffect);
	}
	
	return instruction;
}

const edl::Clip* InstructionGenerator::findClipAtFrame(int frameNumber,
	int trackNumber) const {
	
	double frameTime = frameToTime(frameNumber);
	
	for (const auto& clip : edl.clips) {
		if (clip.track.type == edl::Track::Video &&
			clip.track.number == trackNumber &&
			clip.track.subtype != "effects" &&  // Skip effect clips
			frameTime >= clip.in &&
			frameTime < clip.out) {
			return &clip;
		}
	}
	
	return nullptr;
}

int64_t InstructionGenerator::getSourceFrameNumber(const edl::Clip& clip,
	int timelineFrame) const {
	
	// Convert timeline frame to time
	double timelineTime = frameToTime(timelineFrame);
	
	// Calculate position within the clip
	double positionInClip = timelineTime - clip.in;
	
	// Check if source is a MediaSource (only media sources have frame numbers)
	if (std::holds_alternative<edl::MediaSource>(clip.source)) {
		const auto& mediaSource = std::get<edl::MediaSource>(clip.source);
		// Calculate source time
		double sourceTime = mediaSource.in + positionInClip;
		// Convert to source frame number
		// If source has different fps, use it; otherwise use EDL fps
		int sourceFps = mediaSource.fps > 0 ? mediaSource.fps : edl.fps;
		return static_cast<int64_t>(sourceTime * sourceFps);
	}
	
	return 0;  // Effect sources don't have frame numbers
}

double InstructionGenerator::frameToTime(int frameNumber) const {
	return frameNumber * frameDuration;
}

int InstructionGenerator::timeToFrame(double time) const {
	return static_cast<int>(std::round(time * edl.fps));
}

const edl::Clip* InstructionGenerator::findEffectClipAtFrame(int frameNumber,
	int trackNumber) const {
	
	double frameTime = frameToTime(frameNumber);
	
	for (const auto& clip : edl.clips) {
		if (clip.track.type == edl::Track::Video &&
			clip.track.number == trackNumber &&
			clip.track.subtype == "effects" &&
			frameTime >= clip.in &&
			frameTime < clip.out) {
			return &clip;
		}
	}
	
	return nullptr;
}

void InstructionGenerator::applyEffectClip(CompositorInstruction& instruction,
	const edl::Clip& effectClip, int frameNumber) {
	
	// Check if source is an EffectSource
	if (!std::holds_alternative<edl::EffectSource>(effectClip.source)) {
		return;
	}
	
	const auto& effectSource = std::get<edl::EffectSource>(effectClip.source);
	double frameTime = frameToTime(frameNumber);
	double timeInClip = frameTime - effectClip.in;
	
	// Process insideMaskFilters (for now, apply to entire frame)
	for (const auto& filter : effectSource.insideMaskFilters) {
		if (filter.type == "brightness") {
			Effect effect;
			effect.type = Effect::Brightness;
			effect.useLinearMapping = true;
			effect.linearMapping = interpolateLinearMapping(filter, timeInClip);
			instruction.effects.push_back(effect);
		}
		// Add other filter types as needed
	}
}

std::vector<LinearMapping> InstructionGenerator::interpolateLinearMapping(
	const edl::Filter& filter, double timeInClip) {
	
	if (filter.controlPoints.empty()) {
		return {};
	}
	
	// Find the appropriate control points for this time
	const edl::FilterControlPoint* prevCP = nullptr;
	const edl::FilterControlPoint* nextCP = nullptr;
	
	for (const auto& cp : filter.controlPoints) {
		if (cp.point <= timeInClip) {
			prevCP = &cp;
		} else if (!nextCP) {
			nextCP = &cp;
			break;
		}
	}
	
	// If we only have one control point or time is before/after all points
	if (!prevCP && !nextCP) {
		return {};
	} else if (!prevCP) {
		// Use the first control point
		std::vector<LinearMapping> result;
		for (const auto& mapping : nextCP->linear) {
			result.push_back({mapping.src, mapping.dst});
		}
		return result;
	} else if (!nextCP) {
		// Use the last control point
		std::vector<LinearMapping> result;
		for (const auto& mapping : prevCP->linear) {
			result.push_back({mapping.src, mapping.dst});
		}
		return result;
	}
	
	// Interpolate between two control points
	double t = (timeInClip - prevCP->point) / (nextCP->point - prevCP->point);
	
	// For now, use linear interpolation of the dst values
	// Ensure both control points have the same number of mappings
	if (prevCP->linear.size() != nextCP->linear.size()) {
		// Use the previous control point if sizes don't match
		std::vector<LinearMapping> result;
		for (const auto& mapping : prevCP->linear) {
			result.push_back({mapping.src, mapping.dst});
		}
		return result;
	}
	
	std::vector<LinearMapping> result;
	for (size_t i = 0; i < prevCP->linear.size(); ++i) {
		LinearMapping mapping;
		mapping.src = prevCP->linear[i].src;  // src values should be the same
		mapping.dst = prevCP->linear[i].dst * (1.0 - t) + nextCP->linear[i].dst * t;
		result.push_back(mapping);
	}
	
	return result;
}

} // namespace compositor