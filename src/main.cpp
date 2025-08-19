#include "edl/EDLParser.h"
#include "compositor/InstructionGenerator.h"
#include "compositor/FrameCompositor.h"
#include "media/FFmpegDecoder.h"
#include "media/FFmpegEncoder.h"
#include "media/HardwareAcceleration.h"
#include "media/HardwareContextManager.h"
#include "utils/Logger.h"
#include "utils/Timer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <iostream>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <thread>
#include <iomanip>
#include <variant>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

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
			try {
				opts.bitrate = std::stoi(argv[++i]);
			} catch (const std::invalid_argument& e) {
				std::cerr << "Error: Invalid bitrate value: " << argv[i] << "\n";
				std::exit(1);
			} catch (const std::out_of_range& e) {
				std::cerr << "Error: Bitrate value out of range: " << argv[i] << "\n";
				std::exit(1);
			}
		} else if ((arg == "-p" || arg == "--preset") && i + 1 < argc) {
			opts.preset = argv[++i];
		} else if (arg == "--crf" && i + 1 < argc) {
			try {
				opts.crf = std::stoi(argv[++i]);
				opts.bitrate = 0; // Enable CRF mode by setting bitrate to 0
			} catch (const std::invalid_argument& e) {
				std::cerr << "Error: Invalid CRF value: " << argv[i] << "\n";
				std::exit(1);
			} catch (const std::out_of_range& e) {
				std::cerr << "Error: CRF value out of range: " << argv[i] << "\n";
				std::exit(1);
			}
		} else if (arg == "--hw-accel" && i + 1 < argc) {
			opts.hwAccelType = argv[++i];
		} else if (arg == "--hw-device" && i + 1 < argc) {
			try {
				opts.hwDevice = std::stoi(argv[++i]);
			} catch (const std::invalid_argument& e) {
				std::cerr << "Error: Invalid hardware device index: " << argv[i] << "\n";
				std::exit(1);
			} catch (const std::out_of_range& e) {
				std::cerr << "Error: Hardware device index out of range: " << argv[i] << "\n";
				std::exit(1);
			}
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

int getTerminalWidth() {
	int width = 80; // Default width
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
		width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
#else
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
		width = w.ws_col;
	}
#endif
	return width;
}

void printProgress(int current, int total, double fps, double /*elapsed*/) {
	double progress = (double)current / total * 100.0;
	// Get terminal width and calculate available space for progress bar
	int termWidth = getTerminalWidth();
	
	// Calculate space needed for the text parts
	// Format: "[bar] XXX.X% (XXXXX/XXXXX frames) FPS: XXX.X ETA: XXXXs "
	int currentDigits = std::to_string(current).length();
	int totalDigits = std::to_string(total).length();
	double eta = (fps > 0.001) ? (total - current) / fps : 0.0;
	int etaDigits = std::to_string(static_cast<int>(eta)).length();
	
	// Reserve space for: brackets(2) + percentage(7) + frames text + FPS text + ETA text + spaces
	int textWidth = 2 + 7 + 3 + currentDigits + 1 + totalDigits + 9 + 10 + 6 + etaDigits + 2 + 5;
	
	// Calculate bar width, ensure minimum of 10 characters
	int barWidth = termWidth - textWidth;
	if (barWidth < 10) barWidth = 10;
	if (barWidth > 100) barWidth = 100; // Cap at reasonable maximum
	
	int pos = static_cast<int>(barWidth * current / total);
	
	// Allow pos to reach barWidth for 100% completion
	if (pos > barWidth) pos = barWidth;
	
	std::cout << "\r[";
	for (int i = 0; i < barWidth; ++i) {
		if (i < pos) std::cout << "=";
		else if (i == pos && current < total) std::cout << ">";
		else std::cout << " ";
	}
	
	std::cout << "] " << std::fixed << std::setprecision(1) << progress << "% ";
	std::cout << "(" << current << "/" << total << " frames) ";
	std::cout << "FPS: " << std::setprecision(1) << fps << " ";
	std::cout << "ETA: " << std::setprecision(0) << eta << "s ";
	std::cout << std::flush;
}

