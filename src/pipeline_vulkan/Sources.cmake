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

list(
    APPEND
    SOURCES
    "src/backend_vulkan.cpp"
    "src/buffer_vulkan.cpp"
    "src/buffer_vulkan_ahb.cpp"
    "src/buffer_vulkan_ahb_buffer.cpp"
    "src/buffer_vulkan_ahb_image.cpp"
    "src/buffer_vulkan_vma.cpp"
    "src/compute_vulkan.cpp"
    "src/frame_context_vulkan.cpp"
    "src/frame_vulkan.cpp"
    "src/picture_lock_vulkan.cpp"
    "src/picture_vulkan.cpp"
    "src/pipeline_builder_vulkan.cpp"
    "src/pipeline_vulkan.cpp"
    "src/render_vulkan.cpp"
    "src/tasks_vulkan.cpp"
    "src/temporal_buffer_vulkan.cpp"
    "src/timestamp_vulkan.cpp")

list(
    APPEND
    HEADERS
    "src/backend_vulkan.h"
    "src/buffer_vulkan.h"
    "src/buffer_vulkan_ahb.h"
    "src/buffer_vulkan_ahb_buffer.h"
    "src/buffer_vulkan_ahb_image.h"
    "src/buffer_vulkan_vma.h"
    "src/compute_vulkan.h"
    "src/frame_context_vulkan.h"
    "src/frame_vulkan.h"
    "src/from_base.h"
    "src/picture_lock_vulkan.h"
    "src/picture_vulkan.h"
    "src/pipeline_builder_vulkan.h"
    "src/pipeline_config_vulkan.h"
    "src/pipeline_vulkan.h"
    "src/render_vulkan.h"
    "src/tasks_vulkan.h"
    "src/temporal_buffer_vulkan.h"
    "src/timestamp_vulkan.h"
    "src/vulkan_handle.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/apply.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_in.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_nv12.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_p010.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_planar8.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_planar16.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_p010.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_nv12.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba8.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba8_generic.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba10.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba10_generic.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/dequant_transform.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_fragment.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_vertex.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_1d.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_2d.h")

list(APPEND INTERFACES "include/LCEVC/pipeline_vulkan/create_pipeline.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

set(SHADER_INPUT_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/apply.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/common.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_common.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_common.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_in.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_nv12.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_p010.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_planar8.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_planar16.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_p010.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_nv12.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba8.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba8_generic.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba10.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/convert_out_image_rgba10_generic.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/dequant_transform.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/dequant_transform.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/render_fragment.frag
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/render_vertex.vert
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_constants.glsl
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_1d.comp
    ${CMAKE_CURRENT_LIST_DIR}/src/shaders/src/upscale_2d.comp)

set(SHADER_OUTPUT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/apply.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_in.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_nv12.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_p010.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_planar8.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_planar16.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_p010.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_nv12.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba8.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba8_generic.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba10.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/convert_out_image_rgba10_generic.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/dequant_transform.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_fragment.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/hpp/render_vertex.h
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_1d.h"
    "${CMAKE_CURRENT_BINARY_DIR}/src/hpp/upscale_2d.h")
