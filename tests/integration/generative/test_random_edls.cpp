#include <catch2/catch_all.hpp>
#include "../common/TestRunner.h"
#include "../common/EDLGenerator.h"
#include <random>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE("Random single clips produce similar output", "[generative][single]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	// Use Catch2's random seed for reproducibility
	auto seed = GENERATE(take(10, random(0u, 1000000u)));
	CAPTURE(seed);  // Log seed for reproduction
	
	test::EDLGenerator gen(seed);
	auto edl = gen
		.withClips(1, 1)
		.withDuration(3.0)  // Keep short for speed
		.withImplementedEffects()
		.withFades(true)
		.withMaxComplexity(2)
		.generateSingleClip();
	
	auto result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 30.0);  // Reasonable similarity
	
	if (result.avgPSNR < 30.0) {
		INFO("Failed with seed: " << seed);
		INFO(result.summary());
	}
}

TEST_CASE("Random multiple clips produce similar output", "[generative][multiple]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	auto seed = GENERATE(take(10, random(0u, 1000000u)));
	CAPTURE(seed);
	
	test::EDLGenerator gen(seed);
	auto edl = gen
		.withClips(2, 5)
		.withDuration(5.0)
		.withImplementedEffects()
		.withFades(true)
		.withMaxComplexity(3)
		.generate();
	
	auto result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 28.0);  // Slightly lower threshold for complex EDLs
	CHECK(result.maxFrameDiff < 10);
	
	if (result.avgPSNR < 28.0) {
		INFO("Failed with seed: " << seed);
		INFO(result.summary());
		
		// Save failing EDL for debugging
		std::string failPath = "generative/failures/edl_" + std::to_string(seed) + ".json";
		fs::create_directories(fs::path(failPath).parent_path());
		std::ofstream file(failPath);
		file << edl.dump(2);
		file.close();
		INFO("Saved failing EDL to: " << failPath);
	}
}

TEST_CASE("Random effects produce reasonable output", "[generative][effects]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	auto seed = GENERATE(take(10, random(0u, 1000000u)));
	CAPTURE(seed);
	
	test::EDLGenerator gen(seed);
	
	// Generate random brightness value
	std::uniform_real_distribution<float> brightnessDist(0.3f, 2.0f);
	std::mt19937 rng(seed);
	float brightness = brightnessDist(rng);
	
	auto edl = gen.generateWithEffect("brightness", brightness);
	
	auto result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 25.0);  // Effects can cause larger differences
	
	// Verify contrast effect similarly
	std::uniform_real_distribution<float> contrastDist(0.3f, 2.0f);
	float contrast = contrastDist(rng);
	
	edl = gen.generateWithEffect("contrast", contrast);
	result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 25.0);
}

TEST_CASE("Random fades produce reasonable output", "[generative][fades]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	
	auto seed = GENERATE(take(10, random(0u, 1000000u)));
	CAPTURE(seed);
	
	test::EDLGenerator gen(seed);
	
	// Generate random fade values
	std::uniform_real_distribution<float> fadeDist(0.0f, 2.0f);
	std::mt19937 rng(seed);
	float topFade = fadeDist(rng);
	float tailFade = fadeDist(rng);
	
	auto edl = gen.generateWithFades(topFade, tailFade);
	
	auto result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	CHECK(result.avgPSNR > 30.0);
	CHECK(result.isVisuallyIdentical());
}

// Helper function for random values in tests
double randomBetween(double min, double max) {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<double> dist(min, max);
	return dist(gen);
}

int randomBetween(int min, int max) {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(min, max);
	return dist(gen);
}

TEST_CASE("Property: Any valid EDL should render without crashing", "[generative][property]") {
	test::TestRunner runner;
	runner.setVerbose(false);
	runner.setKeepTempFiles(false);
	
	// Test 50 random EDLs
	auto seed = GENERATE(take(50, random(0u, 10000000u)));
	CAPTURE(seed);
	
	test::EDLGenerator gen(seed);
	auto edl = gen
		.withClips(1, 10)
		.withDuration(randomBetween(1.0, 10.0))
		.withImplementedEffects()
		.withFades(true)
		.withMaxComplexity(randomBetween(1, 5))
		.generate();
	
	// Property 1: Both renderers should complete
	auto result = runner.compareRenders(edl);
	REQUIRE(result.completed);
	
	// Property 2: Frame counts should match
	CHECK(result.totalFrames > 0);
	CHECK(result.ourChecksums.size() == result.refChecksums.size());
	
	// Property 3: PSNR should be reasonable (not completely different)
	CHECK(result.avgPSNR > 20.0);  // Very lenient threshold
	
	// Property 4: No NaN or infinite values
	CHECK(std::isfinite(result.avgPSNR));
	CHECK(std::isfinite(result.minPSNR));
	CHECK(std::isfinite(result.maxPSNR));
}

TEST_CASE("Performance comparison", "[generative][performance][!benchmark]") {
	test::TestRunner runner;
	runner.setVerbose(true);
	
	// Generate a moderately complex EDL
	test::EDLGenerator gen(42);  // Fixed seed for consistency
	auto edl = gen
		.withClips(5, 5)
		.withDuration(10.0)
		.withImplementedEffects()
		.withFades(true)
		.withMaxComplexity(3)
		.generate();
	
	auto result = runner.compareRenders(edl);
	
	REQUIRE(result.completed);
	
	// Log performance metrics
	double speedup = result.refRenderTime / result.ourRenderTime;
	
	std::cout << "\nPerformance Comparison:\n";
	std::cout << "  edl2ffmpeg: " << result.ourRenderTime << " ms\n";
	std::cout << "  reference:  " << result.refRenderTime << " ms\n";
	std::cout << "  Speedup:    " << speedup << "x\n";
	
	// We should be at least as fast as the reference
	CHECK(speedup >= 0.8);  // Allow 20% slower for safety
	
	// Ideally we should be faster
	if (speedup > 1.5) {
		std::cout << "  ✓ Significant performance improvement!\n";
	}
}