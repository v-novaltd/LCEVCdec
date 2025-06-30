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

list(
    APPEND
    SOURCES
    "src/diagnostics_ostream.cpp"
    "src/test_configure.cpp"
    "src/test_cpp_tools.cpp"
    "src/test_deque.cpp"
    "src/test_diagnostics_buffer.cpp"
    "src/test_diagnostics_c.c"
    "src/test_diagnostics_cpp.cpp"
    "src/test_memory.cpp"
    "src/test_ring_buffer.cpp"
    "src/test_rolling_arena.cpp"
    "src/test_string_format.cpp"
    "src/test_task_pool.cpp"
    "src/test_task_group.cpp"
    "src/test_task_pool_wrappers.cpp"
    "src/test_threads.cpp"
    "src/test_trace.cpp"
    "src/test_vector.cpp")

list(APPEND SOURCES_MAIN "src/common_main.cpp")

set(HEADERS)

# Convenience
set(ALL_FILES "CMakeLists.txt" "Sources.cmake" ${HEADERS} ${SOURCES} ${CONFIG})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
