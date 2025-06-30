# Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

set(SOURCES
    "src/test_pool.cpp"
    "src/event_tester.cpp"
    "src/decoder_asynchronous.cpp"
    "src/decoder_synchronous.cpp"
    "src/test_api_bad_streams.cpp"
    "src/test_api_events_threaded.cpp"
    "src/test_event_dispatcher.cpp"
    "src/test_pipeline_types.cpp"
    "src/utils.cpp")

set(HEADERS "src/event_tester.h" "src/decoder_asynchronous.h" "src/decoder_synchronous.h"
            "src/data.h" "src/utils.h")

# Convenience
set(ALL_FILES "CMakeLists.txt" "Sources.cmake" ${HEADERS} ${SOURCES} ${CONFIG})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
