#include "edl/EDLParser.h"
#include "compositor/InstructionGenerator.h"
#include "compositor/FrameCompositor.h"
#include "media/FFmpegDecoder.h"
#include "media/FFmpegEncoder.h"
#include "media/HardwareAcceleration.h"
#include "utils/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <iostream>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

void printUsage(const char* programName) {
	std::cout << "Usage: " << programName << " <edl_file> <output_file> [options]\n";
	std::cout << "\nOptions:\n";
	std::cout << "  -c, --codec <codec>      Video codec (default: libx264)\n";
	std::cout << "  -b, --bitrate <bitrate>  Video bitrate (default: 446464 / 436Ki)\n";
	std::cout << "  -p, --preset <preset>    Encoder preset (default: faster)\n";
	std::cout << "  --crf <value>            Use Constant Rate Factor mode (disables bitrate)\n";
	std::cout << "  --hw-accel <type>        Hardware acceleration (auto, none, nvenc, vaapi, videotoolbox)\n";
	std::cout << "  --hw-device <device>     Hardware device index (default: 0)\n";
	std::cout << "  --hw-decode              Enable hardware decoding (default: auto)\n";
	std::cout << "  --hw-encode              Enable hardware encoding (default: auto)\n";
	std::cout << "  -v, --verbose            Enable verbose logging\n";
	std::cout << "  -q, --quiet              Suppress all non-error output\n";
	std::cout << "  -h, --help               Show this help message\n";
	std::cout << "\nExamples:\n";
	std::cout << "  " << programName << " input.json output.mp4\n";
	std::cout << "  " << programName << " input.json output.mp4 --codec libx265 --crf 28\n";
	std::cout << "  " << programName << " input.json output.mp4 -b 8000000 -p fast\n";
	std::cout << "  " << programName << " input.json output.mp4 --hw-accel nvenc --hw-encode\n";
	std::cout << "  " << programName << " input.json output.mp4 --hw-accel auto --hw-encode --hw-decode\n";
}

struct Options {
	std::string edlFile;
	std::string outputFile;
	std::string codec = "libx264";
	int bitrate = 446464;  // 436Ki (436 * 1024) - matching ftv_toffmpeg default
	std::string preset = "faster";  // matching ftv_toffmpeg default
	int crf = 23;
	bool verbose = false;
	bool quiet = false;
	
	// Hardware acceleration options
	std::string hwAccelType = "auto";
	int hwDevice = 0;
	bool hwDecode = false;
	bool hwEncode = false;
};

Options parseCommandLine(int argc, char* argv[]) {
	Options opts;
	
	if (argc < 3) {
		printUsage(argv[0]);
		std::exit(1);
	}
	
	opts.edlFile = argv[1];
	opts.outputFile = argv[2];
	
	for (int i = 3; i < argc; ++i) {
		std::string arg = argv[i];
		
		if (arg == "-h" || arg == "--help") {
			printUsage(argv[0]);
			std::exit(0);
		} else if (arg == "-v" || arg == "--verbose") {
			opts.verbose = true;
		} else if (arg == "-q" || arg == "--quiet") {
			opts.quiet = true;
		} else if ((arg == "-c" || arg == "--codec") && i + 1 < argc) {
			opts.codec = argv[++i];
		} else if ((arg == "-b" || arg == "--bitrate") && i + 1 < argc) {
			opts.bitrate = std::stoi(argv[++i]);
		} else if ((arg == "-p" || arg == "--preset") && i + 1 < argc) {
			opts.preset = argv[++i];
		} else if (arg == "--crf" && i + 1 < argc) {
			opts.crf = std::stoi(argv[++i]);
			opts.bitrate = 0; // Enable CRF mode by setting bitrate to 0
		} else if (arg == "--hw-accel" && i + 1 < argc) {
			opts.hwAccelType = argv[++i];
		} else if (arg == "--hw-device" && i + 1 < argc) {
			opts.hwDevice = std::stoi(argv[++i]);
		} else if (arg == "--hw-decode") {
			opts.hwDecode = true;
		} else if (arg == "--hw-encode") {
			opts.hwEncode = true;
		} else {
			std::cerr << "Unknown option: " << arg << "\n";
			printUsage(argv[0]);
			std::exit(1);
		}
	}
	
	return opts;
}

std::string getMediaPath(const std::string& uri, const std::string& edlPath) {
	// First, check if uri is already a full path
	if (fs::exists(uri)) {
		return uri;
	}
	
	// Try relative to EDL file directory
	fs::path edlDir = fs::path(edlPath).parent_path();
	fs::path mediaPath = edlDir / uri;
	if (fs::exists(mediaPath)) {
		return mediaPath.string();
	}
	
	// Try in current directory
	if (fs::exists(fs::path(uri))) {
		return uri;
	}
	
	// Return as-is and let FFmpeg handle it
	return uri;
}

