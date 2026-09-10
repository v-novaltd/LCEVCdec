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

include(ExternalProject)

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

set(LCEVCDEC_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS=OFF
    -DANDROID_ABI=${ANDROID_ABI}
    -DANDROID_NDK=${ANDROID_NDK}
    -DANDROID_PLATFORM=${ANDROID_PLATFORM}
    -DVN_SDK_PIPELINE_VULKAN=ON
    -DVN_SDK_PIPELINE_CPU=ON
    -DVN_SDK_SIMD=${ANDROID_ARM_NEON}
    -DVN_SDK_MINIMUM_LIB_SIZE=OFF)

# VN_SDK_MINIMUM_LIB_SIZE can be enabled to reduce library size at the expense of logs, consider
# reducing built ABIs to further reduce size.

set(LCEVCDEC_MODULES
    api
    api_utility
    extract
    pipeline_cpu
    pipeline_vulkan
    pipeline
    enhancement
    pixel_processing
    common)

set(LCEVCDEC_LIB_DIR "${LCEVCDEC_BUILD_DIR}/lib")

set(LCEVCDEC_STATIC_BYPRODUCTS)
foreach (MODULE IN LISTS LCEVCDEC_MODULES)
    set(MODULE_LIB_PATH "${LCEVCDEC_LIB_DIR}/liblcevc_dec_${MODULE}.a")
    list(APPEND LCEVCDEC_STATIC_BYPRODUCTS "${MODULE_LIB_PATH}")
endforeach ()

ExternalProject_Add(
    LCEVCdec_build
    SOURCE_DIR ${PROJECT_ROOT}
    BINARY_DIR ${LCEVCDEC_BUILD_DIR}
    CMAKE_GENERATOR ${CMAKE_GENERATOR}
    CMAKE_ARGS ${LCEVCDEC_CMAKE_ARGS}
    BUILD_BYPRODUCTS ${LCEVCDEC_STATIC_BYPRODUCTS}
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
    INSTALL_COMMAND "")

# ExternalProject's configure step is normally stamped as complete forever. Re-run it before each
# incremental build so changes to LCEVCDEC_CMAKE_ARGS (such as enabling another pipeline) are
# applied to existing Android Studio build trees.
ExternalProject_Add_Step(
    LCEVCdec_build reconfigure
    COMMAND ${CMAKE_COMMAND} -S ${PROJECT_ROOT} -B <BINARY_DIR> -G ${CMAKE_GENERATOR}
            ${LCEVCDEC_CMAKE_ARGS}
    DEPENDEES configure
    DEPENDERS build
    ALWAYS TRUE)

set(LCEVCDEC_LIBS)
foreach (MODULE IN LISTS LCEVCDEC_MODULES)
    set(MODULE_TARGET "lcevc_dec_${MODULE}")
    set(MODULE_LIB_PATH "${LCEVCDEC_LIB_DIR}/liblcevc_dec_${MODULE}.a")

    add_library(${MODULE_TARGET} STATIC IMPORTED)
    set_target_properties(${MODULE_TARGET} PROPERTIES IMPORTED_LOCATION "${MODULE_LIB_PATH}")
    add_dependencies(${MODULE_TARGET} LCEVCdec_build)
    list(APPEND LCEVCDEC_LIBS "${MODULE_TARGET}")
endforeach ()

# ~~~
# Dec JNI library
# ~~~

add_library(lcevc_dec_jni SHARED lcevc_dec_jni.cpp surface_presenter.cpp)

target_link_libraries(lcevc_dec_jni PRIVATE ${LCEVCDEC_LIBS} ${PLATFORM_LIBRARIES})

# Make NDK symbols newer than the target API weak imports (SurfaceControl and
# AHardwareBuffer_lockPlanes are API 29) - call sites gate on the device API level.
target_compile_definitions(lcevc_dec_jni PRIVATE __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)
target_include_directories(
    lcevc_dec_jni PRIVATE ${PROJECT_ROOT}/include ${PROJECT_ROOT}/src/api/include
                          ${PROJECT_ROOT}/src/extract/include)
add_dependencies(lcevc_dec_jni LCEVCdec_build)

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
