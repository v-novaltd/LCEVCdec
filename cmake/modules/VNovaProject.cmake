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

# Target specific configuration - included automatically after project()
#

# Architecture
#
# Make a consistent description of target architecture
#
if (NOT DEFINED TARGET_ARCH)
    if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(TARGET_ARCH "wasm")
    elseif (
        CMAKE_SYSTEM_PROCESSOR STREQUAL "i686"
        OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i386"
        OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86"
        OR CMAKE_SYSTEM_PROCESSOR STREQUAL "X86")
        set(TARGET_ARCH "x86")
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(TARGET_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^armv7")
        set(TARGET_ARCH "armv7")
    elseif (
        CMAKE_SYSTEM_PROCESSOR MATCHES "^armv8"
        OR CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64"
        OR CMAKE_SYSTEM_PROCESSOR MATCHES "^arm64")
        set(TARGET_ARCH "armv8")
    else ()
        message(WARNING "Unknown target processor: [${CMAKE_SYSTEM_PROCESSOR}],
                         this is an untested and unsupported target")
    endif ()
endif ()

# Platform
#
add_library(lcevc_dec::platform INTERFACE IMPORTED)

if (NOT DEFINED TARGET_PLATFORM)
    if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(TARGET_PLATFORM "Emscripten")
    elseif (WIN32)
        set(TARGET_PLATFORM "Windows")
    elseif (ANDROID)
        set(TARGET_PLATFORM "Android")
    elseif (IOS)
        set(TARGET_PLATFORM "iOS")
    elseif (TVOS)
        set(TARGET_PLATFORM "tvOS")
    elseif (APPLE)
        set(TARGET_PLATFORM "macOS")
    elseif (UNIX)
        set(TARGET_PLATFORM "Linux")
    else ()
        message(FATAL_ERROR "Unknown target platform")
    endif ()
endif ()

# Compiler
#
add_library(lcevc_dec::compiler INTERFACE IMPORTED)

target_include_directories(lcevc_dec::compiler INTERFACE ${CMAKE_BINARY_DIR}/generated)
target_include_directories(lcevc_dec::compiler INTERFACE "${CMAKE_SOURCE_DIR}/include")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT DEFINED TARGET_COMPILER)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(TARGET_COMPILER "MSVC")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(TARGET_COMPILER "GNU")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        set(TARGET_COMPILER "Intel")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        set(TARGET_COMPILER "ClangApple")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            set(TARGET_COMPILER "ClangMSVC")
        elseif (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
            set(TARGET_COMPILER "ClangGNU")
        endif ()
    else ()
        message(
            FATAL_ERROR
                "Unknown target compiler: ${CMAKE_CXX_COMPILER_ID}  (${CMAKE_CXX_COMPILER_FRONTEND_VARIANT})"
        )
    endif ()
endif ()

message(
    STATUS "Target: Platform=${TARGET_PLATFORM} Arch=${TARGET_ARCH} Compiler=${TARGET_COMPILER}")

#
include("Arch/${TARGET_ARCH}" OPTIONAL)
include("Platform/${TARGET_PLATFORM}")
include("Compiler/${TARGET_COMPILER}")
