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
    "src/buffer_cpu.cpp"
    "src/frame_cpu.cpp"
    "src/picture_cpu.cpp"
    "src/picture_lock_cpu.cpp"
    "src/pipeline_builder_cpu.cpp"
    "src/pipeline_cpu.cpp")

list(
    APPEND
    HEADERS
    "src/buffer_cpu.h"
    "src/frame_cpu.h"
    "src/picture_cpu.h"
    "src/picture_lock_cpu.h"
    "src/pipeline_builder_cpu.h"
    "src/pipeline_config_cpu.h"
    "src/pipeline_cpu.h")

list(APPEND INTERFACES "include/LCEVC/pipeline_cpu/create_pipeline.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
