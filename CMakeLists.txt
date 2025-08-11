cmake_minimum_required(VERSION 3.16)
project(edl2ffmpeg VERSION 1.0.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENABLE_GPU "Enable GPU acceleration" OFF)
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

# Source files
set(SOURCES
	src/main.cpp
	src/edl/EDLParser.cpp
	src/compositor/InstructionGenerator.cpp
	src/compositor/FrameCompositor.cpp
	src/media/FFmpegDecoder.cpp
	src/media/FFmpegEncoder.cpp
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

# Installation
install(TARGETS edl2ffmpeg
	RUNTIME DESTINATION bin
)