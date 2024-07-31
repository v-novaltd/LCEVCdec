# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

cmake_minimum_required(VERSION 3.17.1 FATAL_ERROR)

# Configuration
#
# General
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(VN_MSVC_RUNTIME_STATIC "Use static multi-threaded runtime for MSVC" ON)
option(VN_STRIP_RELEASE_BUILDS "Compile Release builds without debug symbols." ON)

# SDK
option(VN_SDK_EXECUTABLES "SDK: Include samples and test harness executables in SDK installations"
       ON)
option(VN_SDK_SAMPLE_SOURCE "SDK: Include sample sources in SDK installations" ON)
option(VN_SDK_UNIT_TESTS "SDK: Enable unit test executables" ON)
option(VN_SDK_API_LAYER "SDK: Include API layer library in SDK installations" ON)
option(VN_SDK_JSON_CONFIG "Allow decoder to be configured with JSON, requires rapidjson" ON)
option(VN_SDK_DOCS "Build Doxygen and Sphinx documentation" OFF)
option(VN_SDK_USE_GHC_FILESYSTEM "Use ghc-filesystem instead of C++ standard <filesystem>" OFF)
option(VN_SDK_COVERAGE "Generate gcov statistics for coverage" OFF)

set(VN_SDK_FFMPEG_LIBS_PACKAGE
    "pkgconfig"
    CACHE
        STRING
        "SDK: Package to find for the 'ffmpeg...'' libraries used by samples and tests to decode base streams, or empty string for none"
)
# Unfortunately, some parsers read VAR="" as setting the variable to the 2-char string, "\"\"",
# hence this awkward check.
if(VN_SDK_FFMPEG_LIBS_PACKAGE AND NOT (VN_SDK_FFMPEG_LIBS_PACKAGE STREQUAL "\"\""))
    message(STATUS "Using ${VN_SDK_FFMPEG_LIBS_PACKAGE} base decoder for test harnesses")
    set(VN_SDK_BASE_DECODER ON)
else()
    set(VN_SDK_BASE_DECODER OFF)
endif()

if(VN_SDK_EXECUTABLES AND NOT VN_SDK_JSON_CONFIG)
    message(FATAL_ERROR "JSON configuration option is required for executables")
endif()

# Core
option(VN_CORE_THREADING "Core: Enable multithreading" ON)
option(VN_CORE_SIMD "Core: Enable SIMD for current architecture" ON)
option(VN_CORE_SSE "Core: Enable SSE for current architecture" ON)
option(VN_CORE_AVX2 "Core: Enable AVX2 for current architecture" ON)
option(VN_CORE_NEON "Core: Enable NEON for current architecture" ON)
option(VN_CORE_PROFILER "Core: Enable intrusive profiling" OFF)
option(VN_CORE_STATS "Core: Enable intrusive statistics - required for performance tests" OFF)
option(VN_CORE_DEBUG_SYNTAX "Core: Enable debugging of bitstream syntax" OFF)
option(
    VN_CORE_GENERATE_PGO
    "Core: Make an instrumented build (i.e. a profile-generating build), for profile-guided optimization (PGO)."
    OFF)
option(
    VN_CORE_USE_PGO
    "Core: Use previously-generated PGO profiles (from past runs of an instrumented build) to optimize this build."
    OFF)
option(VN_CORE_FORCE_OVERLAY "Core: Enable forced overlay" OFF)
option(VN_CORE_TRACE_MEMORY "Core: Enable memory tracing" OFF)
option(VN_CORE_BENCHMARK "Core: Build benchmark tool" OFF)

# Local modules - searched last
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("VNovaSetup")

#
project(
    LCEVCdec_SDK
    DESCRIPTION "The Open Source LCEVC Decoder SDK"
    LANGUAGES CXX C)

# Binary destination
set(MONOLITHIC_BUILD_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${MONOLITHIC_BUILD_OUTPUT_DIR})

# Decoder components
#

# API Layer
if(VN_SDK_API_LAYER)
    lcevc_add_subdirectory(src/api)
    lcevc_add_subdirectory(src/utility)
    lcevc_add_subdirectory(src/enhancement_cpu)
    if(VN_SDK_BASE_DECODER)
        if(VN_SDK_UNIT_TESTS)
            lcevc_add_subdirectory(src/utility/test/unit)
        endif()
        if(VN_SDK_EXECUTABLES)
            lcevc_add_subdirectory(src/sample_cpp)
            lcevc_add_subdirectory(src/api/test/harness)
        endif()
    endif()
    if(VN_SDK_UNIT_TESTS)
        lcevc_add_subdirectory(src/api/test/unit)
    endif()
endif()

# Core layer
lcevc_add_subdirectory(src/core/decoder)
lcevc_add_subdirectory(src/core/sequencing)
lcevc_add_subdirectory(src/overlay_images)
if(VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory(src/core/test/decoder_unit)
    lcevc_add_subdirectory(src/core/test/sequencing_unit)
    lcevc_add_subdirectory(src/utility/test/utilities)
endif()
if(VN_CORE_BENCHMARK)
    lcevc_add_subdirectory(src/core/test/benchmark)
endif()

if(VN_SDK_DOCS)
    lcevc_add_subdirectory(docs/sphinx)
endif()

# Define the variabels to install files based on the GNU conventions
include(GNUInstallDirs)

# Record configuration - header and README
configure_file("cmake/templates/lcevc_config.h.in" "generated/lcevc_config.h")
configure_file("cmake/templates/README.md.in" "install_config/README.md")

configure_file("cmake/templates/lcevc_dec.pc.in" "generated/lcevc_dec.pc" @ONLY)

# Top level install
#
install(FILES "COPYING" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "${CMAKE_BINARY_DIR}/install_config/README.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "cmake/install/conanfile.txt" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "${CMAKE_BINARY_DIR}/generated/lcevc_dec.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
install(DIRECTORY "licenses" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "LICENSE.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses/LCEVCdec")

#
install(FILES "include/LCEVC/lcevc.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC")

#
if(VN_SDK_EXECUTABLES)
    if(WIN32)
        install(
            DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
            OPTIONAL
            DESTINATION "bin"
            FILES_MATCHING
            PATTERN "*.dll")
    else()
        install(
            DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
            OPTIONAL
            DESTINATION "lib"
            FILES_MATCHING
            PATTERN "*")
    endif()
endif()

# Install dependencies from conan
enable_testing()

# Package install into a ZIP file
#
set(CPACK_PACKAGE_VERSION ${GIT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
