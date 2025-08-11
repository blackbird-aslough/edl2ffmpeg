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