int main(int argc, char* argv[]) {
	try {
		TIME_BLOCK("main_total");
		
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
		{
			TIME_BLOCK("edl_parsing");
			utils::Logger::info("Parsing EDL file: {}", opts.edlFile);
			edl::EDL edl = edl::EDLParser::parse(opts.edlFile);
		
			utils::Logger::info("EDL: {}x{} @ {} fps, {} clips",
				edl.width, edl.height, edl.fps, edl.clips.size());
		}
		
		// Re-parse EDL after timing block
		edl::EDL edl = edl::EDLParser::parse(opts.edlFile);
		
		// Initialize shared hardware context if hardware acceleration is requested
		AVBufferRef* sharedHwContext = nullptr;
		if (opts.hwDecode || opts.hwEncode) {
			media::HWConfig hwConfig;
			hwConfig.type = media::HardwareAcceleration::stringToHWAccelType(opts.hwAccelType);
			hwConfig.deviceIndex = opts.hwDevice;
			hwConfig.allowFallback = true;
			
			if (media::HardwareContextManager::getInstance().initialize(hwConfig)) {
				sharedHwContext = media::HardwareContextManager::getInstance().getSharedContext();
				utils::Logger::info("Shared hardware context initialized for GPU passthrough");
			} else {
				utils::Logger::warn("Failed to initialize shared hardware context, components will create their own");
			}
		}
		
		// Initialize decoders for all unique media files
		std::unordered_map<std::string, std::unique_ptr<media::FFmpegDecoder>> decoders;
		
		{
			TIME_BLOCK("decoder_initialization");
			for (const auto& clip : edl.clips) {
				if (clip.track.type != edl::Track::Video) {
					continue;
				}
				
				// Check if this is a media source (skip effect sources)
				if (!clip.source.has_value() || !std::holds_alternative<edl::MediaSource>(clip.source.value())) {
					continue;
				}
				
				const auto& mediaSource = std::get<edl::MediaSource>(clip.source.value());
				const std::string& uri = mediaSource.uri;
				if (decoders.find(uri) == decoders.end()) {
					std::string mediaPath = getMediaPath(uri, opts.edlFile);
					utils::Logger::info("Loading media: {} -> {}", uri, mediaPath);
					
					try {
						TIME_BLOCK(std::string("decoder_init_") + uri);
						// Configure decoder with hardware acceleration if requested
						media::FFmpegDecoder::Config decoderConfig;
						decoderConfig.useHardwareDecoder = opts.hwDecode;
						decoderConfig.hwConfig.type = media::HardwareAcceleration::stringToHWAccelType(opts.hwAccelType);
						decoderConfig.hwConfig.deviceIndex = opts.hwDevice;
						decoderConfig.hwConfig.allowFallback = true;
						// Enable GPU passthrough if both decode and encode use hardware
						decoderConfig.keepHardwareFrames = opts.hwDecode && opts.hwEncode;
						// Use shared hardware context if available
						decoderConfig.externalHwDeviceCtx = sharedHwContext;
						
						decoders[uri] = std::make_unique<media::FFmpegDecoder>(mediaPath, decoderConfig);
					} catch (const std::exception& e) {
						utils::Logger::error("Failed to load media {}: {}", mediaPath, e.what());
						throw;
					}
				}
			}
		}
		
		// Setup encoder
		media::FFmpegEncoder encoder(opts.outputFile, [&]() {
			TIME_BLOCK("encoder_initialization");
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
			// Use shared hardware context if available
			encoderConfig.externalHwDeviceCtx = sharedHwContext;
			// Enable GPU passthrough mode when both decode and encode use hardware
			encoderConfig.expectHardwareFrames = opts.hwDecode && opts.hwEncode;
			
			utils::Logger::info("Creating output file: {}", opts.outputFile);
			return encoderConfig;
		}());
		
		// Setup compositor
		compositor::FrameCompositor compositor(edl.width, edl.height, AV_PIX_FMT_YUV420P);
		
		// Setup instruction generator
		compositor::InstructionGenerator generator(edl);
		int totalFrames = generator.getTotalFrames();
		
		utils::Logger::info("Processing {} frames...", totalFrames);
		
		// Analyze if GPU passthrough is possible
		// Check if all decoders actually have hardware enabled (not just the command line flags)
		bool allDecodersHaveHardware = !decoders.empty();
		for (const auto& [uri, decoder] : decoders) {
			if (decoder && !decoder->isUsingHardware()) {
				allDecodersHaveHardware = false;
				break;
			}
		}
		
		bool canUseGPUPassthrough = allDecodersHaveHardware && opts.hwEncode;
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
		
		// Track timing for first few frames
		bool trackFirstFrames = true;
		int firstFramesToTrack = 10;
		
		for (const auto& instruction : generator) {
			std::shared_ptr<AVFrame> outputFrame;
			
			// Time the first few frames individually
			if (trackFirstFrames && frameCount < firstFramesToTrack) {
				TIME_BLOCK(std::string("frame_") + std::to_string(frameCount));
			}
			
			// Check if we can use GPU passthrough (no effects, transforms, or color generation)
			// Must check if decoder actually has hardware enabled, not just command line flags
			bool useGPUPassthrough = false;
			if (instruction.type == compositor::CompositorInstruction::DrawFrame) {
				auto it = decoders.find(instruction.uri);
				if (it != decoders.end() && it->second) {
					useGPUPassthrough = it->second->isUsingHardware() && opts.hwEncode && 
										!requiresCPUProcessing(instruction);
				}
			}
			
			if (useGPUPassthrough) {
				// GPU passthrough path - no CPU processing needed
				auto& decoder = decoders[instruction.uri];
				if (decoder) {
					// Get hardware frame directly from decoder
					auto hwFrame = decoder->getHardwareFrame(instruction.sourceFrameNumber);
					if (hwFrame) {
						// Write hardware frame directly to encoder
						if (!encoder.writeHardwareFrame(hwFrame.get())) {
							utils::Logger::error("Failed to write hardware frame {} to encoder", frameCount);
							// Try to continue with next frame
						}
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
						// Hardware frame failed - assume we've reached EOF or encountered an error
						utils::Logger::info("Failed to get hardware frame at output frame {} (source frame {}), stopping", 
							frameCount, instruction.sourceFrameNumber);
						// Stop processing
						break;
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
					
					if (!inputFrame) {
						utils::Logger::info("Failed to get frame at output frame {} (source frame {}), stopping", 
							frameCount, instruction.sourceFrameNumber);
						// Stop processing
						break;
					}
					
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
		
		// Print timing report if verbose mode is enabled
		if (opts.verbose) {
			utils::Timer::getInstance().printReport();
		}
		
		// Explicit cleanup to ensure proper destruction order
		// Clear decoders first (they may reference the shared hardware context)
		decoders.clear();
		
		// For hardware encoding, add a small delay to ensure GPU operations complete
		if (opts.hwEncode || opts.hwDecode) {
			// Give GPU time to finish any pending operations
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		
		// Reset the shared hardware context manager
		// This ensures it's cleaned up before static destruction
		media::HardwareContextManager::getInstance().reset();
		
		return 0;
		
	} catch (const std::exception& e) {
		utils::Logger::error("Fatal error: {}", e.what());
		
		// Ensure cleanup happens even on error
		media::HardwareContextManager::getInstance().reset();
		
		return 1;
	}
}