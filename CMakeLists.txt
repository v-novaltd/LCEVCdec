# Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

# Compiler configuration
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(VN_MSVC_RUNTIME_STATIC "Use static multi-threaded runtime for MSVC" ON)
option(VN_STRIP_RELEASE "Strip any debug information from release binaries" ON)
option(VN_SDK_SIMD "Enable SIMD for current architecture (SSE + AVX2 or NEON)" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("CMakeSetup")

project(
    LCEVCdec_SDK
    DESCRIPTION "The Open Source LCEVC Decoder SDK"
    VERSION 4.0.1
    LANGUAGES CXX C)

# Build configuration
option(VN_SDK_EXECUTABLES "Include samples and test harness executables in SDK installations" OFF)
option(VN_SDK_SAMPLE_SOURCE "Include sample sources in SDK installations" ON)
option(VN_SDK_UNIT_TESTS "Enable unit test executables" OFF)
option(VN_SDK_API_LAYER "Include API layer library in SDK installations" ON)
option(VN_SDK_PIPELINE_CPU "Enable the CPU pipeline" ON)
option(VN_SDK_PIPELINE_LEGACY "Enable the legacy pipeline" ON)
option(VN_SDK_PIPELINE_VULKAN "Enable the Vulkan pipeline" OFF)
option(VN_SDK_JSON_CONFIG "Allow decoder to be configured with JSON, requires nlohmann-json" OFF)
option(VN_SDK_DOCS "Build Doxygen and Sphinx documentation" OFF)
option(VN_SDK_COVERAGE "Generate gcov statistics for coverage" OFF)
option(VN_SDK_TRACING
       "Configurable tracing information recorder for function entry/exit in perfetto format" ON)
option(VN_SDK_METRICS "Configurable metrics recording" ON)
option(VN_SDK_DIAGNOSTICS_ASYNC "Enable asynchronous output of logging, tracing and metrics" ON)
set(VN_SDK_MAXIMUM_LOG_LEVEL
    "VERBOSE"
    CACHE STRING "Disable log messages above given level")
option(VN_SDK_WARNINGS_FAIL "Compiler warnings will cause a build to fail" OFF)
option(VN_SDK_BUILD_DETAILS "Include build time and origin in libraries - non-reproducible build"
       OFF)
option(VN_SDK_SYSTEM_INSTALL "Install licences and docs to the system or to a package" ON)

# cmake-format: off
set(VN_SDK_FFMPEG_LIBS_PACKAGE "" CACHE STRING
        "Define a path to search for libav packages, conan or pkg-config will be used by default if not specified")
# cmake-format: on

if (VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS)
    set(VN_SDK_BASE_DECODER ON)
else ()
    set(VN_SDK_BASE_DECODER OFF)
endif ()

if ((VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS) AND NOT VN_SDK_JSON_CONFIG)
    message(STATUS "Executables or tests enabled, enabling required JSON config")
    set(VN_SDK_JSON_CONFIG ON)
endif ()

if ((VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS) AND NOT VN_SDK_API_LAYER)
    message(FATAL_ERROR "API layer required when building executables or unit tests")
endif ()

if (EMSCRIPTEN)
    message(STATUS "Detected emscripten build, configuring appropriately")
    set(BUILD_SHARED_LIBS OFF)
    set(VN_SDK_API_LAYER OFF)
    set(VN_SDK_PIPELINE_CPU OFF)
    set(VN_SDK_PIPELINE_LEGACY OFF)
endif ()

if (VN_SDK_PIPELINE_VULKAN
    AND ((CMAKE_SYSTEM_NAME MATCHES "^(Darwin|iOS|macOS|tvOS|watchOS|Emscripten)$")
         OR (CMAKE_SYSTEM_NAME MATCHES "Android" AND ANDROID_PLATFORM LESS 28)))
    message(
        FATAL_ERROR
            "Target build platform doesn't support Vulkan, please disable VN_SDK_PIPELINE_VULKAN")
endif ()

# Performance options
option(VN_SDK_THREADING "Enable multithreading" ON)
option(VN_SDK_THREADS_CUSTOM "Use a custom threading implementation." OFF)
option(VN_SDK_GENERATE_PGO "Make an instrumented build for profile-guided optimization (PGO)." OFF)
option(VN_SDK_USE_PGO "Use previously-generated PGO profiles to optimize this build." OFF)
option(VN_SDK_BENCHMARK "Build benchmark google benchmark tests" OFF)
option(VN_SDK_FORCE_OVERLAY "Enable forced overlay" OFF)

# Pipeline Versions
set(PIPELINE_CPU_VERSION 1)
set(PIPELINE_VULKAN_VERSION 1)
set(PIPELINE_LEGACY_VERSION 1)

# Decoder components

# Common
lcevc_add_subdirectory(src/common)
lcevc_add_subdirectory_if(src/common/test/unit VN_SDK_UNIT_TESTS)

# Enhancement
lcevc_add_subdirectory(src/enhancement)
lcevc_add_subdirectory_if(src/enhancement/test/unit VN_SDK_UNIT_TESTS)
lcevc_add_subdirectory_if(src/enhancement/test/sample VN_SDK_EXECUTABLES)

# Pipeline
lcevc_add_subdirectory(src/pipeline)
lcevc_add_subdirectory_if(src/pipeline/test/unit VN_SDK_UNIT_TESTS)

# Pixel processing
lcevc_add_subdirectory(src/pixel_processing)
lcevc_add_subdirectory_if(src/pixel_processing/test/unit VN_SDK_UNIT_TESTS)

# LCEVC NALU Extract
lcevc_add_subdirectory(src/extract)

# API Layer
if (VN_SDK_API_LAYER)
    lcevc_add_subdirectory(src/api)
    lcevc_add_subdirectory(src/api_utility)
    lcevc_add_subdirectory_if(src/api/test/unit VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory_if(src/sample_cpp VN_SDK_EXECUTABLES)
    lcevc_add_subdirectory_if(src/api/test/harness VN_SDK_EXECUTABLES)
endif ()

# Pipelines
if (VN_SDK_PIPELINE_CPU)
    lcevc_add_subdirectory(src/pipeline_cpu)
    lcevc_add_subdirectory_if(src/pipeline_cpu/test/unit VN_SDK_UNIT_TESTS)
endif ()

if (VN_SDK_PIPELINE_LEGACY)
    lcevc_add_subdirectory(src/legacy/decoder)
    lcevc_add_subdirectory(src/legacy/sequencer)
    lcevc_add_subdirectory(src/overlay_images)
    lcevc_add_subdirectory_if(src/legacy/test/decoder_unit VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory_if(src/legacy/test/sequencer_unit VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory_if(src/legacy/test/sequencing_unit VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory_if(src/legacy/test/benchmark VN_SDK_BENCHMARK)

    if (VN_SDK_API_LAYER)
        lcevc_add_subdirectory(src/color_conversion)

        lcevc_add_subdirectory(src/pipeline_legacy)
        lcevc_add_subdirectory_if(src/pipeline_legacy/test/unit VN_SDK_UNIT_TESTS)
    endif ()
endif ()

if (VN_SDK_PIPELINE_VULKAN)
    lcevc_add_subdirectory(src/pipeline_vulkan)
    lcevc_add_subdirectory_if(src/pipeline_vulkan/test/unit VN_SDK_UNIT_TESTS)
endif ()

if (EMSCRIPTEN)
    lcevc_add_subdirectory(src/emscripten)
endif ()

# Utility
if (VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory(src/utility)
    lcevc_add_subdirectory_if(src/utility/test/unit VN_SDK_UNIT_TESTS)
    lcevc_add_subdirectory_if(src/utility/test/utilities VN_SDK_UNIT_TESTS)
endif ()

lcevc_add_subdirectory_if(docs/sphinx VN_SDK_DOCS)

# Define the variables to install files based on the GNU conventions
include(GNUInstallDirs)

# Install project specific files and templates
include("CMakeInstall")
