# Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

# ~~~
# Root project config
# ~~~

# NDK r27 bundled CMake version is 3.22.1.
cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

project(LCEVCdec_JNI)

# C++17 is the recommended standard since NDK r23. NDK r27 fully supports C++20 but we will stick to
# C++17 until there is a reason not to.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VN_SDK_TRACING_SYSTEM_SINK "Build tracing support, including Android system Perfetto sink"
       OFF)

# Path to where the core dec library can be included from
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..)

# ~~~
# Platform libs & linker
# ~~~

set(PLATFORM_LIBRARIES)
set(PLATFORM_LIBRARIES_MEDIA)

find_library(PLATFORM_ANDROID_LIBRARY NAMES android)
find_library(PLATFORM_LOG_LIBRARY NAMES log)
find_library(PLATFORM_VULKAN_LIBRARY NAMES vulkan)
find_library(PLATFORM_NATIVE_WINDOW_LIBRARY NAMES nativewindow)
find_library(PLATFORM_Z_LIBRARY NAMES z)

list(APPEND PLATFORM_LIBRARIES ${PLATFORM_ANDROID_LIBRARY} ${PLATFORM_LOG_LIBRARY}
     ${PLATFORM_VULKAN_LIBRARY} ${PLATFORM_Z_LIBRARY})
list(APPEND PLATFORM_LIBRARIES_MEDIA ${PLATFORM_NATIVE_WINDOW_LIBRARY})

if (NOT ANDROID_VENDOR)
    find_library(PLATFORM_MEDIANDK_LIBRARY NAMES mediandk)
    list(APPEND PLATFORM_LIBRARIES_MEDIA ${PLATFORM_MEDIANDK_LIBRARY})
endif ()

message(STATUS "Platform libraries: ${PLATFORM_LIBRARIES}")
message(STATUS "Platform media libraries: ${PLATFORM_LIBRARIES_MEDIA}")

# ~~~
# Core Dec library
# ~~~

message(
    STATUS
        "Building LCEVCdec CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}, ANDROID_ABI=${ANDROID_ABI}, ANDROID_PLATFORM=${ANDROID_PLATFORM}"
)
set(LCEVCDEC_BUILD_DIR "${CMAKE_BINARY_DIR}/lcevcdec-${ANDROID_ABI}-${CMAKE_BUILD_TYPE}")

if (CONAN_DEPS_ROOT)
    set(CONAN_DEPS_DIR "${CONAN_DEPS_ROOT}/${CMAKE_BUILD_TYPE}/${ANDROID_ABI}")
    set(VN_SDK_CONAN_DEPS_DIR
        "${CONAN_DEPS_DIR}"
        CACHE PATH "" FORCE)
endif ()

if (VN_SDK_TRACING_SYSTEM_SINK)
    if (NOT CONAN_DEPS_DIR)
        message(FATAL_ERROR "The system tracing sink requires -PCONAN_ENABLED=TRUE")
    endif ()
    set(fmt_DIR "${CONAN_DEPS_DIR}")
    set(perfetto_DIR "${CONAN_DEPS_DIR}")
endif ()

# VN_SDK_MINIMUM_LIB_SIZE can be enabled to reduce library size at the expense of logs, consider
# reducing built ABIs to further reduce size.

set(BUILD_SHARED_LIBS OFF)
set(VN_SDK_PIPELINE_VULKAN ON)
set(VN_SDK_PIPELINE_CPU ON)
set(VN_SDK_SIMD ${ANDROID_ARM_NEON})
set(VN_SDK_TRACING ${VN_SDK_TRACING_SYSTEM_SINK})
set(VN_SDK_TRACING_SYSTEM_SINK ${VN_SDK_TRACING_SYSTEM_SINK})
set(VN_SDK_MINIMUM_LIB_SIZE OFF)

add_subdirectory(${PROJECT_ROOT} ${LCEVCDEC_BUILD_DIR} EXCLUDE_FROM_ALL)

# ~~~
# Dec JNI library
# ~~~

add_library(lcevc_dec_jni SHARED lcevc_dec_jni.cpp surface_presenter.cpp)
target_link_libraries(lcevc_dec_jni PRIVATE lcevc_dec::api lcevc_dec::extract lcevc_dec::common
                                            ${PLATFORM_LIBRARIES})

if (VN_SDK_TRACING_SYSTEM_SINK)
    target_link_libraries(lcevc_dec_jni PRIVATE lcevc_dec::utility)
    target_compile_definitions(lcevc_dec_jni PRIVATE VN_SDK_TRACING_SYSTEM_SINK=1)
endif ()

# Make NDK symbols newer than the target API weak imports (SurfaceControl and
# AHardwareBuffer_lockPlanes are API 29) - call sites gate on the device API level.
target_compile_definitions(lcevc_dec_jni PRIVATE __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)

# Library size reduction flags and stubs
target_compile_options(
    lcevc_dec_jni
    PRIVATE -Oz
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            -ffunction-sections
            -fdata-sections
            -fno-exceptions
            -fno-rtti)
target_link_options(lcevc_dec_jni PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,--icf=all
                    -Wl,--no-eh-frame-hdr)
target_sources(lcevc_dec_jni PRIVATE ${PROJECT_ROOT}/cmake/sources/minimal_runtime_stubs.cpp)
