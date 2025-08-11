#include "edl/EDLParser.h"
#include "utils/Logger.h"
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

void testSimpleEDL() {
	const char* testDataDir = std::getenv("TEST_DATA_DIR");
	std::string edlPath = testDataDir ? 
		fs::path(testDataDir) / "simple_single_clip.json" :
		"sample_edls/simple_single_clip.json";
	
	std::cout << "Testing simple EDL parsing: " << edlPath << std::endl;
	
	try {
		edl::EDL edl = edl::EDLParser::parse(edlPath);
		
		assert(edl.fps == 30);
		assert(edl.width == 1920);
		assert(edl.height == 1080);
		assert(edl.clips.size() == 1);
		
		const auto& clip = edl.clips[0];
		assert(clip.source.mediaId == "test_video.mp4");
		assert(clip.source.trackId == "V1");
		assert(clip.source.in == 0);
		assert(clip.source.out == 10);
		assert(clip.in == 0);
		assert(clip.out == 10);
		assert(clip.track.type == edl::Track::Video);
		assert(clip.track.number == 1);
		
		std::cout << "✓ Simple EDL test passed" << std::endl;
		
	} catch (const std::exception& e) {
		std::cerr << "✗ Simple EDL test failed: " << e.what() << std::endl;
		throw;
	}
}

void testComplexEDL() {
	const char* testDataDir = std::getenv("TEST_DATA_DIR");
	std::string edlPath = testDataDir ? 
		fs::path(testDataDir) / "multiple_clips_with_effects.json" :
		"sample_edls/multiple_clips_with_effects.json";
	
	std::cout << "Testing complex EDL parsing: " << edlPath << std::endl;
	
	try {
		edl::EDL edl = edl::EDLParser::parse(edlPath);
		
		assert(edl.fps == 30);
		assert(edl.width == 1920);
		assert(edl.height == 1080);
		assert(edl.clips.size() == 3);
		
		// Test first clip with fades
		const auto& clip1 = edl.clips[0];
		assert(clip1.source.mediaId == "clip1.mp4");
		assert(clip1.topFade == 1.0f);
		assert(clip1.tailFade == 0.5f);
		
		// Test second clip with motion and transition
		const auto& clip2 = edl.clips[1];
		assert(clip2.source.mediaId == "clip2.mp4");
		assert(clip2.motion.panX == 0.1f);
		assert(clip2.motion.panY == -0.1f);
		assert(clip2.motion.zoomX == 1.2f);
		assert(clip2.motion.zoomY == 1.2f);
		assert(clip2.motion.rotation == 5.0f);
		assert(clip2.transition.type == "dissolve");
		assert(clip2.transition.duration == 1.0);
		
		// Test third clip
		const auto& clip3 = edl.clips[2];
		assert(clip3.source.mediaId == "clip3.mp4");
		assert(clip3.tailFade == 2.0f);
		
		std::cout << "✓ Complex EDL test passed" << std::endl;
		
	} catch (const std::exception& e) {
		std::cerr << "✗ Complex EDL test failed: " << e.what() << std::endl;
		throw;
	}
}

void testInlineJSON() {
	std::cout << "Testing inline JSON parsing" << std::endl;
	
	nlohmann::json j = {
		{"fps", 24},
		{"width", 1280},
		{"height", 720},
		{"clips", nlohmann::json::array({
			{
				{"source", {
					{"mediaId", "test.mp4"},
					{"trackId", "V1"},
					{"in", 5.5},
					{"out", 15.5}
				}},
				{"in", 0},
				{"out", 10},
				{"track", {
					{"type", "video"},
					{"number", 1}
				}}
			}
		})}
	};
	
	try {
		edl::EDL edl = edl::EDLParser::parseJSON(j);
		
		assert(edl.fps == 24);
		assert(edl.width == 1280);
		assert(edl.height == 720);
		assert(edl.clips.size() == 1);
		
		const auto& clip = edl.clips[0];
		assert(clip.source.in == 5.5);
		assert(clip.source.out == 15.5);
		
		std::cout << "✓ Inline JSON test passed" << std::endl;
		
	} catch (const std::exception& e) {
		std::cerr << "✗ Inline JSON test failed: " << e.what() << std::endl;
		throw;
	}
}

int main() {
	utils::Logger::setLevel(utils::Logger::INFO);
	
	try {
		testSimpleEDL();
		testComplexEDL();
		testInlineJSON();
		
		std::cout << "\n✓ All tests passed!" << std::endl;
		return 0;
		
	} catch (const std::exception& e) {
		std::cerr << "\n✗ Test suite failed: " << e.what() << std::endl;
		return 1;
	}
}