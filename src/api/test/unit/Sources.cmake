# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

set(SOURCES
    "src/test_api_events.cpp"
    "src/test_api_bad_streams.cpp"
    "src/test_decoder.cpp"
    "src/test_event_manager.cpp"
    "src/test_picture.cpp"
    "src/test_pool.cpp"
    "src/test_misc.cpp"
    "src/utils.cpp")

set(HEADERS "src/test_api_events.h" "src/test_decoder.h" "src/utils.h" "src/data.h")

# Convenience
set(ALL_FILES "CMakeLists.txt" "Sources.cmake" ${HEADERS} ${SOURCES} ${CONFIG})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
