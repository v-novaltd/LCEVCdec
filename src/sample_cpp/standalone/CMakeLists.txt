# Copyright (c) V-Nova International Limited 2023. All rights reserved.
#
cmake_minimum_required(VERSION 3.17.1 FATAL_ERROR)

project(LCEVC_DEC_SDK_API_Sample_Project)

list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)
find_package(libav REQUIRED)
find_package(lcevc_dec REQUIRED)

add_executable(lcevc_dec_api_sample_cpp main.cpp)

target_link_libraries(lcevc_dec_api_sample_cpp
	fmt::fmt
	CLI11::CLI11
	libav::libav
	lcevc_dec::lcevc_dec
	)
