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

project(LCEVCdec_SDK_API_Sample)

list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)

add_library(ffmpeg-libs::ffmpeg-libs INTERFACE IMPORTED)
list(APPEND REQUIRED_LIBAV_LIBS "libavcodec" "libavformat" "libavfilter" "libavutil")
find_package("ffmpeg" QUIET)
get_cmake_property(PACKAGE_LIST PACKAGES_FOUND)
if ("ffmpeg" IN_LIST PACKAGE_LIST)
    message(STATUS "Found libav packages from conan package 'ffmpeg'")
    target_link_libraries(ffmpeg-libs::ffmpeg-libs INTERFACE ffmpeg::ffmpeg)
else ()
    message(STATUS "Found libav and vulkan via pkg-config")
    find_package(PkgConfig)
    foreach (_LIB ${REQUIRED_LIBAV_LIBS})
        pkg_check_modules(${_LIB} IMPORTED_TARGET ${_LIB})
    endforeach ()
    target_link_libraries(
        ffmpeg-libs::ffmpeg-libs INTERFACE PkgConfig::libavcodec PkgConfig::libavformat
                                           PkgConfig::libavutil PkgConfig::libavfilter)
    pkg_get_variable(VULKAN_PIPELINE lcevc_dec vulkan_pipeline)
endif ()
if (VULKAN_PIPELINE)
    find_package(Vulkan REQUIRED)
endif ()

find_package(lcevc_dec REQUIRED)

add_executable(lcevc_dec_api_sample_cpp ../src/main.cpp)

target_link_libraries(lcevc_dec_api_sample_cpp fmt::fmt CLI11::CLI11 lcevc_dec::lcevc_dec
                      ffmpeg-libs::ffmpeg-libs $<TARGET_NAME_IF_EXISTS:Vulkan::Vulkan>)
