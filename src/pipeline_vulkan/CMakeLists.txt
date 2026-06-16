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

include("Sources.cmake")

find_package(VulkanShim REQUIRED)

if (VN_SDK_MINIMUM_LIB_SIZE)
    find_package(ZLIB REQUIRED)
endif ()

# Compile shaders
find_package(Python3 REQUIRED)
set(SHADER_SCRIPT_PATH ${CMAKE_CURRENT_LIST_DIR}/src/shaders/compile_shaders.py)
set(SHADER_SCRIPT_DIR ${CMAKE_CURRENT_LIST_DIR}/src/shaders)

set(SHADER_INPUT_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/apply.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/add.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/common.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_common.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_in.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_p010.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_nv12.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba8.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba10.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/render_fragment.frag
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/render_vertex.vert
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_constants.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_horizontal.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_vertical.comp)

set(SHADER_OUTPUT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/apply.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/add.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_in.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_p010.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_nv12.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba8.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba10.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_fragment.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_vertex.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_horizontal.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_vertical.h)

set(SHADER_COMPILE_ARGS --cmakebuildinfodir ${CMAKE_CURRENT_BINARY_DIR}/../..)
if (VN_SDK_MINIMUM_LIB_SIZE)
    list(APPEND SHADER_COMPILE_ARGS --optimize)
endif ()

add_custom_command(
    OUTPUT ${SHADER_OUTPUT_FILES}
    COMMAND ${Python3_EXECUTABLE} ${SHADER_SCRIPT_PATH} ${SHADER_COMPILE_ARGS}
    WORKING_DIRECTORY ${SHADER_SCRIPT_DIR}
    DEPENDS ${SHADER_INPUT_FILES} ${SHADER_SCRIPT_PATH}
    COMMENT "Running Python script to compile GLSL shaders"
    VERBATIM)

add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUT_FILES})

set(LINK_LIBRARIES
    lcevc_dec::platform
    lcevc_dec::compiler
    lcevc_dec::common
    lcevc_dec::pipeline
    lcevc_dec::enhancement
    lcevc_dec::pixel_processing
    VulkanShim::VulkanShim)

if (VN_SDK_MINIMUM_LIB_SIZE)
    list(APPEND LINK_LIBRARIES ZLIB::ZLIB)
endif ()

if (VN_SDK_EXECUTABLES AND NOT ANDROID)
    find_package(glfw3 REQUIRED)
endif ()

# Explicit static library for vulkan unit tests
if (VN_SDK_UNIT_TESTS)
    add_library(lcevc_dec_pipeline_vulkan_static STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
    add_dependencies(lcevc_dec_pipeline_vulkan_static compile_shaders)

    lcevc_set_properties(lcevc_dec_pipeline_vulkan_static)

    target_compile_definitions(lcevc_dec_pipeline_vulkan_static PRIVATE VNEnablePipelineAPIExport)
    if (VN_SDK_EXECUTABLES AND NOT ANDROID)
        target_compile_definitions(lcevc_dec_pipeline_vulkan_static PRIVATE GLFW_STATIC)
        target_compile_definitions(lcevc_dec_pipeline_vulkan_static PRIVATE VN_SDK_EXECUTABLES)
        target_link_libraries(lcevc_dec_pipeline_vulkan_static PRIVATE glfw)
    endif ()

    target_include_directories(
        lcevc_dec_pipeline_vulkan_static
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
        PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/src/hpp"
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src/vma")

    target_link_libraries(lcevc_dec_pipeline_vulkan_static PUBLIC ${LINK_LIBRARIES})

    add_library(lcevc_dec::pipeline_vulkan_static ALIAS lcevc_dec_pipeline_vulkan_static)
endif ()

# Shared build target
add_library(lcevc_dec_pipeline_vulkan ${SOURCES} ${HEADERS} ${INTERFACES})
add_dependencies(lcevc_dec_pipeline_vulkan compile_shaders)

lcevc_set_properties(lcevc_dec_pipeline_vulkan)
set_target_properties(lcevc_dec_pipeline_vulkan PROPERTIES SOVERSION ${PIPELINE_VULKAN_VERSION})

target_compile_definitions(lcevc_dec_pipeline_vulkan PRIVATE VNEnablePipelineAPIExport)
if (VN_SDK_EXECUTABLES AND NOT ANDROID)
    target_compile_definitions(lcevc_dec_pipeline_vulkan PRIVATE GLFW_STATIC)
    target_compile_definitions(lcevc_dec_pipeline_vulkan PRIVATE VN_SDK_EXECUTABLES)
    target_link_libraries(lcevc_dec_pipeline_vulkan PRIVATE glfw)
endif ()

target_include_directories(
    lcevc_dec_pipeline_vulkan
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/src/hpp"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src/vma")

target_link_libraries(lcevc_dec_pipeline_vulkan PUBLIC ${LINK_LIBRARIES})

add_library(lcevc_dec::pipeline_vulkan ALIAS lcevc_dec_pipeline_vulkan)

lcevc_install(lcevc_dec_pipeline_vulkan)
