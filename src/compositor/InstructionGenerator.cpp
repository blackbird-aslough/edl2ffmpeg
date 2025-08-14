#include "compositor/InstructionGenerator.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>

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
	
	return createInstruction(*clip, frameNumber);
}

CompositorInstruction InstructionGenerator::createInstruction(const edl::Clip& clip,
	int frameNumber) {
	
	CompositorInstruction instruction;
	instruction.type = CompositorInstruction::DrawFrame;
	instruction.trackNumber = clip.track.number;
	instruction.uri = clip.source.uri;
	
	// Calculate source frame number
	instruction.sourceFrameNumber = getSourceFrameNumber(clip, frameNumber);
	
	// Apply motion parameters
	instruction.panX = clip.motion.panX;
	instruction.panY = clip.motion.panY;
	instruction.zoomX = clip.motion.zoomX;
	instruction.zoomY = clip.motion.zoomY;
	instruction.rotation = clip.motion.rotation;
	instruction.flip = clip.source.flip;
	
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
	
	return instruction;
}

const edl::Clip* InstructionGenerator::findClipAtFrame(int frameNumber,
	int trackNumber) const {
	
	double frameTime = frameToTime(frameNumber);
	
	for (const auto& clip : edl.clips) {
		if (clip.track.type == edl::Track::Video &&
			clip.track.number == trackNumber &&
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
	
	// Calculate source time
	double sourceTime = clip.source.in + positionInClip;
	
	// Convert to source frame number
	// If source has different fps, use it; otherwise use EDL fps
	int sourceFps = clip.source.fps > 0 ? clip.source.fps : edl.fps;
	return static_cast<int64_t>(sourceTime * sourceFps);
}

double InstructionGenerator::frameToTime(int frameNumber) const {
	return frameNumber * frameDuration;
}

int InstructionGenerator::timeToFrame(double time) const {
	return static_cast<int>(std::round(time * edl.fps));
}

} // namespace compositor