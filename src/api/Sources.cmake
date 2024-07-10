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

list(
    APPEND
    SOURCES
    "src/api.cpp"
    "src/buffer_manager.cpp"
    "src/decoder.cpp"
    "src/decoder_config.cpp"
    "src/decoder_pool.cpp"
    "src/event_manager.cpp"
    "src/interface.cpp"
    "src/lcevc_processor.cpp"
    "src/log.cpp"
    "src/picture.cpp"
    "src/picture_copy.cpp"
    "src/picture_lock.cpp"
    "src/pool.cpp"
    "src/threading.cpp")

list(
    APPEND
    HEADERS
    "src/accel_context.h"
    "src/buffer_manager.h"
    "src/config_map.h"
    "src/decoder.h"
    "src/decoder_config.h"
    "src/decoder_pool.h"
    "src/enums.h"
    "src/event_manager.h"
    "src/handle.h"
    "src/interface.h"
    "src/lcevc_processor.h"
    "src/log.h"
    "src/picture.h"
    "src/picture_copy.h"
    "src/picture_lock.h"
    "src/pool.h"
    "src/threading.h")

list(APPEND INTERFACES "include/LCEVC/lcevc_dec.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
