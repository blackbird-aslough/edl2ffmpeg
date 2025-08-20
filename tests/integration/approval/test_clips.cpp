#include <catch2/catch_all.hpp>
#include "../common/TestRunner.h"
#include "../common/EDLGenerator.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Single clip renders correctly", "[approval][clips][quick]") {
	test::TestRunner runner;
	runner.setVerbose(true);  // Enable verbose for debugging
	
	auto edl = test::templates::basicSingleClip(
		"fixtures/test_bars_1080p_30fps_10s.mp4", 3.0);
	
	std::string edlPath = "approval/fixtures/single_clip.json";
	fs::create_directories(fs::path(edlPath).parent_path());
	std::ofstream file(edlPath);
	file << edl.dump(2);
	file.close();
	
	auto result = runner.compareRenders(edlPath);
	
	if (!result.completed) {
		INFO("Error: " << result.errorMsg);
		INFO("Our output: " << runner.getLastOurOutput());
		INFO("Ref output: " << runner.getLastRefOutput());
	}
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 40.0);  // Should be nearly identical
	CHECK(result.isVisuallyIdentical());
	CHECK(result.totalFrames == 90);  // 3 seconds at 30fps
	
	INFO(result.summary());
}

TEST_CASE("Multiple sequential clips render correctly", "[approval][clips]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("Two clips") {
		auto edl = test::templates::sequentialClips(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 2, 2.0);
		
		std::string edlPath = "approval/fixtures/two_clips.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
		CHECK(result.totalFrames == 120);  // 4 seconds at 30fps
	}
	
	SECTION("Five clips") {
		auto edl = test::templates::sequentialClips(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 5, 1.0);
		
		std::string edlPath = "approval/fixtures/five_clips.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
		CHECK(result.totalFrames == 150);  // 5 seconds at 30fps
	}
}

TEST_CASE("Clips with different frame rates", "[approval][clips][framerates]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("24fps source to 30fps output") {
		nlohmann::json edl;
		edl["fps"] = 30;  // Output frame rate
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clip;
		clip["in"] = 0;
		clip["out"] = 3;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = "fixtures/test_bars_1080p_24fps_5s.mp4";
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = 0;
		clip["source"]["out"] = 3;
		
		edl["clips"] = nlohmann::json::array({clip});
		
		std::string edlPath = "approval/fixtures/framerate_24to30.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 30.0);  // Frame rate conversion may cause some differences
		CHECK(result.totalFrames == 90);  // 3 seconds at 30fps output
	}
	
	SECTION("60fps source to 30fps output") {
		nlohmann::json edl;
		edl["fps"] = 30;  // Output frame rate
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clip;
		clip["in"] = 0;
		clip["out"] = 2;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = "fixtures/test_bars_720p_60fps_5s.mp4";
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = 0;
		clip["source"]["out"] = 2;
		
		edl["clips"] = nlohmann::json::array({clip});
		
		std::string edlPath = "approval/fixtures/framerate_60to30.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.totalFrames == 60);  // 2 seconds at 30fps output
	}
}

TEST_CASE("Clips with different resolutions", "[approval][clips][resolutions]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("720p source to 1080p output") {
		nlohmann::json edl;
		edl["fps"] = 30;
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clip;
		clip["in"] = 0;
		clip["out"] = 2;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = "fixtures/test_bars_720p_60fps_5s.mp4";
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = 0;
		clip["source"]["out"] = 2;
		
		edl["clips"] = nlohmann::json::array({clip});
		
		std::string edlPath = "approval/fixtures/resolution_720to1080.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 30.0);  // Scaling may cause some differences
	}
	
	SECTION("480p source to 1080p output") {
		nlohmann::json edl;
		edl["fps"] = 30;
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clip;
		clip["in"] = 0;
		clip["out"] = 2;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = "fixtures/test_bars_480p_30fps_5s.mp4";
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = 0;
		clip["source"]["out"] = 2;
		
		edl["clips"] = nlohmann::json::array({clip});
		
		std::string edlPath = "approval/fixtures/resolution_480to1080.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 25.0);  // Lower quality due to upscaling
	}
}

TEST_CASE("Frame-accurate seeking", "[approval][clips][seeking]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("Seek to middle of clip") {
		nlohmann::json edl;
		edl["fps"] = 30;
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clip;
		clip["in"] = 0;
		clip["out"] = 2;
		clip["track"]["type"] = "video";
		clip["track"]["number"] = 1;
		clip["source"]["uri"] = "fixtures/counter_1080p_30fps_10s.mp4";  // Has frame counter
		clip["source"]["trackId"] = "V1";
		clip["source"]["in"] = 5;  // Start from 5 seconds in
		clip["source"]["out"] = 7;  // End at 7 seconds
		
		edl["clips"] = nlohmann::json::array({clip});
		
		std::string edlPath = "approval/fixtures/seek_middle.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
		CHECK(result.totalFrames == 60);  // 2 seconds at 30fps
	}
	
	SECTION("Multiple seeks in sequence") {
		nlohmann::json edl;
		edl["fps"] = 30;
		edl["width"] = 1920;
		edl["height"] = 1080;
		
		nlohmann::json clips = nlohmann::json::array();
		
		// Clip 1: frames 0-30 (1 second)
		nlohmann::json clip1;
		clip1["in"] = 0;
		clip1["out"] = 1;
		clip1["track"]["type"] = "video";
		clip1["track"]["number"] = 1;
		clip1["source"]["uri"] = "fixtures/counter_1080p_30fps_10s.mp4";
		clip1["source"]["trackId"] = "V1";
		clip1["source"]["in"] = 0;
		clip1["source"]["out"] = 1;
		clips.push_back(clip1);
		
		// Clip 2: frames 240-270 (8-9 seconds)
		nlohmann::json clip2;
		clip2["in"] = 1;
		clip2["out"] = 2;
		clip2["track"]["type"] = "video";
		clip2["track"]["number"] = 1;
		clip2["source"]["uri"] = "fixtures/counter_1080p_30fps_10s.mp4";
		clip2["source"]["trackId"] = "V1";
		clip2["source"]["in"] = 8;
		clip2["source"]["out"] = 9;
		clips.push_back(clip2);
		
		// Clip 3: frames 90-120 (3-4 seconds) - backward seek
		nlohmann::json clip3;
		clip3["in"] = 2;
		clip3["out"] = 3;
		clip3["track"]["type"] = "video";
		clip3["track"]["number"] = 1;
		clip3["source"]["uri"] = "fixtures/counter_1080p_30fps_10s.mp4";
		clip3["source"]["trackId"] = "V1";
		clip3["source"]["in"] = 3;
		clip3["source"]["out"] = 4;
		clips.push_back(clip3);
		
		edl["clips"] = clips;
		
		std::string edlPath = "approval/fixtures/multiple_seeks.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
		CHECK(result.totalFrames == 90);  // 3 seconds at 30fps
	}
}