// Check if instruction requires CPU processing (effects, transforms, etc.)
bool requiresCPUProcessing(const compositor::CompositorInstruction& instruction) {
	// Check for effects
	if (!instruction.effects.empty()) {
		return true;
	}
	
	// Check for fade
	if (instruction.fade < 1.0f) {
		return true;
	}
	
	// Check for transforms
	if (std::abs(instruction.panX) > 0.001f ||
		std::abs(instruction.panY) > 0.001f ||
		std::abs(instruction.zoomX - 1.0f) > 0.001f ||
		std::abs(instruction.zoomY - 1.0f) > 0.001f ||
		std::abs(instruction.rotation) > 0.001f ||
		instruction.flip) {
		return true;
	}
	
	// Check for transitions
	if (instruction.transition.type != compositor::TransitionInfo::None) {
		return true;
	}
	
	// Check if it's not a simple draw frame
	if (instruction.type != compositor::CompositorInstruction::DrawFrame) {
		return true;
	}
	
	return false;
}

void printProgress(int current, int total, double fps, double /*elapsed*/) {
	double progress = (double)current / total * 100.0;
	int barWidth = 50;
	int pos = static_cast<int>(barWidth * current / total);
	
	// Ensure pos doesn't exceed barWidth - 1
	if (pos >= barWidth) pos = barWidth - 1;
	
	std::cout << "\r[";
	for (int i = 0; i < barWidth; ++i) {
		if (i < pos) std::cout << "=";
		else if (i == pos && current < total) std::cout << ">";
		else std::cout << " ";
	}
	
	double eta = (total - current) / fps;
	
	std::cout << "] " << std::fixed << std::setprecision(1) << progress << "% ";
	std::cout << "(" << current << "/" << total << " frames) ";
	std::cout << "FPS: " << std::setprecision(1) << fps << " ";
	std::cout << "ETA: " << std::setprecision(0) << eta << "s ";
	std::cout << std::flush;
}

