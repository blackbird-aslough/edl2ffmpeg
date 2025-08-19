#pragma once

#include <chrono>
#include <string>
#include <map>
#include <iostream>
#include <iomanip>

namespace utils {

class Timer {
public:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;
	using Duration = std::chrono::duration<double>;
	
	struct TimingResult {
		double totalTime = 0.0;
		int count = 0;
		double minTime = std::numeric_limits<double>::max();
		double maxTime = 0.0;
		
		void add(double time) {
			totalTime += time;
			count++;
			minTime = std::min(minTime, time);
			maxTime = std::max(maxTime, time);
		}
		
		double average() const {
			return count > 0 ? totalTime / count : 0.0;
		}
	};
	
	class ScopedTimer {
	public:
		ScopedTimer(Timer& timer, const std::string& name)
			: timer_(timer), name_(name), start_(Clock::now()) {}
		
		~ScopedTimer() {
			auto end = Clock::now();
			Duration elapsed = end - start_;
			timer_.addTiming(name_, elapsed.count());
		}
		
	private:
		Timer& timer_;
		std::string name_;
		TimePoint start_;
	};
	
	static Timer& getInstance() {
		static Timer instance;
		return instance;
	}
	
	ScopedTimer scope(const std::string& name) {
		return ScopedTimer(*this, name);
	}
	
	void addTiming(const std::string& name, double seconds) {
		results_[name].add(seconds);
	}
	
	void printReport() const {
		if (results_.empty()) {
			return;
		}
		
		std::cout << "\n=== Performance Timing Report ===\n";
		std::cout << std::setw(40) << std::left << "Operation"
				  << std::setw(12) << std::right << "Total (s)"
				  << std::setw(10) << "Count"
				  << std::setw(12) << "Avg (ms)"
				  << std::setw(12) << "Min (ms)"
				  << std::setw(12) << "Max (ms)" << "\n";
		std::cout << std::string(98, '-') << "\n";
		
		for (const auto& [name, result] : results_) {
			std::cout << std::setw(40) << std::left << name
					  << std::setw(12) << std::right << std::fixed << std::setprecision(3) << result.totalTime
					  << std::setw(10) << result.count
					  << std::setw(12) << std::setprecision(1) << result.average() * 1000.0
					  << std::setw(12) << result.minTime * 1000.0
					  << std::setw(12) << result.maxTime * 1000.0 << "\n";
		}
		std::cout << std::string(98, '-') << "\n";
	}
	
	void reset() {
		results_.clear();
	}
	
private:
	std::map<std::string, TimingResult> results_;
};

// Convenience macro for timing a block
#define TIME_BLOCK(name) utils::Timer::ScopedTimer _timer_##__LINE__(utils::Timer::getInstance(), name)

} // namespace utils