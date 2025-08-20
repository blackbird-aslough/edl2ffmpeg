#include <catch2/catch_all.hpp>
#include "../common/TestRunner.h"
#include "../common/EDLGenerator.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Brightness effect renders correctly", "[approval][effects][quick]") {
	test::TestRunner runner;
	runner.setVerbose(true);  // Enable verbose for debugging
	
	SECTION("50% brightness") {
		auto edl = test::templates::clipWithBrightness(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 0.5f);
		
		// Save EDL to file
		std::string edlPath = "approval/fixtures/brightness_50percent.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 25.0);  // Allow for brightness effect and encoder differences
		CHECK(result.avgPSNR < 40.0);  // But should show effect applied
		
		// Check or update golden checksums
		std::string checksumPath = "approval/approved/brightness_50percent.checksums";
		if (std::getenv("UPDATE_GOLDEN")) {
			fs::create_directories(fs::path(checksumPath).parent_path());
			result.saveChecksums(checksumPath);
			INFO("Updated golden checksums: " << checksumPath);
		} else if (fs::exists(checksumPath)) {
			CHECK(result.matchesChecksums(checksumPath));
		}
	}
	
	SECTION("150% brightness") {
		auto edl = test::templates::clipWithBrightness(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 1.5f);
		
		std::string edlPath = "approval/fixtures/brightness_150percent.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 25.0);  // Allow for brightness effect and encoder differences
		CHECK(result.avgPSNR < 40.0);  // But should show effect applied
		
		std::string checksumPath = "approval/approved/brightness_150percent.checksums";
		if (std::getenv("UPDATE_GOLDEN")) {
			fs::create_directories(fs::path(checksumPath).parent_path());
			result.saveChecksums(checksumPath);
			INFO("Updated golden checksums: " << checksumPath);
		} else if (fs::exists(checksumPath)) {
			CHECK(result.matchesChecksums(checksumPath));
		}
	}
	
	SECTION("Normal brightness (100%)") {
		auto edl = test::templates::clipWithBrightness(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 1.0f);
		
		std::string edlPath = "approval/fixtures/brightness_100percent.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);  // Should be nearly identical with encoder differences
		CHECK(result.avgPSNR < 50.0);  // But still very similar
	}
}

TEST_CASE("Contrast effect renders correctly", "[approval][effects]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("50% contrast") {
		auto edl = test::templates::clipWithContrast(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 0.5f);
		
		std::string edlPath = "approval/fixtures/contrast_50percent.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 25.0);  // Allow for brightness effect and encoder differences
		CHECK(result.avgPSNR < 40.0);  // But should show effect applied
		
		std::string checksumPath = "approval/approved/contrast_50percent.checksums";
		if (std::getenv("UPDATE_GOLDEN")) {
			fs::create_directories(fs::path(checksumPath).parent_path());
			result.saveChecksums(checksumPath);
			INFO("Updated golden checksums: " << checksumPath);
		} else if (fs::exists(checksumPath)) {
			CHECK(result.matchesChecksums(checksumPath));
		}
	}
	
	SECTION("150% contrast") {
		auto edl = test::templates::clipWithContrast(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 1.5f);
		
		std::string edlPath = "approval/fixtures/contrast_150percent.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 25.0);  // Allow for brightness effect and encoder differences
		CHECK(result.avgPSNR < 40.0);  // But should show effect applied
		
		std::string checksumPath = "approval/approved/contrast_150percent.checksums";
		if (std::getenv("UPDATE_GOLDEN")) {
			fs::create_directories(fs::path(checksumPath).parent_path());
			result.saveChecksums(checksumPath);
			INFO("Updated golden checksums: " << checksumPath);
		} else if (fs::exists(checksumPath)) {
			CHECK(result.matchesChecksums(checksumPath));
		}
	}
}

TEST_CASE("Fade effects render correctly", "[approval][effects]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("Fade in only") {
		auto edl = test::templates::clipWithFades(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 1.0f, 0.0f);
		
		std::string edlPath = "approval/fixtures/fade_in.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
	}
	
	SECTION("Fade out only") {
		auto edl = test::templates::clipWithFades(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 0.0f, 1.0f);
		
		std::string edlPath = "approval/fixtures/fade_out.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
	}
	
	SECTION("Fade in and out") {
		auto edl = test::templates::clipWithFades(
			"fixtures/test_bars_1080p_30fps_10s.mp4", 1.0f, 1.5f);
		
		std::string edlPath = "approval/fixtures/fade_in_out.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 35.0);
		CHECK(result.isVisuallyIdentical());
	}
}

TEST_CASE("Combined effects render correctly", "[approval][effects]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	SECTION("Complex EDL with multiple effects") {
		auto edl = test::templates::complexEDL(
			"fixtures/test_bars_1080p_30fps_10s.mp4");
		
		std::string edlPath = "approval/fixtures/complex_effects.json";
		fs::create_directories(fs::path(edlPath).parent_path());
		std::ofstream file(edlPath);
		file << edl.dump(2);
		file.close();
		
		auto result = runner.compareRenders(edlPath);
		
		if (!result.completed) {
			INFO("Error: " << result.errorMsg);
			INFO("EDL Path: " << edlPath);
		}
		
		REQUIRE(result.completed);
		CHECK(result.avgPSNR > 30.0);
		CHECK(result.maxFrameDiff < 10);  // More tolerance for complex EDL
		
		INFO(result.summary());
	}
}