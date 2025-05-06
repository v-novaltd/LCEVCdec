# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
# software may be incorporated into a project under a compatible license provided the requirements
# of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
# licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
# ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
# THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(VN_MSVC_RUNTIME_STATIC "Use static multi-threaded runtime for MSVC" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("VNovaSetup")

project(
    LCEVCdec_SDK
    DESCRIPTION "The Open Source LCEVC Decoder SDK"
    LANGUAGES CXX C)

# Configuration
option(VN_SDK_EXECUTABLES "Include samples and test harness executables in SDK installations" OFF)
option(VN_SDK_SAMPLE_SOURCE "Include sample sources in SDK installations" ON)
option(VN_SDK_UNIT_TESTS "Enable unit test executables" OFF)
option(VN_SDK_API_LAYER "Include API layer library in SDK installations" ON)
option(VN_SDK_JSON_CONFIG "Allow decoder to be configured with JSON, requires nlohmann-json" OFF)
option(VN_SDK_DOCS "Build Doxygen and Sphinx documentation" OFF)
option(VN_SDK_COVERAGE "Generate gcov statistics for coverage" OFF)
option(VN_SDK_WARNINGS_FAIL "Compiler warnings will cause a build to fail" OFF)
option(VN_SDK_BUILD_DETAILS "Include build time and origin in libraries - non-reproducible build"
       OFF)

# cmake-format: off
set(VN_SDK_FFMPEG_LIBS_PACKAGE "" CACHE STRING
        "Define a path to search for libav packages, conan or pkg-config will be used by default \
        if not specified")
# cmake-format: on

if (VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS)
    set(VN_SDK_BASE_DECODER ON)
else ()
    set(VN_SDK_BASE_DECODER OFF)
endif ()

if (VN_SDK_EXECUTABLES AND NOT VN_SDK_JSON_CONFIG)
    message(STATUS "Executables enabled, enabling required JSON config")
    set(VN_SDK_JSON_CONFIG ON)
endif ()

# Core
option(VN_CORE_THREADING "Core: Enable multithreading" ON)
option(VN_CORE_SIMD "Core: Enable SIMD for current architecture" ON)
option(VN_CORE_SSE "Core: Enable SSE for current architecture" ON)
option(VN_CORE_AVX2 "Core: Enable AVX2 for current architecture" ON)
option(VN_CORE_NEON "Core: Enable NEON for current architecture" ON)
option(
    VN_CORE_GENERATE_PGO
    "Core: Make an instrumented build (i.e. a profile-generating build), for profile-guided optimization (PGO)."
    OFF)
option(
    VN_CORE_USE_PGO
    "Core: Use previously-generated PGO profiles (from past runs of an instrumented build) to optimize this build."
    OFF)
option(VN_CORE_FORCE_OVERLAY "Core: Enable forced overlay" OFF)
option(VN_CORE_BENCHMARK "Core: Build benchmark tool" OFF)

# Binary destination
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)

# Decoder components
#

# API Layer
if (VN_SDK_API_LAYER)
    lcevc_add_subdirectory(src/api)
    lcevc_add_subdirectory(src/api_utility)
    lcevc_add_subdirectory(src/enhancement_cpu)
    if (VN_SDK_EXECUTABLES)
        lcevc_add_subdirectory(src/sample_cpp)
        lcevc_add_subdirectory(src/api/test/harness)
    endif ()
    if (VN_SDK_UNIT_TESTS)
        lcevc_add_subdirectory(src/api/test/unit)
    endif ()
endif ()

if (VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory(src/utility)
endif ()

# Core layer
lcevc_add_subdirectory(src/core/decoder)
lcevc_add_subdirectory(src/core/sequencing)
lcevc_add_subdirectory(src/overlay_images)
if (VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory(src/utility/test/unit)
    lcevc_add_subdirectory(src/utility/test/utilities)
    lcevc_add_subdirectory(src/core/test/decoder_unit)
    lcevc_add_subdirectory(src/core/test/sequencing_unit)
endif ()
if (VN_CORE_BENCHMARK)
    lcevc_add_subdirectory(src/core/test/benchmark)
endif ()

if (VN_SDK_DOCS)
    lcevc_add_subdirectory(docs/sphinx)
endif ()

# Define the variables to install files based on the GNU conventions
include(GNUInstallDirs)

# Record configuration - header and README
configure_file("cmake/templates/lcevc_config.h.in" "generated/lcevc_config.h")
configure_file("cmake/templates/README.md.in" "install_config/README.md")
configure_file("cmake/templates/lcevc_dec.pc.in" "generated/lcevc_dec.pc" @ONLY)

# Top level install
#
install(FILES "COPYING" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")
install(FILES "${CMAKE_BINARY_DIR}/install_config/README.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "${CMAKE_BINARY_DIR}/generated/lcevc_dec.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
install(FILES "LICENSE.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")

#
if (NOT CMAKE_INSTALL_INCLUDEDIR)
    set(CMAKE_INSTALL_INCLUDEDIR include)
endif ()
install(FILES "${CMAKE_BINARY_DIR}/generated/lcevc_config.h" DESTINATION "include")
install(FILES "include/LCEVC/lcevc.h" DESTINATION "include/LCEVC")

#
if (VN_SDK_EXECUTABLES)
    if (WIN32)
        install(
            DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
            OPTIONAL
            DESTINATION "bin"
            FILES_MATCHING
            PATTERN "liblcevc*.dll")
    else ()
        install(
            DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
            OPTIONAL
            DESTINATION "lib"
            FILES_MATCHING
            PATTERN "liblcevc*")
    endif ()
endif ()

# Install dependencies from conan
enable_testing()

# Package install into a ZIP file
#
set(CPACK_PACKAGE_VERSION ${GIT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
