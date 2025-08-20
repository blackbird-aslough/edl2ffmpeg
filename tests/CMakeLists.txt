enable_testing()

# Fetch external testing libraries
include(FetchContent)

# Catch2 testing framework
FetchContent_Declare(
	Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG v3.5.0
)

# ApprovalTests for golden file testing
FetchContent_Declare(
	ApprovalTests
	GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp.git
	GIT_TAG v.10.13.0
)

FetchContent_MakeAvailable(Catch2 ApprovalTests)

# Test executable for EDL parser
add_executable(test_edl_parser test_edl_parser.cpp
	${CMAKE_SOURCE_DIR}/src/edl/EDLParser.cpp
	${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
)

target_include_directories(test_edl_parser PRIVATE
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_edl_parser PRIVATE
	nlohmann_json::nlohmann_json
)

add_test(NAME EDLParser COMMAND test_edl_parser)

# Set test data directory
set_tests_properties(EDLParser PROPERTIES
	ENVIRONMENT "TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/sample_edls"
)

# Integration test sources
set(INTEGRATION_TEST_SOURCES
	integration/common/VideoComparator.cpp
	integration/common/TestRunner.cpp
	integration/common/EDLGenerator.cpp
	integration/approval/test_effects.cpp
	integration/approval/test_clips.cpp
	integration/approval/test_transitions.cpp
	integration/generative/test_random_edls.cpp
	integration/main.cpp
)

# Integration test executable
add_executable(test_integration ${INTEGRATION_TEST_SOURCES})

target_include_directories(test_integration PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_BINARY_DIR}  # For generated config headers
)

target_link_libraries(test_integration PRIVATE
	Catch2::Catch2
	ApprovalTests::ApprovalTests
	PkgConfig::LIBAV  # Use the same FFmpeg target as main executable
	nlohmann_json::nlohmann_json
	${CMAKE_THREAD_LIBS_INIT}
)

# Register integration tests with CTest
# Note: Catch test discovery happens at build time, not configure time
# Disable automatic discovery if it causes issues
# include(Catch)
# catch_discover_tests(test_integration
# 	EXTRA_ARGS --reporter compact
# 	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
# )

# Add manual test instead
add_test(NAME IntegrationTests COMMAND test_integration)

# Custom target to generate test fixtures
add_custom_target(generate_test_fixtures
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generate_fixtures.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
	COMMENT "Generating test video fixtures..."
)

# Custom targets for different test types
add_custom_target(test_approval
	COMMAND test_integration "[approval]" --reporter compact
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS test_integration
	COMMENT "Running approval tests..."
)

add_custom_target(test_generative
	COMMAND test_integration "[generative]" --rng-seed \${SEED:-time}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS test_integration
	COMMENT "Running generative tests..."
)

add_custom_target(test_quick
	COMMAND test_integration "[quick]" --reporter compact
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS test_integration
	COMMENT "Running quick smoke tests..."
)

# Custom target to run all tests with fixtures
add_custom_target(test_with_fixtures
	COMMAND ${CMAKE_COMMAND} --build . --target generate_test_fixtures
	COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
	DEPENDS generate_test_fixtures
	COMMENT "Running tests with generated fixtures..."
)

# Custom target to update golden files
add_custom_target(update_golden
	COMMAND ${CMAKE_COMMAND} -E env UPDATE_GOLDEN=1 test_integration "[approval]"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS test_integration
	COMMENT "Updating golden reference files..."
)