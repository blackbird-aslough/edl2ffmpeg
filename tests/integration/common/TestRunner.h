#pragma once

#include "VideoComparator.h"
#include <string>
#include <chrono>
#include <functional>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace test {

class TestRunner {
public:
	TestRunner();
	~TestRunner();
	
	// Run both renderers and compare output
	ComparisonResult compareRenders(const std::string& edlPath,
									const std::string& inputVideo = "");
	
	// Run both renderers with inline EDL JSON
	ComparisonResult compareRenders(const nlohmann::json& edlJson,
									const std::string& inputVideo = "");
	
	// Run edl2ffmpeg only
	bool runEdl2ffmpeg(const std::string& edlPath,
					  const std::string& outputPath,
					  const std::string& inputVideo = "");
	
	// Run reference renderer only
	bool runReference(const std::string& edlPath,
					 const std::string& outputPath,
					 const std::string& inputVideo = "");
	
	// Time execution of a function
	template<typename Func>
	double timeExecution(Func func) {
		auto start = std::chrono::high_resolution_clock::now();
		func();
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double, std::milli>(end - start).count();
	}
	
	// Configuration
	void setWorkDir(const std::string& dir) { workDir_ = dir; }
	void setFixtureDir(const std::string& dir) { fixtureDir_ = dir; }
	void setVerbose(bool verbose) { verbose_ = verbose; }
	void setKeepTempFiles(bool keep) { keepTempFiles_ = keep; }
	void setEdl2ffmpegPath(const std::string& path) { edl2ffmpegPath_ = path; }
	void setReferenceScript(const std::string& path) { referenceScript_ = path; }
	
	// Get paths
	std::string getLastOurOutput() const { return lastOurOutput_; }
	std::string getLastRefOutput() const { return lastRefOutput_; }
	
private:
	std::string workDir_;
	std::string fixtureDir_;
	std::string edl2ffmpegPath_;
	std::string referenceScript_;
	bool verbose_ = false;
	bool keepTempFiles_ = false;
	
	// Last output paths for debugging
	std::string lastOurOutput_;
	std::string lastRefOutput_;
	
	// Helper to generate temp file path
	std::string getTempPath(const std::string& suffix);
	
	// Helper to update EDL with correct video path
	bool updateEDLVideoPath(const std::string& edlPath,
						   const std::string& videoPath,
						   const std::string& outputPath);
	
	// Execute command and capture output
	struct CommandResult {
		int exitCode;
		std::string stdout;
		std::string stderr;
		double executionTime;
	};
	
	CommandResult executeCommand(const std::string& command);
	
	// Clean up temp files
	void cleanup();
	
	// Find executable paths
	void findExecutables();
};

} // namespace test