#pragma once

#include "compositor/CompositorInstruction.h"
#include "edl/EDLTypes.h"
#include <memory>
#include <vector>

namespace compositor {

class InstructionGenerator {
public:
	InstructionGenerator(const edl::EDL& edl);
	
	// Iterator for lazy evaluation
	class Iterator {
	public:
		Iterator(InstructionGenerator* generator, int frameNumber);
		
		CompositorInstruction operator*() const;
		Iterator& operator++();
		bool operator!=(const Iterator& other) const;
		
	private:
		InstructionGenerator* generator;
		int frameNumber;
		mutable CompositorInstruction current;
		mutable bool currentValid = false;
	};
	
	Iterator begin();
	Iterator end();
	
	// Direct access
	CompositorInstruction getInstructionForFrame(int frameNumber);
	
	int getTotalFrames() const { return totalFrames; }
	
private:
	double frameToTime(int frameNumber) const;
	int timeToFrame(double time) const;
	int64_t getSourceFrameNumber(const edl::Clip& clip, int timelineFrame) const;
	CompositorInstruction createInstruction(const edl::Clip& clip, int frameNumber);
	const edl::Clip* findClipAtFrame(int frameNumber, int trackNumber = 1) const;
	
	edl::EDL edl;
	int totalFrames;
	double frameDuration;  // Duration of one frame in seconds
};

} // namespace compositor