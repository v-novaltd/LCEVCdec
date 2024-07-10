# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

project(LCEVC_DEC_SDK_API_Sample_Project)

list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)
if(${VN_SDK_FFMPEG_LIBS_PACKAGE})
    find_package(${VN_SDK_FFMPEG_LIBS_PACKAGE} REQUIRED)
endif()
find_package(lcevc_dec REQUIRED)

add_executable(lcevc_dec_api_sample_cpp main.cpp)

target_link_libraries(
    lcevc_dec_api_sample_cpp fmt::fmt CLI11::CLI11
    $<TARGET_NAME_IF_EXISTS:${VN_SDK_FFMPEG_LIBS_PACKAGE}::${VN_SDK_FFMPEG_LIBS_PACKAGE}>
    lcevc_dec::lcevc_dec)
