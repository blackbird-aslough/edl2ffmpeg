#include "VideoComparator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/crc.h>
}

namespace test {

VideoComparator::VideoComparator() {
	// Initialize FFmpeg (once per process) - only needed for old FFmpeg
	static bool initialized = false;
	if (!initialized) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		av_register_all();
#endif
		initialized = true;
	}
}

VideoComparator::~VideoComparator() = default;

ComparisonResult VideoComparator::compare(const std::string& video1Path, 
										  const std::string& video2Path,
										  bool calculateChecksums) {
	ComparisonResult result;
	
	VideoReader reader1, reader2;
	
	if (!reader1.open(video1Path)) {
		result.errorMsg = "Failed to open video 1: " + video1Path;
		return result;
	}
	
	if (!reader2.open(video2Path)) {
		result.errorMsg = "Failed to open video 2: " + video2Path;
		return result;
	}
	
	int frameNum = 0;
	double totalPSNR = 0.0;
	
	while (true) {
		if (maxFramesToCompare_ > 0 && frameNum >= maxFramesToCompare_) {
			break;
		}
		
		AVFrame* frame1 = reader1.readNextFrame();
		AVFrame* frame2 = reader2.readNextFrame();
		
		// Check if both videos ended
		if (!frame1 && !frame2) {
			break;
		}
		
		// Check for length mismatch
		if (!frame1 || !frame2) {
			result.errorMsg = "Video length mismatch at frame " + std::to_string(frameNum);
			result.mismatchedFrames++;
			if (frame1) av_frame_free(&frame1);
			if (frame2) av_frame_free(&frame2);
			break;
		}
		
		// Calculate PSNR
		double psnr = calculatePSNR(frame1, frame2);
		totalPSNR += psnr;
		
		if (psnr < result.minPSNR) result.minPSNR = psnr;
		if (psnr > result.maxPSNR) result.maxPSNR = psnr;
		
		if (psnr < psnrThreshold_) {
			result.mismatchedFrames++;
			if (result.mismatchedFrames == 1) {
				result.maxFrameDiff = frameNum;
			}
		}
		
		// Calculate checksums if requested
		if (calculateChecksums) {
			FrameChecksum cs1 = {
				frameNum,
				calculateFrameChecksum(frame1),
				static_cast<double>(frame1->pts)
			};
			FrameChecksum cs2 = {
				frameNum,
				calculateFrameChecksum(frame2),
				static_cast<double>(frame2->pts)
			};
			result.ourChecksums.push_back(cs1);
			result.refChecksums.push_back(cs2);
		}
		
		av_frame_free(&frame1);
		av_frame_free(&frame2);
		frameNum++;
	}
	
	result.totalFrames = frameNum;
	if (frameNum > 0) {
		result.avgPSNR = totalPSNR / frameNum;
		result.completed = true;
		result.identical = (result.mismatchedFrames == 0);
	}
	
	return result;
}

std::vector<FrameChecksum> VideoComparator::extractChecksums(const std::string& videoPath) {
	std::vector<FrameChecksum> checksums;
	
	VideoReader reader;
	if (!reader.open(videoPath)) {
		return checksums;
	}
	
	int frameNum = 0;
	while (AVFrame* frame = reader.readNextFrame()) {
		FrameChecksum cs = {
			frameNum++,
			calculateFrameChecksum(frame),
			static_cast<double>(frame->pts)
		};
		checksums.push_back(cs);
		av_frame_free(&frame);
		
		if (maxFramesToCompare_ > 0 && frameNum >= maxFramesToCompare_) {
			break;
		}
	}
	
	return checksums;
}

double VideoComparator::calculatePSNR(AVFrame* frame1, AVFrame* frame2) {
	if (!frame1 || !frame2) return 0.0;
	
	// Only compare Y plane for speed
	uint64_t mse = 0;
	int width = std::min(frame1->width, frame2->width);
	int height = std::min(frame1->height, frame2->height);
	
	for (int y = 0; y < height; y++) {
		uint8_t* ptr1 = frame1->data[0] + y * frame1->linesize[0];
		uint8_t* ptr2 = frame2->data[0] + y * frame2->linesize[0];
		
		for (int x = 0; x < width; x++) {
			int diff = ptr1[x] - ptr2[x];
			mse += diff * diff;
		}
	}
	
	if (mse == 0) return 100.0;  // Identical frames
	
	double mseD = static_cast<double>(mse) / (width * height);
	double psnr = 20.0 * log10(255.0 / sqrt(mseD));
	
	return psnr;
}

double VideoComparator::calculateSSIM(AVFrame* frame1, AVFrame* frame2) {
	// TODO: Implement SSIM calculation if needed
	// This is more expensive than PSNR but provides better perceptual similarity
	return 0.0;
}

