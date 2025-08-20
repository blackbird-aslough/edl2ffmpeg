#include "TestRunner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <array>
#include <memory>
#include <random>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace test {

TestRunner::TestRunner() {
	// Set default paths
	workDir_ = fs::temp_directory_path() / "edl2ffmpeg_tests";
	fs::create_directories(workDir_);
	
	fixtureDir_ = "fixtures";
	
	// Find executables
	findExecutables();
}

TestRunner::~TestRunner() {
	if (!keepTempFiles_) {
		cleanup();
	}
}

ComparisonResult TestRunner::compareRenders(const std::string& edlPath,
											const std::string& inputVideo) {
	ComparisonResult result;
	
	// Generate output paths
	std::string baseName = fs::path(edlPath).stem();
	lastOurOutput_ = getTempPath(baseName + "_our.mp4");
	lastRefOutput_ = getTempPath(baseName + "_ref.mp4");
	
	// If input video specified, update EDL to use it
	std::string actualEdlPath = edlPath;
	if (!inputVideo.empty()) {
		std::string tempEdl = getTempPath(baseName + "_temp.json");
		if (!updateEDLVideoPath(edlPath, inputVideo, tempEdl)) {
			result.errorMsg = "Failed to update EDL with input video path";
			return result;
		}
		actualEdlPath = tempEdl;
	}
	
	// Run our renderer
	auto ourStart = std::chrono::high_resolution_clock::now();
	bool ourSuccess = runEdl2ffmpeg(actualEdlPath, lastOurOutput_, inputVideo);
	auto ourEnd = std::chrono::high_resolution_clock::now();
	result.ourRenderTime = std::chrono::duration<double, std::milli>(ourEnd - ourStart).count();
	
	if (!ourSuccess) {
		result.errorMsg = "edl2ffmpeg failed to render";
		return result;
	}
	
	// Run reference renderer
	auto refStart = std::chrono::high_resolution_clock::now();
	bool refSuccess = runReference(actualEdlPath, lastRefOutput_, inputVideo);
	auto refEnd = std::chrono::high_resolution_clock::now();
	result.refRenderTime = std::chrono::duration<double, std::milli>(refEnd - refStart).count();
	
	if (!refSuccess) {
		result.errorMsg = "Reference renderer failed (Docker may not be running or image not loaded)";
		return result;
	}
	
	// Compare outputs
	VideoComparator comparator;
	result = comparator.compare(lastOurOutput_, lastRefOutput_);
	
	// Add timing info
	result.ourRenderTime = std::chrono::duration<double, std::milli>(ourEnd - ourStart).count();
	result.refRenderTime = std::chrono::duration<double, std::milli>(refEnd - refStart).count();
	
	return result;
}

ComparisonResult TestRunner::compareRenders(const nlohmann::json& edlJson,
											const std::string& inputVideo) {
	// Save JSON to temp file
	std::string tempEdl = getTempPath("generated.json");
	std::ofstream file(tempEdl);
	file << edlJson.dump(2);
	file.close();
	
	return compareRenders(tempEdl, inputVideo);
}

bool TestRunner::runEdl2ffmpeg(const std::string& edlPath,
							   const std::string& outputPath,
							   const std::string& inputVideo) {
	std::string cmd = edl2ffmpegPath_ + " " + edlPath + " " + outputPath;
	
	if (verbose_) {
		cmd += " -v";
	}
	
	if (verbose_) {
		std::cout << "Running: " << cmd << std::endl;
	}
	
	auto result = executeCommand(cmd);
	
	if (result.exitCode != 0) {
		if (verbose_) {
			std::cerr << "edl2ffmpeg failed with exit code: " << result.exitCode << std::endl;
			std::cerr << "Output: " << result.stdout << std::endl;
		}
	}
	
	return result.exitCode == 0 && fs::exists(outputPath);
}

bool TestRunner::runReference(const std::string& edlPath,
							  const std::string& outputPath,
							  const std::string& inputVideo) {
	std::string cmd = referenceScript_ + " " + edlPath + " " + outputPath;
	
	if (verbose_) {
		std::cout << "Running reference: " << cmd << std::endl;
	}
	
	auto result = executeCommand(cmd);
	
	if (result.exitCode != 0) {
		if (verbose_) {
			std::cerr << "Reference failed with exit code: " << result.exitCode << std::endl;
			std::cerr << "Output: " << result.stdout << std::endl;
		}
	}
	
	bool exists = fs::exists(outputPath);
	if (!exists && verbose_) {
		std::cerr << "Reference output file not created: " << outputPath << std::endl;
	}
	
	return result.exitCode == 0 && exists;
}

