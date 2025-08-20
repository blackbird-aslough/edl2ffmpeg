#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <ApprovalTests/ApprovalTests.hpp>
#include <iostream>
#include <cstdlib>

// Custom reporter for video comparison results
class VideoComparisonReporter : public ApprovalTests::Reporter {
public:
	bool report(std::string received, std::string approved) const override {
		std::cout << "\n";
		std::cout << "Video comparison failed!\n";
		std::cout << "Received: " << received << "\n";
		std::cout << "Approved: " << approved << "\n";
		std::cout << "\nTo approve the new output, set UPDATE_GOLDEN=1\n";
		return true;
	}
};

int main(int argc, char* argv[]) {
	// Initialize ApprovalTests
	auto disposer = ApprovalTests::Approvals::useAsDefaultReporter(
		std::make_shared<VideoComparisonReporter>()
	);
	
	// Check for UPDATE_GOLDEN environment variable
	const char* updateGolden = std::getenv("UPDATE_GOLDEN");
	if (updateGolden && std::string(updateGolden) == "1") {
		std::cout << "UPDATE_GOLDEN mode enabled - will update golden files\n";
	}
	
	// Run Catch2 tests
	int result = Catch::Session().run(argc, argv);
	
	return result;
}