# Copyright 2023 - V-Nova Ltd.

cmake_minimum_required(VERSION 3.17.1 FATAL_ERROR)

## Configuration
#
# General
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(VN_MSVC_RUNTIME_STATIC  "Use static multithreaded runtime for MSVC" ON)
option(VN_STRIP_RELEASE_BUILDS "Compile Release builds without debug symbols." ON)

# SDK
option(VN_SDK_EXECUTABLES "SDK: Include samples and test harness executables in SDK installations" ON)
option(VN_SDK_SAMPLE_SOURCE "SDK: Include sample sources n SDK installations" ON)
option(VN_SDK_UNIT_TESTS "SDK: Enable unit test executables" ON)
option(VN_SDK_TESTS "SDK: Enable funcitonal tests" OFF)
option(VN_SDK_UTILITY "SDK: Include 'utility' libraries in SDK installations" ON)
option(VN_SDK_API_LAYER "SDK: Include API layer library in SDK installations" ON)
option(VN_SDK_DOCS "Build Doxygen and Sphinx documentation" OFF)
option(VN_SDK_USE_GHC_FILESYSTEM "Use ghc-filesystem instead of C++ standard <filesystem>" OFF)
option(VN_SDK_USE_SPAN_LITE "Use span-lite instead of C++ standard <span>" ON)

set(VN_SDK_FFMPEG_LIBS_PACKAGE "ffmpeg" CACHE STRING "SDK: Package to find for the 'libav...'' libraries used by samples and tests to decode base streams, or empty string for none")
if(VN_SDK_FFMPEG_LIBS_PACKAGE)
	set(VN_SDK_BASE_DECODER ON)
else()
	set(VN_SDK_BASE_DECODER OFF)
endif()

# Core
option(VN_CORE_TEST "Core: Build tests" OFF)
option(VN_CORE_THREADING "Core: Enable multithreading" ON)
option(VN_CORE_SIMD "Core: Enable SIMD for current architecture" ON)
option(VN_CORE_SSE "Core: Enable SSE for current architecture" ON)
option(VN_CORE_AVX2 "Core: Enable AVX2 for current architecture" ON)
option(VN_CORE_NEON "Core: Enable NEON for current architecture" ON)
option(VN_CORE_PROFILER "Core: Enable intrusive profiling" OFF)
option(VN_CORE_DEBUG_SYNTAX "Core: Enable debugging of bitstream syntax" OFF)
option(VN_CORE_FORCE_OVERLAY "Core: Enable forced overlay" OFF)
option(VN_CORE_TRACE_MEMORY "Core: Enable memory tracing" OFF)

# This is a kludge to enable general builds whilst the benchmark package is broken.
option(VN_CORE_BENCHMARK "Core: Build benchmark tool" OFF)

# Local modules - searched last
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("VNovaSetup")

##
#
project(LCEVC_DEC_SDK
		DESCRIPTION "The Open Source LCEVC Decoder SDK"
		LANGUAGES CXX C)

# Binary destination
set(MONOLITHIC_BUILD_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})

## Decoder components
#

# API Layer
if(VN_SDK_API_LAYER)
	lcevc_add_subdirectory(src/api)
	if(VN_SDK_BASE_DECODER)
		lcevc_add_subdirectory(src/utility)
		if(VN_SDK_TESTS)
			lcevc_add_subdirectory(src/utility/test)
		endif()
		lcevc_add_subdirectory(src/sample_c)
		lcevc_add_subdirectory(src/sample_cpp)
	endif()
	if(VN_SDK_TESTS)
		lcevc_add_subdirectory(src/api/test/harness)
		lcevc_add_subdirectory(src/api/test/unit)
	endif()
endif()

# Core layer
lcevc_add_subdirectory(src/core/decoder)
lcevc_add_subdirectory(src/core/sequencing)
lcevc_add_subdirectory(src/overlay_images)

if(VN_CORE_BENCHMARK)
	lcevc_add_subdirectory(src/core/test/benchmark)
endif()
if(VN_CORE_TEST)
	lcevc_add_subdirectory(src/core/test/harness)
	lcevc_add_subdirectory(src/core/test/decoder_unit)
endif()

if(VN_SDK_TESTS)
	lcevc_add_subdirectory(src/core/test/sequencing_unit)
	lcevc_add_subdirectory(src/integration/test/api)
	lcevc_add_subdirectory(src/func_tests)
endif()

if(VN_SDK_DOCS)
	# Build Doxygen->Breathe->Sphinx docs
	lcevc_add_subdirectory("docs/sphinx")
endif()

# Record configuration - header and README
configure_file("cmake/templates/lcevc_config.h.in" "generated/lcevc_config.h")
configure_file("cmake/templates/README.md.in" "install_config/README.md")

## Top level install
#
install(FILES "COPYING" DESTINATION "./")
install(FILES "${CMAKE_BINARY_DIR}/install_config/README.md" DESTINATION "./")
install(FILES "cmake/install/conanfile.txt" DESTINATION "./")
install(FILES "cmake/install/lcevc_dec.pc" DESTINATION "./pkgconfig/")
install(DIRECTORY "licences" DESTINATION "./")

#
install(FILES "include/LCEVC/lcevc.h" DESTINATION "./include/LCEVC")

## Install dependencies from conan
enable_testing()

## Package install into a ZIP file
#
set(CPACK_PACKAGE_VERSION ${GIT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