uint64_t VideoComparator::calculateFrameChecksum(AVFrame* frame) {
	if (!frame) return 0;
	
	// Use FFmpeg's CRC calculation for Y plane
	const AVCRC* crcTable = av_crc_get_table(AV_CRC_32_IEEE);
	uint32_t crc = 0;
	
	for (int y = 0; y < frame->height; y++) {
		crc = av_crc(crcTable, crc, 
					frame->data[0] + y * frame->linesize[0], 
					frame->width);
	}
	
	// Also include U and V planes for YUV formats
	if (frame->format == AV_PIX_FMT_YUV420P || 
		frame->format == AV_PIX_FMT_YUV422P ||
		frame->format == AV_PIX_FMT_YUV444P) {
		
		int chromaHeight = frame->height >> (frame->format == AV_PIX_FMT_YUV420P ? 1 : 0);
		int chromaWidth = frame->width >> (frame->format != AV_PIX_FMT_YUV444P ? 1 : 0);
		
		for (int p = 1; p <= 2; p++) {
			for (int y = 0; y < chromaHeight; y++) {
				crc = av_crc(crcTable, crc,
							frame->data[p] + y * frame->linesize[p],
							chromaWidth);
			}
		}
	}
	
	return crc;
}

bool VideoComparator::VideoReader::open(const std::string& path) {
	// Open input file
	if (avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr) < 0) {
		return false;
	}
	
	// Find stream info
	if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
		close();
		return false;
	}
	
	// Find video stream
	for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
		if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStreamIndex = i;
			break;
		}
	}
	
	if (videoStreamIndex == -1) {
		close();
		return false;
	}
	
	// Find decoder (handle const difference in FFmpeg versions)
	const AVCodec* codec = avcodec_find_decoder(formatCtx->streams[videoStreamIndex]->codecpar->codec_id);
	if (!codec) {
		close();
		return false;
	}
	
	// Allocate codec context
	codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx) {
		close();
		return false;
	}
	
	// Copy codec parameters
	if (avcodec_parameters_to_context(codecCtx, formatCtx->streams[videoStreamIndex]->codecpar) < 0) {
		close();
		return false;
	}
	
	// Open codec
	if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
		close();
		return false;
	}
	
	return true;
}

AVFrame* VideoComparator::VideoReader::readNextFrame() {
	if (!formatCtx || !codecCtx) return nullptr;
	
	AVPacket packet;
	av_init_packet(&packet);
	
	while (av_read_frame(formatCtx, &packet) >= 0) {
		if (packet.stream_index == videoStreamIndex) {
			// Send packet to decoder
			int ret = avcodec_send_packet(codecCtx, &packet);
			av_packet_unref(&packet);
			
			if (ret < 0) {
				continue;
			}
			
			// Get decoded frame
			AVFrame* frame = av_frame_alloc();
			ret = avcodec_receive_frame(codecCtx, frame);
			
			if (ret == 0) {
				return frame;
			} else {
				av_frame_free(&frame);
			}
		} else {
			av_packet_unref(&packet);
		}
	}
	
	// Flush decoder
	avcodec_send_packet(codecCtx, nullptr);
	AVFrame* frame = av_frame_alloc();
	if (avcodec_receive_frame(codecCtx, frame) == 0) {
		return frame;
	}
	av_frame_free(&frame);
	
	return nullptr;
}

void VideoComparator::VideoReader::close() {
	if (swsCtx) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	if (codecCtx) {
		avcodec_free_context(&codecCtx);
	}
	if (formatCtx) {
		avformat_close_input(&formatCtx);
	}
	videoStreamIndex = -1;
}

bool ComparisonResult::matchesChecksums(const std::string& checksumFile) const {
	auto loaded = loadChecksums(checksumFile);
	if (loaded.size() != ourChecksums.size()) {
		return false;
	}
	
	for (size_t i = 0; i < loaded.size(); i++) {
		if (loaded[i].checksum != ourChecksums[i].checksum) {
			return false;
		}
	}
	
	return true;
}

void ComparisonResult::saveChecksums(const std::string& checksumFile) const {
	test::saveChecksums(ourChecksums, checksumFile);
}

std::string ComparisonResult::summary() const {
	std::ostringstream ss;
	ss << "Comparison Result:\n";
	ss << "  Completed: " << (completed ? "Yes" : "No") << "\n";
	ss << "  Total Frames: " << totalFrames << "\n";
	ss << "  Avg PSNR: " << std::fixed << std::setprecision(2) << avgPSNR << " dB\n";
	ss << "  Min PSNR: " << minPSNR << " dB\n";
	ss << "  Max PSNR: " << maxPSNR << " dB\n";
	ss << "  Mismatched Frames: " << mismatchedFrames << "\n";
	ss << "  Visually Identical: " << (isVisuallyIdentical() ? "Yes" : "No") << "\n";
	
	if (!errorMsg.empty()) {
		ss << "  Error: " << errorMsg << "\n";
	}
	
	return ss.str();
}

std::vector<FrameChecksum> loadChecksums(const std::string& path) {
	std::vector<FrameChecksum> checksums;
	std::ifstream file(path);
	
	if (!file.is_open()) {
		return checksums;
	}
	
	std::string line;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		FrameChecksum cs;
		if (iss >> cs.frameNumber >> cs.checksum >> cs.pts) {
			checksums.push_back(cs);
		}
	}
	
	return checksums;
}

void saveChecksums(const std::vector<FrameChecksum>& checksums, const std::string& path) {
	std::ofstream file(path);
	
	for (const auto& cs : checksums) {
		file << cs.frameNumber << " " 
			 << cs.checksum << " "
			 << std::fixed << std::setprecision(6) << cs.pts << "\n";
	}
}

} // namespace test