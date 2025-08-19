/**
 * test_nvenc_pipeline.cpp
 * 
 * Test program to demonstrate proper NVENC hardware acceleration setup,
 * zero-copy pipeline between decode and encode, and correct resource cleanup.
 * 
 * This test helps identify the correct cleanup sequence to avoid hangs
 * and resource leaks when using hardware acceleration.
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libswscale/swscale.h>
}

// Configuration
struct TestConfig {
	std::string inputFile = "test_input.mp4";
	std::string outputFile = "test_output.mp4";
	int maxFrames = 100;
	bool useHardware = true;
	bool verboseLogging = true;
};

// Logger
class Logger {
public:
	static void info(const std::string& msg) {
		std::cout << "[INFO] " << msg << std::endl;
	}
	
	static void error(const std::string& msg) {
		std::cerr << "[ERROR] " << msg << std::endl;
	}
	
	static void debug(const std::string& msg) {
		std::cout << "[DEBUG] " << msg << std::endl;
	}
};

// Test class
class NVENCPipelineTest {
public:
	NVENCPipelineTest(const TestConfig& config) : config_(config) {}
	
	~NVENCPipelineTest() {
		cleanup();
	}
	
	bool run() {
		try {
			Logger::info("Starting NVENC pipeline test");
			
			if (!initializeHardware()) {
				Logger::error("Failed to initialize hardware");
				return false;
			}
			
			if (!openInput()) {
				Logger::error("Failed to open input");
				return false;
			}
			
			if (!setupDecoder()) {
				Logger::error("Failed to setup decoder");
				return false;
			}
			
			if (!openOutput()) {
				Logger::error("Failed to open output");
				return false;
			}
			
			if (!setupEncoder()) {
				Logger::error("Failed to setup encoder");
				return false;
			}
			
			if (!processFrames()) {
				Logger::error("Failed to process frames");
				return false;
			}
			
			if (!finalize()) {
				Logger::error("Failed to finalize");
				return false;
			}
			
			Logger::info("Test completed successfully");
			return true;
			
		} catch (const std::exception& e) {
			Logger::error("Exception: " + std::string(e.what()));
			return false;
		}
	}
	
private:
	TestConfig config_;
	
	// Hardware context - shared between decoder and encoder
	AVBufferRef* hwDeviceCtx_ = nullptr;
	
	// Decoder members
	AVFormatContext* inputFormatCtx_ = nullptr;
	AVCodecContext* decoderCtx_ = nullptr;
	int videoStreamIndex_ = -1;
	
	// Encoder members
	AVFormatContext* outputFormatCtx_ = nullptr;
	AVCodecContext* encoderCtx_ = nullptr;
	AVStream* outputStream_ = nullptr;
	
	// Frame processing
	std::atomic<int> framesInFlight_{0};
	int processedFrames_ = 0;
	static constexpr int ASYNC_QUEUE_SIZE = 16;
	bool asyncMode_ = false;
	
	bool initializeHardware() {
		if (!config_.useHardware) {
			Logger::info("Hardware acceleration disabled");
			return true;
		}
		
		Logger::info("Initializing CUDA hardware context");
		
		// Create CUDA device context
		int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, AV_HWDEVICE_TYPE_CUDA,
										 nullptr, nullptr, 0);
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Failed to create CUDA device context: " + std::string(errbuf));
			return false;
		}
		
		Logger::info("CUDA device context created successfully");
		return true;
	}
	
	bool openInput() {
		Logger::info("Opening input file: " + config_.inputFile);
		
		int ret = avformat_open_input(&inputFormatCtx_, config_.inputFile.c_str(),
									  nullptr, nullptr);
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Failed to open input: " + std::string(errbuf));
			return false;
		}
		
		ret = avformat_find_stream_info(inputFormatCtx_, nullptr);
		if (ret < 0) {
			Logger::error("Failed to find stream info");
			return false;
		}
		
		// Find video stream
		for (unsigned int i = 0; i < inputFormatCtx_->nb_streams; i++) {
			if (inputFormatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				videoStreamIndex_ = i;
				break;
			}
		}
		
		if (videoStreamIndex_ == -1) {
			Logger::error("No video stream found");
			return false;
		}
		
		Logger::info("Found video stream at index " + std::to_string(videoStreamIndex_));
		return true;
	}
	
	bool setupDecoder() {
		AVStream* stream = inputFormatCtx_->streams[videoStreamIndex_];
		AVCodecID codecId = stream->codecpar->codec_id;
		
		const AVCodec* decoder = nullptr;
		
		if (config_.useHardware) {
			// Try hardware decoder first
			std::string hwDecoderName;
			switch (codecId) {
			case AV_CODEC_ID_H264:
				hwDecoderName = "h264_cuvid";
				break;
			case AV_CODEC_ID_HEVC:
				hwDecoderName = "hevc_cuvid";
				break;
			default:
				Logger::info("No hardware decoder for codec ID " + std::to_string(codecId));
				break;
			}
			
			if (!hwDecoderName.empty()) {
				decoder = avcodec_find_decoder_by_name(hwDecoderName.c_str());
				if (decoder) {
					Logger::info("Using hardware decoder: " + hwDecoderName);
				}
			}
		}
		
		// Fall back to software decoder
		if (!decoder) {
			decoder = avcodec_find_decoder(codecId);
			if (!decoder) {
				Logger::error("Decoder not found");
				return false;
			}
			Logger::info("Using software decoder: " + std::string(decoder->name));
		}
		
		decoderCtx_ = avcodec_alloc_context3(decoder);
		if (!decoderCtx_) {
			Logger::error("Failed to allocate decoder context");
			return false;
		}
		
		// Copy codec parameters
		int ret = avcodec_parameters_to_context(decoderCtx_, stream->codecpar);
		if (ret < 0) {
			Logger::error("Failed to copy codec parameters");
			return false;
		}
		
		// Set hardware device context for decoder
		if (config_.useHardware && hwDeviceCtx_) {
			decoderCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
			if (!decoderCtx_->hw_device_ctx) {
				Logger::error("Failed to reference hardware context for decoder");
				return false;
			}
			Logger::info("Hardware context set for decoder");
		}
		
		// Open decoder
		ret = avcodec_open2(decoderCtx_, decoder, nullptr);
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Failed to open decoder: " + std::string(errbuf));
			return false;
		}
		
		Logger::info("Decoder opened successfully");
		return true;
	}
	
	bool openOutput() {
		Logger::info("Opening output file: " + config_.outputFile);
		
		int ret = avformat_alloc_output_context2(&outputFormatCtx_, nullptr, nullptr,
												 config_.outputFile.c_str());
		if (ret < 0 || !outputFormatCtx_) {
			Logger::error("Failed to allocate output context");
			return false;
		}
		
		// Create video stream
		outputStream_ = avformat_new_stream(outputFormatCtx_, nullptr);
		if (!outputStream_) {
			Logger::error("Failed to create output stream");
			return false;
		}
		
		return true;
	}
	
	bool setupEncoder() {
		const AVCodec* encoder = nullptr;
		
		if (config_.useHardware) {
			encoder = avcodec_find_encoder_by_name("h264_nvenc");
			if (encoder) {
				Logger::info("Using hardware encoder: h264_nvenc");
			}
		}
		
		// Fall back to software encoder
		if (!encoder) {
			encoder = avcodec_find_encoder_by_name("libx264");
			if (!encoder) {
				encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			}
			if (!encoder) {
				Logger::error("Encoder not found");
				return false;
			}
			Logger::info("Using software encoder: " + std::string(encoder->name));
		}
		
		encoderCtx_ = avcodec_alloc_context3(encoder);
		if (!encoderCtx_) {
			Logger::error("Failed to allocate encoder context");
			return false;
		}
		
		// Set encoder parameters
		encoderCtx_->width = decoderCtx_->width;
		encoderCtx_->height = decoderCtx_->height;
		encoderCtx_->time_base = {1, 30}; // 30 fps
		encoderCtx_->framerate = {30, 1};
		encoderCtx_->gop_size = 30;
		encoderCtx_->max_b_frames = 0; // Disable B-frames for simplicity
		encoderCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
		
		// Set hardware device context for encoder
		if (config_.useHardware && hwDeviceCtx_) {
			encoderCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
			if (!encoderCtx_->hw_device_ctx) {
				Logger::error("Failed to reference hardware context for encoder");
				return false;
			}
			
			// For NVENC, we need to create hw_frames_ctx
			AVBufferRef* hwFramesRef = av_hwframe_ctx_alloc(hwDeviceCtx_);
			if (!hwFramesRef) {
				Logger::error("Failed to allocate hardware frames context");
				return false;
			}
			
			AVHWFramesContext* hwFramesCtx = (AVHWFramesContext*)hwFramesRef->data;
			hwFramesCtx->format = AV_PIX_FMT_CUDA;
			hwFramesCtx->sw_format = AV_PIX_FMT_YUV420P;
			hwFramesCtx->width = encoderCtx_->width;
			hwFramesCtx->height = encoderCtx_->height;
			hwFramesCtx->initial_pool_size = 20;
			
			int ret = av_hwframe_ctx_init(hwFramesRef);
			if (ret < 0) {
				av_buffer_unref(&hwFramesRef);
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				Logger::error("Failed to init hardware frames context: " + std::string(errbuf));
				return false;
			}
			
			encoderCtx_->hw_frames_ctx = hwFramesRef;
			encoderCtx_->pix_fmt = AV_PIX_FMT_CUDA;
			
			Logger::info("Hardware frames context set for encoder");
		}
		
		// Set quality
		if (outputFormatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
			encoderCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
		
		av_opt_set(encoderCtx_->priv_data, "preset", "p4", 0); // Balanced preset
		av_opt_set(encoderCtx_->priv_data, "rc", "vbr", 0);
		encoderCtx_->bit_rate = 4000000; // 4 Mbps
		
		// Set async options BEFORE opening encoder
		if (config_.useHardware) {
			asyncMode_ = true;
			Logger::info("Setting async encoding options for hardware encoder");
			
			// Set async depth for NVENC
			av_opt_set_int(encoderCtx_->priv_data, "delay", 0, 0); // No B-frame delay
			av_opt_set_int(encoderCtx_->priv_data, "surfaces", ASYNC_QUEUE_SIZE * 2, 0);
		}
		
		// Open encoder
		int ret = avcodec_open2(encoderCtx_, encoder, nullptr);
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Failed to open encoder: " + std::string(errbuf));
			return false;
		}
		
		// Copy codec parameters to output stream
		ret = avcodec_parameters_from_context(outputStream_->codecpar, encoderCtx_);
		if (ret < 0) {
			Logger::error("Failed to copy codec parameters to stream");
			return false;
		}
		
		outputStream_->time_base = encoderCtx_->time_base;
		
		// Open output file
		if (!(outputFormatCtx_->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&outputFormatCtx_->pb, config_.outputFile.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				Logger::error("Failed to open output file");
				return false;
			}
		}
		
		// Write header
		ret = avformat_write_header(outputFormatCtx_, nullptr);
		if (ret < 0) {
			Logger::error("Failed to write header");
			return false;
		}
		
		Logger::info("Encoder setup completed");
		return true;
	}
	
	bool processFrames() {
		Logger::info("Starting frame processing");
		
		AVPacket* packet = av_packet_alloc();
		AVFrame* frame = av_frame_alloc();
		AVFrame* swFrame = nullptr;
		
		if (!packet || !frame) {
			Logger::error("Failed to allocate packet or frame");
			av_packet_free(&packet);
			av_frame_free(&frame);
			return false;
		}
		
		// For software fallback when hardware frames can't be used directly
		if (config_.useHardware) {
			swFrame = av_frame_alloc();
			if (!swFrame) {
				Logger::error("Failed to allocate software frame");
				av_packet_free(&packet);
				av_frame_free(&frame);
				return false;
			}
		}
		
		int ret;
		bool success = true;
		
		// Read and decode frames
		while (processedFrames_ < config_.maxFrames) {
			ret = av_read_frame(inputFormatCtx_, packet);
			if (ret < 0) {
				if (ret == AVERROR_EOF) {
					Logger::info("End of input file reached");
					break;
				}
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				Logger::error("Error reading frame: " + std::string(errbuf));
				success = false;
				break;
			}
			
			if (packet->stream_index != videoStreamIndex_) {
				av_packet_unref(packet);
				continue;
			}
			
			// Send packet to decoder
			ret = avcodec_send_packet(decoderCtx_, packet);
			av_packet_unref(packet);
			
			if (ret < 0) {
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				Logger::error("Error sending packet to decoder: " + std::string(errbuf));
				success = false;
				break;
			}
			
			// Receive frames from decoder
			while (ret >= 0) {
				ret = avcodec_receive_frame(decoderCtx_, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					char errbuf[AV_ERROR_MAX_STRING_SIZE];
					av_strerror(ret, errbuf, sizeof(errbuf));
					Logger::error("Error receiving frame from decoder: " + std::string(errbuf));
					success = false;
					break;
				}
				
				// Process the decoded frame
				if (!encodeFrame(frame, swFrame)) {
					Logger::error("Failed to encode frame");
					success = false;
					break;
				}
				
				processedFrames_++;
				if (processedFrames_ % 10 == 0) {
					Logger::info("Processed " + std::to_string(processedFrames_) + " frames");
				}
				
				av_frame_unref(frame);
			}
			
			if (!success) break;
		}
		
		// Flush decoder
		if (success) {
			Logger::info("Flushing decoder");
			avcodec_send_packet(decoderCtx_, nullptr);
			
			while (true) {
				ret = avcodec_receive_frame(decoderCtx_, frame);
				if (ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					break;
				}
				
				encodeFrame(frame, swFrame);
				av_frame_unref(frame);
			}
		}
		
		// Cleanup
		av_packet_free(&packet);
		av_frame_free(&frame);
		if (swFrame) {
			av_frame_free(&swFrame);
		}
		
		Logger::info("Frame processing completed. Total frames: " + std::to_string(processedFrames_));
		return success;
	}
	
	bool encodeFrame(AVFrame* frame, AVFrame* swFrame) {
		AVFrame* frameToEncode = frame;
		
		// Check if we need to handle hardware frames
		if (config_.useHardware) {
			// For zero-copy, we want to keep frames in GPU memory
			if (frame->hw_frames_ctx) {
				// Frame is already in GPU memory, use it directly
				frameToEncode = frame;
				Logger::debug("Using hardware frame directly (zero-copy)");
			} else if (encoderCtx_->hw_frames_ctx) {
				// Need to upload software frame to GPU
				Logger::debug("Uploading software frame to GPU");
				
				// Get a hardware frame from the encoder's pool
				AVFrame* hwFrame = av_frame_alloc();
				if (!hwFrame) {
					Logger::error("Failed to allocate hardware frame");
					return false;
				}
				
				hwFrame->format = encoderCtx_->pix_fmt;
				hwFrame->width = frame->width;
				hwFrame->height = frame->height;
				
				int ret = av_hwframe_get_buffer(encoderCtx_->hw_frames_ctx, hwFrame, 0);
				if (ret < 0) {
					av_frame_free(&hwFrame);
					Logger::error("Failed to get hardware buffer");
					return false;
				}
				
				ret = av_hwframe_transfer_data(hwFrame, frame, 0);
				if (ret < 0) {
					av_frame_free(&hwFrame);
					Logger::error("Failed to transfer data to hardware");
					return false;
				}
				
				// Copy metadata
				ret = av_frame_copy_props(hwFrame, frame);
				if (ret < 0) {
					av_frame_free(&hwFrame);
					Logger::error("Failed to copy frame properties");
					return false;
				}
				
				frameToEncode = hwFrame;
			}
		}
		
		// Set proper PTS
		frameToEncode->pts = av_rescale_q(processedFrames_, {1, 30}, encoderCtx_->time_base);
		
		// Use async or sync encoding based on mode
		if (asyncMode_) {
			return encodeFrameAsync(frameToEncode);
		} else {
			return encodeFrameSync(frameToEncode);
		}
	}
	
	bool encodeFrameAsync(AVFrame* frame) {
		// Send frame to encoder without waiting for packet
		int ret = avcodec_send_frame(encoderCtx_, frame);
		
		if (ret < 0 && ret != AVERROR(EAGAIN)) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Error sending frame to encoder (async): " + std::string(errbuf));
			return false;
		}
		
		if (ret == 0 && frame != nullptr) {
			framesInFlight_++;
			Logger::debug("Async frame sent, frames in flight: " + std::to_string(framesInFlight_.load()));
			
			// Try to receive packets if queue is getting full
			if (framesInFlight_ >= ASYNC_QUEUE_SIZE - 2) {
				Logger::debug("Queue getting full, draining packets");
				receivePacketsAsync();
			}
		}
		
		// Don't free the frame here in async mode - it may still be in use by the encoder
		
		return true;
	}
	
	bool receivePacketsAsync() {
		bool receivedAny = false;
		
		// Try to receive multiple packets
		while (true) {
			AVPacket* pkt = av_packet_alloc();
			if (!pkt) {
				break;
			}
			
			int ret = avcodec_receive_packet(encoderCtx_, pkt);
			
			if (ret == AVERROR(EAGAIN)) {
				// No more packets available right now
				av_packet_free(&pkt);
				break;
			} else if (ret == AVERROR_EOF) {
				// End of stream
				av_packet_free(&pkt);
				framesInFlight_ = 0;
				break;
			} else if (ret < 0) {
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				Logger::error("Error receiving packet from encoder (async): " + std::string(errbuf));
				av_packet_free(&pkt);
				break;
			}
			
			// Got a packet, write it
			pkt->stream_index = outputStream_->index;
			av_packet_rescale_ts(pkt, encoderCtx_->time_base, outputStream_->time_base);
			
			ret = av_interleaved_write_frame(outputFormatCtx_, pkt);
			av_packet_free(&pkt);
			
			if (ret < 0) {
				Logger::error("Error writing async packet");
				break;
			}
			
			if (framesInFlight_ > 0) {
				framesInFlight_--;
			}
			receivedAny = true;
		}
		
		Logger::debug("Async packets received, frames in flight: " + std::to_string(framesInFlight_.load()));
		return receivedAny;
	}
	
	bool encodeFrameSync(AVFrame* frame) {
		// Send frame to encoder
		int ret = avcodec_send_frame(encoderCtx_, frame);
		
		// Don't free the frame here - caller is responsible
		
		if (ret < 0) {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Error sending frame to encoder: " + std::string(errbuf));
			return false;
		}
		
		// Receive packets from encoder
		AVPacket* pkt = av_packet_alloc();
		if (!pkt) {
			Logger::error("Failed to allocate packet");
			return false;
		}
		
		while (ret >= 0) {
			ret = avcodec_receive_packet(encoderCtx_, pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, errbuf, sizeof(errbuf));
				Logger::error("Error receiving packet from encoder: " + std::string(errbuf));
				av_packet_free(&pkt);
				return false;
			}
			
			// Write packet
			pkt->stream_index = outputStream_->index;
			av_packet_rescale_ts(pkt, encoderCtx_->time_base, outputStream_->time_base);
			
			ret = av_interleaved_write_frame(outputFormatCtx_, pkt);
			if (ret < 0) {
				Logger::error("Error writing packet");
				av_packet_free(&pkt);
				return false;
			}
			
			av_packet_unref(pkt);
		}
		
		av_packet_free(&pkt);
		return true;
	}
	
	bool finalize() {
		Logger::info("Finalizing output");
		
		if (asyncMode_) {
			// For async mode, first process any remaining frames in the queue
			Logger::info("Processing remaining async frames, frames in flight: " + std::to_string(framesInFlight_.load()));
			int flushAttempts = 0;
			while (framesInFlight_ > 0 && flushAttempts < 100) {
				bool received = receivePacketsAsync();
				if (!received) {
					// No packets received, wait a bit before retrying
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				flushAttempts++;
			}
			
			if (framesInFlight_ > 0) {
				Logger::error("Still have " + std::to_string(framesInFlight_.load()) + " frames in flight after flush attempts");
			}
		}
		
		// Flush encoder
		Logger::info("Flushing encoder");
		int ret = avcodec_send_frame(encoderCtx_, nullptr);
		
		if (ret >= 0 || ret == AVERROR_EOF) {
			// Drain all remaining packets
			int maxIterations = 1000;  // Prevent infinite loop
			int iterations = 0;
			int drainCount = 0;
			bool done = false;
			
			while (!done && iterations < maxIterations) {
				AVPacket* pkt = av_packet_alloc();
				if (!pkt) {
					break;
				}
				
				ret = avcodec_receive_packet(encoderCtx_, pkt);
				if (ret == AVERROR(EAGAIN)) {
					// For async mode, EAGAIN might mean we need to wait
					if (asyncMode_ && iterations < 10) {
						av_packet_free(&pkt);
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
						iterations++;
						continue;
					}
					// No more packets available, we're done
					av_packet_free(&pkt);
					done = true;
				} else if (ret == AVERROR_EOF || ret < 0) {
					// End of stream or error
					done = true;
					av_packet_free(&pkt);
					if (ret == AVERROR_EOF) {
						Logger::info("Encoder flushed successfully (EOF)");
					}
				} else {
					// Got a packet
					pkt->stream_index = outputStream_->index;
					av_packet_rescale_ts(pkt, encoderCtx_->time_base, outputStream_->time_base);
					
					ret = av_interleaved_write_frame(outputFormatCtx_, pkt);
					if (ret < 0) {
						Logger::error("Error writing packet during flush");
					}
					drainCount++;
					av_packet_free(&pkt);
				}
				
				iterations++;
			}
			
			Logger::info("Drained " + std::to_string(drainCount) + " packets from encoder during flush");
			
			if (iterations >= maxIterations) {
				Logger::error("Hit max iterations during flush");
			}
		} else {
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			Logger::error("Failed to send flush frame to encoder: " + std::string(errbuf));
		}
		
		// Reset frames in flight
		framesInFlight_ = 0;
		
		// Write trailer
		ret = av_write_trailer(outputFormatCtx_);
		if (ret < 0) {
			Logger::error("Failed to write trailer");
			return false;
		}
		
		Logger::info("Finalization completed");
		return true;
	}
	
	void cleanup() {
		Logger::info("Starting cleanup");
		
		// Close output file
		if (outputFormatCtx_ && !(outputFormatCtx_->oformat->flags & AVFMT_NOFILE)) {
			avio_closep(&outputFormatCtx_->pb);
		}
		
		// Free encoder context (must be done before freeing hardware context)
		if (encoderCtx_) {
			Logger::debug("Freeing encoder context");
			// For hardware encoders, ensure everything is flushed
			if (config_.useHardware) {
				// Close the codec first
				avcodec_close(encoderCtx_);
				Logger::debug("Encoder codec closed");
				
				// Small delay to ensure GPU operations complete
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			avcodec_free_context(&encoderCtx_);
			Logger::debug("Encoder context freed");
		}
		
		// Free decoder context
		if (decoderCtx_) {
			Logger::debug("Freeing decoder context");
			if (config_.useHardware) {
				avcodec_close(decoderCtx_);
				Logger::debug("Decoder codec closed");
			}
			avcodec_free_context(&decoderCtx_);
			Logger::debug("Decoder context freed");
		}
		
		// Free format contexts
		if (outputFormatCtx_) {
			Logger::debug("Freeing output format context");
			avformat_free_context(outputFormatCtx_);
			outputFormatCtx_ = nullptr;
		}
		
		if (inputFormatCtx_) {
			Logger::debug("Freeing input format context");
			avformat_close_input(&inputFormatCtx_);
		}
		
		// Free hardware device context last
		if (hwDeviceCtx_) {
			Logger::debug("Freeing hardware device context");
			av_buffer_unref(&hwDeviceCtx_);
		}
		
		Logger::info("Cleanup completed");
	}
};

// Helper function to create test input
bool createTestInput(const std::string& filename) {
	Logger::info("Creating test input file: " + filename);
	
	std::string cmd = "ffmpeg -y -f lavfi -i testsrc=duration=10:size=1920x1080:rate=30 "
					  "-c:v libx264 -preset ultrafast -pix_fmt yuv420p " + filename + " 2>/dev/null";
	
	int ret = system(cmd.c_str());
	if (ret != 0) {
		Logger::error("Failed to create test input");
		return false;
	}
	
	Logger::info("Test input created successfully");
	return true;
}

int main(int argc, char** argv) {
	// Initialize FFmpeg
	av_log_set_level(AV_LOG_WARNING);
	
	TestConfig config;
	
	// Parse arguments
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--no-hardware") {
			config.useHardware = false;
		} else if (arg == "--frames" && i + 1 < argc) {
			config.maxFrames = std::atoi(argv[++i]);
		} else if (arg == "--input" && i + 1 < argc) {
			config.inputFile = argv[++i];
		} else if (arg == "--output" && i + 1 < argc) {
			config.outputFile = argv[++i];
		} else if (arg == "--quiet") {
			config.verboseLogging = false;
			av_log_set_level(AV_LOG_ERROR);
		}
	}
	
	// Create test input if it doesn't exist
	FILE* f = fopen(config.inputFile.c_str(), "r");
	if (!f) {
		if (!createTestInput(config.inputFile)) {
			return 1;
		}
	} else {
		fclose(f);
	}
	
	// Run test
	Logger::info("=== NVENC Pipeline Test ===");
	Logger::info("Configuration:");
	Logger::info("  Input: " + config.inputFile);
	Logger::info("  Output: " + config.outputFile);
	Logger::info("  Max frames: " + std::to_string(config.maxFrames));
	Logger::info("  Hardware acceleration: " + std::string(config.useHardware ? "enabled" : "disabled"));
	
	NVENCPipelineTest test(config);
	bool success = test.run();
	
	if (success) {
		Logger::info("=== TEST PASSED ===");
		return 0;
	} else {
		Logger::error("=== TEST FAILED ===");
		return 1;
	}
}