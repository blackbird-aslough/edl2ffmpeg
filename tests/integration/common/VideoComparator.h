#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

namespace test {

struct FrameChecksum {
	int frameNumber;
	uint64_t checksum;
	double pts;  // Presentation timestamp
};

struct ComparisonResult {
	bool completed = false;
	bool identical = false;
	
	// Frame-level metrics
	double avgPSNR = 0.0;
	double minPSNR = 100.0;
	double maxPSNR = 0.0;
	int maxFrameDiff = 0;
	int totalFrames = 0;
	int mismatchedFrames = 0;
	
	// Checksums for exact comparison
	std::vector<FrameChecksum> ourChecksums;
	std::vector<FrameChecksum> refChecksums;
	
	// Error information
	std::string errorMsg;
	
	// Performance metrics
	double ourRenderTime = 0.0;
	double refRenderTime = 0.0;
	
	// Helper methods
	bool isVisuallyIdentical() const { return avgPSNR > 35.0 && maxFrameDiff < 5; }
	bool matchesChecksums(const std::string& checksumFile) const;
	void saveChecksums(const std::string& checksumFile) const;
	std::string summary() const;
};

class VideoComparator {
public:
	VideoComparator();
	~VideoComparator();
	
	// Compare two video files frame by frame
	ComparisonResult compare(const std::string& video1Path, 
							 const std::string& video2Path,
							 bool calculateChecksums = true);
	
	// Extract frame checksums from a video
	std::vector<FrameChecksum> extractChecksums(const std::string& videoPath);
	
	// Calculate PSNR between two frames
	static double calculatePSNR(AVFrame* frame1, AVFrame* frame2);
	
	// Calculate SSIM between two frames (optional, more expensive)
	static double calculateSSIM(AVFrame* frame1, AVFrame* frame2);
	
	// Set comparison tolerance
	void setPSNRThreshold(double threshold) { psnrThreshold_ = threshold; }
	void setMaxFramesToCompare(int maxFrames) { maxFramesToCompare_ = maxFrames; }
	
private:
	struct VideoReader {
		AVFormatContext* formatCtx = nullptr;
		AVCodecContext* codecCtx = nullptr;
		int videoStreamIndex = -1;
		SwsContext* swsCtx = nullptr;
		
		bool open(const std::string& path);
		AVFrame* readNextFrame();
		void close();
		~VideoReader() { close(); }
	};
	
	// Calculate checksum for a frame
	static uint64_t calculateFrameChecksum(AVFrame* frame);
	
	// Convert frame to common format for comparison
	static AVFrame* convertToYUV420P(AVFrame* frame, SwsContext** swsCtx);
	
	double psnrThreshold_ = 35.0;  // Visually identical threshold
	int maxFramesToCompare_ = -1;   // -1 means compare all frames
};

// Utility function to load checksums from file
std::vector<FrameChecksum> loadChecksums(const std::string& path);

// Utility function to save checksums to file
void saveChecksums(const std::vector<FrameChecksum>& checksums, const std::string& path);

} // namespace test