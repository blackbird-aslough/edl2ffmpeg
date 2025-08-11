enable_testing()

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

# Custom target to generate test fixtures
add_custom_target(generate_test_fixtures
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generate_fixtures.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
	COMMENT "Generating test video fixtures..."
)

# Integration tests (to be added)
# add_executable(test_integration
#	integration/test_render.cpp
#	# Link all necessary source files
# )
#
# target_link_libraries(test_integration PRIVATE
#	${FFMPEG_LIBRARIES}
#	nlohmann_json::nlohmann_json
# )
#
# add_test(NAME Integration COMMAND test_integration)

# Custom target to run all tests with fixtures
add_custom_target(test_with_fixtures
	COMMAND ${CMAKE_COMMAND} --build . --target generate_test_fixtures
	COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
	DEPENDS generate_test_fixtures
	COMMENT "Running tests with generated fixtures..."
)