int main(int argc, char* argv[]) {
	try {
		// Initialize FFmpeg (required for older versions)
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		av_register_all();
#endif
		
		// Parse command line
		Options opts = parseCommandLine(argc, argv);
		
		// Set logging level
		if (opts.quiet) {
			utils::Logger::setLevel(utils::Logger::ERROR);
		} else if (opts.verbose) {
			utils::Logger::setLevel(utils::Logger::DEBUG);
		} else {
			utils::Logger::setLevel(utils::Logger::INFO);
		}
		
		// Parse EDL file
		utils::Logger::info("Parsing EDL file: {}", opts.edlFile);
		edl::EDL edl = edl::EDLParser::parse(opts.edlFile);
		
		utils::Logger::info("EDL: {}x{} @ {} fps, {} clips",
			edl.width, edl.height, edl.fps, edl.clips.size());
		
		// Initialize decoders for all unique media files
		std::unordered_map<std::string, std::unique_ptr<media::FFmpegDecoder>> decoders;
		
		for (const auto& clip : edl.clips) {
			if (clip.track.type != edl::Track::Video) {
				continue;
			}
			
			const std::string& uri = clip.source.uri;
			if (decoders.find(uri) == decoders.end()) {
				std::string mediaPath = getMediaPath(uri, opts.edlFile);
				utils::Logger::info("Loading media: {} -> {}", uri, mediaPath);
				
				try {
					// Configure decoder with hardware acceleration if requested
					media::FFmpegDecoder::Config decoderConfig;
					decoderConfig.useHardwareDecoder = opts.hwDecode;
					decoderConfig.hwConfig.type = media::HardwareAcceleration::stringToHWAccelType(opts.hwAccelType);
					decoderConfig.hwConfig.deviceIndex = opts.hwDevice;
					decoderConfig.hwConfig.allowFallback = true;
					// Enable GPU passthrough if both decode and encode use hardware
					decoderConfig.keepHardwareFrames = opts.hwDecode && opts.hwEncode;
					
					decoders[uri] = std::make_unique<media::FFmpegDecoder>(mediaPath, decoderConfig);
				} catch (const std::exception& e) {
					utils::Logger::error("Failed to load media {}: {}", mediaPath, e.what());
					throw;
				}
			}
		}
		
		// Setup encoder
		media::FFmpegEncoder::Config encoderConfig;
		encoderConfig.width = edl.width;
		encoderConfig.height = edl.height;
		encoderConfig.frameRate = {edl.fps, 1};
		encoderConfig.codec = opts.codec;
		encoderConfig.bitrate = opts.bitrate;
		encoderConfig.preset = opts.preset;
		encoderConfig.crf = opts.crf;
		encoderConfig.useHardwareEncoder = opts.hwEncode;
		encoderConfig.hwConfig.type = media::HardwareAcceleration::stringToHWAccelType(opts.hwAccelType);
		encoderConfig.hwConfig.deviceIndex = opts.hwDevice;
		encoderConfig.hwConfig.allowFallback = true;
		
		utils::Logger::info("Creating output file: {}", opts.outputFile);
		media::FFmpegEncoder encoder(opts.outputFile, encoderConfig);
		
		// Setup compositor
		compositor::FrameCompositor compositor(edl.width, edl.height, AV_PIX_FMT_YUV420P);
		
		// Setup instruction generator
		compositor::InstructionGenerator generator(edl);
		int totalFrames = generator.getTotalFrames();
		
		utils::Logger::info("Processing {} frames...", totalFrames);
		
		// Analyze if GPU passthrough is possible
		bool canUseGPUPassthrough = opts.hwDecode && opts.hwEncode;
		if (canUseGPUPassthrough) {
			// Check if any frame needs CPU processing
			bool needsCPU = false;
			for (const auto& instruction : generator) {
				if (requiresCPUProcessing(instruction)) {
					needsCPU = true;
					break;
				}
			}
			
			if (!needsCPU) {
				utils::Logger::info("GPU passthrough enabled - zero-copy pipeline active");
			} else {
				utils::Logger::info("GPU acceleration enabled but some frames require CPU processing");
			}
		}
		
		// Process frames
		auto startTime = std::chrono::high_resolution_clock::now();
		int frameCount = 0;
		int progressUpdateInterval = edl.fps / 2;  // Update twice per second
		
		for (const auto& instruction : generator) {
			std::shared_ptr<AVFrame> outputFrame;
			
			// Check if we can use GPU passthrough (no effects, transforms, or color generation)
			bool useGPUPassthrough = opts.hwDecode && opts.hwEncode && 
									 !requiresCPUProcessing(instruction) &&
									 instruction.type == compositor::CompositorInstruction::DrawFrame;
			
			if (useGPUPassthrough) {
				// GPU passthrough path - no CPU processing needed
				auto& decoder = decoders[instruction.uri];
				if (decoder) {
					// Get hardware frame directly from decoder
					auto hwFrame = decoder->getHardwareFrame(instruction.sourceFrameNumber);
					if (hwFrame) {
						// Write hardware frame directly to encoder
						encoder.writeHardwareFrame(hwFrame.get());
						frameCount++;
						
						// Update progress
						if (!opts.quiet && (frameCount % progressUpdateInterval == 0 || frameCount == totalFrames)) {
							auto currentTime = std::chrono::high_resolution_clock::now();
							std::chrono::duration<double> elapsed = currentTime - startTime;
							double fps = frameCount / elapsed.count();
							printProgress(frameCount, totalFrames, fps, elapsed.count());
						}
						continue;
					} else {
						// Fall back to CPU path if hardware frame failed
						utils::Logger::warn("Failed to get hardware frame, falling back to CPU processing");
						useGPUPassthrough = false;
					}
				}
			}
			
			// CPU processing path (original code)
			if (instruction.type == compositor::CompositorInstruction::DrawFrame) {
				// Get the decoder for this media
				auto& decoder = decoders[instruction.uri];
				if (decoder) {
					// Get the source frame
					auto inputFrame = decoder->getFrame(instruction.sourceFrameNumber);
					
					// Process through compositor
					outputFrame = compositor.processFrame(inputFrame, instruction);
				} else {
					utils::Logger::warn("Decoder not found for media: {}", instruction.uri);
					outputFrame = compositor.generateColorFrame(0, 0, 0);
				}
			} else if (instruction.type == compositor::CompositorInstruction::GenerateColor) {
				// Generate color frame
				outputFrame = compositor.generateColorFrame(
					instruction.color.r,
					instruction.color.g,
					instruction.color.b
				);
			} else {
				// NoOp or unknown - generate black frame
				outputFrame = compositor.generateColorFrame(0, 0, 0);
			}
			
			// Write frame to encoder
			if (outputFrame) {
				encoder.writeFrame(outputFrame.get());
			}
			
			frameCount++;
			
			// Update progress
			if (!opts.quiet && (frameCount % progressUpdateInterval == 0 || frameCount == totalFrames)) {
				auto currentTime = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> elapsed = currentTime - startTime;
				double fps = frameCount / elapsed.count();
				printProgress(frameCount, totalFrames, fps, elapsed.count());
			}
		}
		
		if (!opts.quiet) {
			std::cout << "\n";
		}
		
		// Finalize encoder
		encoder.finalize();
		
		// Calculate and report statistics
		auto endTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> totalTime = endTime - startTime;
		double avgFps = frameCount / totalTime.count();
		
		utils::Logger::info("Rendering complete!");
		utils::Logger::info("Total frames: {}", frameCount);
		utils::Logger::info("Total time: {} seconds", totalTime.count());
		utils::Logger::info("Average FPS: {}", avgFps);
		utils::Logger::info("Output file: {}", opts.outputFile);
		
		return 0;
		
	} catch (const std::exception& e) {
		utils::Logger::error("Fatal error: {}", e.what());
		return 1;
	}
}