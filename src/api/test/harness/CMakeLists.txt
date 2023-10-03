# Copyright 2023 - V-Nova Ltd.
#
include("Sources.cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)
find_package(xxHash REQUIRED)

if(VN_SDK_EXECUTABLES)
	add_executable(lcevc_api_test_harness ${SOURCES})
	lcevc_set_properties(lcevc_api_test_harness)

	target_link_libraries(lcevc_api_test_harness
		lcevc_dec::api
		lcevc_dec::utility
		fmt::fmt
		CLI11::CLI11
		xxHash::xxHash
		)

	# Tests can include interal headers from helper sources
	target_include_directories(lcevc_api_test_harness PUBLIC "${CMAKE_SOURCE_DIR}/src/utility/src")

	add_executable(lcevc_dec::api_test_harness ALIAS lcevc_api_test_harness)

	install(TARGETS "lcevc_api_test_harness"  DESTINATION "bin")
endif()
