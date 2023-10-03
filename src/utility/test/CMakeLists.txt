# Copyright 2022 - V-Nova Ltd.
#
include("Sources.cmake")

find_package(GTest REQUIRED)
find_package(fmt REQUIRED)
find_package(xxHash REQUIRED)
find_package(CLI11 REQUIRED)
if(VN_SDK_USE_GHC_FILESYSTEM)
	find_package(ghc_filesystem REQUIRED)
endif()

add_executable(lcevc_dec_utility_test ${SOURCES})
lcevc_set_properties(lcevc_dec_utility_test)

target_link_libraries(lcevc_dec_utility_test
	lcevc_dec::utility
	lcevc_dec::api
	GTest::gtest
	GTest::gtest_main
	fmt::fmt
	CLI11::CLI11
	xxHash::xxHash
	$<TARGET_NAME_IF_EXISTS:ghcFilesystem::ghc_filesystem>
	)

# Tests can include interal headers from helper sources
target_include_directories(lcevc_dec_utility_test PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../src")

add_executable(lcevc_dec::utility_test ALIAS lcevc_dec_utility_test)

install(TARGETS lcevc_dec_utility_test DESTINATION "bin")
