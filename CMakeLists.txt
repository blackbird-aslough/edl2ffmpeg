cmake_minimum_required(VERSION 3.16)
project(edl2ffmpeg VERSION 1.0.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENABLE_GPU "Enable GPU acceleration" ON)
option(USE_SYSTEM_FFMPEG "Use system FFmpeg instead of building" ON)
option(PROFILE_BUILD "Enable profiling instrumentation" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
	set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
	
	if(ENABLE_SIMD)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	endif()
	
	if(PROFILE_BUILD)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
	endif()
endif()

# Find packages
find_package(PkgConfig REQUIRED)

# FFmpeg libraries
if(USE_SYSTEM_FFMPEG)
	pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
		libavcodec
		libavformat
		libavutil
		libswscale
		libswresample
	)
	
	# Check FFmpeg version and set compatibility defines
	execute_process(COMMAND pkg-config --modversion libavcodec
		OUTPUT_VARIABLE AVCODEC_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(STATUS "Found libavcodec version: ${AVCODEC_VERSION}")
	
	# Parse version to get major/minor numbers
	string(REPLACE "." ";" VERSION_LIST ${AVCODEC_VERSION})
	list(GET VERSION_LIST 0 AVCODEC_MAJOR)
	list(GET VERSION_LIST 1 AVCODEC_MINOR)
	list(GET VERSION_LIST 2 AVCODEC_MICRO)
	
	# Calculate version integer for comparison (major * 10000 + minor * 100 + micro)
	math(EXPR AVCODEC_VERSION_INT "${AVCODEC_MAJOR} * 10000 + ${AVCODEC_MINOR} * 100 + ${AVCODEC_MICRO}")
	message(STATUS "libavcodec version integer: ${AVCODEC_VERSION_INT}")
	
	# Set compatibility defines based on version
	if(${AVCODEC_VERSION_INT} GREATER_EQUAL 573700)
		message(STATUS "Using new FFmpeg API (send/receive, codecpar)")
		add_definitions(-DHAVE_SEND_RECEIVE_API=1)
		add_definitions(-DHAVE_CODECPAR_API=1)
	else()
		message(STATUS "Using legacy FFmpeg API (decode_video2, stream->codec)")
		add_definitions(-DHAVE_SEND_RECEIVE_API=0)
		add_definitions(-DHAVE_CODECPAR_API=0)
	endif()
	
	if(${AVCODEC_VERSION_INT} GREATER_EQUAL 578900)
		add_definitions(-DHAVE_PACKET_ALLOC_API=1)
	else()
		add_definitions(-DHAVE_PACKET_ALLOC_API=0)
	endif()
	
	# Disable GPU features for legacy FFmpeg (< 3.2)
	# Hardware acceleration APIs were introduced in FFmpeg 3.2 (57.64.100)
	if(${AVCODEC_VERSION_INT} LESS 576400)
		message(WARNING "Disabling GPU acceleration for legacy FFmpeg version ${AVCODEC_VERSION}")
		set(ENABLE_GPU OFF)
		add_definitions(-DHAVE_HWDEVICE_API=0)
	else()
		add_definitions(-DHAVE_HWDEVICE_API=1)
	endif()
	
endif()

# JSON library - using nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
	include(FetchContent)
	FetchContent_Declare(json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.11.2
	)
	FetchContent_MakeAvailable(json)
endif()

# Platform detection for GPU acceleration
if(ENABLE_GPU)
	if(APPLE)
		# macOS - VideoToolbox is included in the system
		message(STATUS "GPU acceleration: VideoToolbox (macOS)")
		add_definitions(-DHAVE_VIDEOTOOLBOX=1)
	elseif(UNIX AND NOT APPLE)
		# Linux - Check for NVIDIA and VAAPI
		find_package(PkgConfig)
		
		# Check for CUDA/NVENC support in FFmpeg
		execute_process(
			COMMAND ffmpeg -hide_banner -hwaccels
			OUTPUT_VARIABLE FFMPEG_HWACCELS
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		string(FIND "${FFMPEG_HWACCELS}" "cuda" CUDA_POS)
		if(NOT ${CUDA_POS} EQUAL -1)
			message(STATUS "GPU acceleration: NVENC/NVDEC (FFmpeg has CUDA support)")
			add_definitions(-DHAVE_NVENC=1)
		endif()
		
		# Check for VAAPI support (Intel/AMD)
		pkg_check_modules(LIBVA libva)
		if(LIBVA_FOUND)
			message(STATUS "GPU acceleration: VAAPI (libva found)")
			add_definitions(-DHAVE_VAAPI=1)
		endif()
		
		if(NOT CUDA_FOUND AND NOT LIBVA_FOUND)
			message(WARNING "No GPU acceleration libraries found on Linux")
		endif()
	endif()
else()
	message(STATUS "GPU acceleration disabled")
endif()

# Source files
set(SOURCES
	src/main.cpp
	src/edl/EDLParser.cpp
	src/compositor/InstructionGenerator.cpp
	src/compositor/FrameCompositor.cpp
	src/media/FFmpegDecoder.cpp
	src/media/FFmpegEncoder.cpp
	src/media/FFmpegCompat.cpp
	src/media/HardwareAcceleration.cpp
	src/media/HardwareContextManager.cpp
	src/utils/Logger.cpp
	src/utils/FrameBuffer.cpp
)

# Main executable
add_executable(edl2ffmpeg ${SOURCES})

# Include directories
target_include_directories(edl2ffmpeg PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(edl2ffmpeg PRIVATE
	PkgConfig::LIBAV
	nlohmann_json::nlohmann_json
)

# Tests
if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# Test programs
add_executable(test_nvenc_pipeline test_nvenc_pipeline.cpp)
target_include_directories(test_nvenc_pipeline PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_nvenc_pipeline PRIVATE
	PkgConfig::LIBAV
)

# Installation
install(TARGETS edl2ffmpeg
	RUNTIME DESTINATION bin
)