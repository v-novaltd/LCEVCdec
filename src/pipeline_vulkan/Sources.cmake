# Copyright (c) V-Nova International Limited 2025. All rights reserved.
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
    "src/buffer_vulkan.cpp"
    "src/frame_vulkan.cpp"
    "src/picture_lock_vulkan.cpp"
    "src/picture_vulkan.cpp"
    "src/pipeline_builder_vulkan.cpp"
    "src/pipeline_vulkan.cpp")

list(
    APPEND
    HEADERS
    "src/buffer_vulkan.h"
    "src/frame_vulkan.h"
    "src/picture_lock_vulkan.h"
    "src/picture_vulkan.h"
    "src/pipeline_builder_vulkan.h"
    "src/pipeline_config_vulkan.h"
    "src/pipeline_vulkan.h"
    "${CMAKE_BINARY_DIR}/src/pipeline_vulkan/src/hpp/apply.h"
    "${CMAKE_BINARY_DIR}/src/pipeline_vulkan/src/hpp/blit.h"
    "${CMAKE_BINARY_DIR}/src/pipeline_vulkan/src/hpp/conversion.h"
    "${CMAKE_BINARY_DIR}/src/pipeline_vulkan/src/hpp/upscale_horizontal.h"
    "${CMAKE_BINARY_DIR}/src/pipeline_vulkan/src/hpp/upscale_vertical.h")

list(APPEND INTERFACES "include/LCEVC/pipeline_vulkan/create_pipeline.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")