std::string TestRunner::getTempPath(const std::string& suffix) {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<> dis(100000, 999999);
	
	return (fs::path(workDir_) / (std::to_string(dis(gen)) + "_" + suffix)).string();
}

bool TestRunner::updateEDLVideoPath(const std::string& edlPath,
									const std::string& videoPath,
									const std::string& outputPath) {
	try {
		std::ifstream input(edlPath);
		nlohmann::json edl;
		input >> edl;
		input.close();
		
		// Update video paths in clips
		if (edl.contains("clips")) {
			for (auto& clip : edl["clips"]) {
				if (clip.contains("source") && clip["source"].contains("uri")) {
					clip["source"]["uri"] = videoPath;
				}
				if (clip.contains("sources")) {
					for (auto& source : clip["sources"]) {
						if (source.contains("uri")) {
							source["uri"] = videoPath;
						}
					}
				}
			}
		}
		
		// Update video paths in sources section if present
		if (edl.contains("sources")) {
			for (auto& [key, source] : edl["sources"].items()) {
				if (source.contains("uri")) {
					source["uri"] = videoPath;
				}
			}
		}
		
		std::ofstream output(outputPath);
		output << edl.dump(2);
		output.close();
		
		return true;
	} catch (const std::exception& e) {
		if (verbose_) {
			std::cerr << "Error updating EDL: " << e.what() << std::endl;
		}
		return false;
	}
}

TestRunner::CommandResult TestRunner::executeCommand(const std::string& command) {
	CommandResult result;
	
	auto start = std::chrono::high_resolution_clock::now();
	
	// Redirect stderr to stdout for simplicity
	std::string fullCommand = command + " 2>&1";
	
	std::array<char, 4096> buffer;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(fullCommand.c_str(), "r"), pclose);
	
	if (!pipe) {
		result.exitCode = -1;
		result.stderr = "Failed to execute command";
		return result;
	}
	
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result.stdout += buffer.data();
	}
	
	int status = pclose(pipe.release());
	result.exitCode = WEXITSTATUS(status);
	
	auto end = std::chrono::high_resolution_clock::now();
	result.executionTime = std::chrono::duration<double, std::milli>(end - start).count();
	
	return result;
}

void TestRunner::cleanup() {
	if (!keepTempFiles_ && fs::exists(workDir_)) {
		try {
			// Only remove our temp files, not the whole directory
			for (const auto& entry : fs::directory_iterator(workDir_)) {
				if (entry.path().string().find("_our.mp4") != std::string::npos ||
					entry.path().string().find("_ref.mp4") != std::string::npos ||
					entry.path().string().find("_temp.json") != std::string::npos ||
					entry.path().string().find("generated.json") != std::string::npos) {
					fs::remove(entry.path());
				}
			}
		} catch (const std::exception& e) {
			if (verbose_) {
				std::cerr << "Cleanup error: " << e.what() << std::endl;
			}
		}
	}
}

void TestRunner::findExecutables() {
	// Try to find edl2ffmpeg executable
	if (fs::exists("./edl2ffmpeg")) {
		edl2ffmpegPath_ = "./edl2ffmpeg";
	} else if (fs::exists("../edl2ffmpeg")) {
		edl2ffmpegPath_ = "../edl2ffmpeg";
	} else if (fs::exists("../build/edl2ffmpeg")) {
		edl2ffmpegPath_ = "../build/edl2ffmpeg";
	} else if (fs::exists("../../build/edl2ffmpeg")) {
		edl2ffmpegPath_ = "../../build/edl2ffmpeg";
	} else {
		edl2ffmpegPath_ = "edl2ffmpeg";  // Hope it's in PATH
	}
	
	// Find reference renderer script
	if (fs::exists("../../scripts/ftv_toffmpeg_wrapper_full.sh")) {
		referenceScript_ = "../../scripts/ftv_toffmpeg_wrapper_full.sh";
	} else if (fs::exists("../scripts/ftv_toffmpeg_wrapper_full.sh")) {
		referenceScript_ = "../scripts/ftv_toffmpeg_wrapper_full.sh";
	} else if (fs::exists("scripts/ftv_toffmpeg_wrapper_full.sh")) {
		referenceScript_ = "scripts/ftv_toffmpeg_wrapper_full.sh";
	} else {
		referenceScript_ = "ftv_toffmpeg_wrapper_full.sh";  // Hope it's in PATH
	}
	
	if (verbose_) {
		std::cout << "edl2ffmpeg path: " << edl2ffmpegPath_ << std::endl;
		std::cout << "Reference script: " << referenceScript_ << std::endl;
	}
}

} // namespace test