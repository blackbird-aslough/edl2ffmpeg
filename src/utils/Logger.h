#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <atomic>

namespace utils {

class Logger {
public:
	enum Level {
		ERROR = 0,
		WARN = 1,
		INFO = 2,
		DEBUG = 3
	};
	
	static void setLevel(Level level) {
		currentLevel = level;
	}
	
	template<typename... Args>
	static void error(const std::string& format, Args... args) {
		log(ERROR, "ERROR", format, args...);
	}
	
	template<typename... Args>
	static void warn(const std::string& format, Args... args) {
		log(WARN, "WARN", format, args...);
	}
	
	template<typename... Args>
	static void info(const std::string& format, Args... args) {
		log(INFO, "INFO", format, args...);
	}
	
	template<typename... Args>
	static void debug(const std::string& format, Args... args) {
		log(DEBUG, "DEBUG", format, args...);
	}
	
private:
	static std::atomic<Level> currentLevel;
	
	template<typename... Args>
	static void log(Level level, const std::string& levelStr,
		const std::string& format, Args... args) {
		
		if (level > currentLevel) {
			return;
		}
		
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		
		std::stringstream ss;
		ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
		ss << "[" << levelStr << "] ";
		
		// Simple format string replacement
		std::string message = format;
		size_t argIndex = 0;
		((replaceFirst(message, "{}", toString(args)), ++argIndex), ...);
		
		ss << message;
		
		if (level == ERROR) {
			std::cerr << ss.str() << std::endl;
		} else {
			std::cout << ss.str() << std::endl;
		}
	}
	
	static void replaceFirst(std::string& str, const std::string& from,
		const std::string& to) {
		size_t pos = str.find(from);
		if (pos != std::string::npos) {
			str.replace(pos, from.length(), to);
		}
	}
	
	template<typename T>
	static std::string toString(const T& value) {
		std::stringstream ss;
		ss << value;
		return ss.str();
	}
};

} // namespace utils