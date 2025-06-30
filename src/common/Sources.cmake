# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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
    "src/acceleration.c"
    "src/common_configuration.cpp"
    "src/configure.cpp"
    "src/deque.c"
    "src/diagnostics.c"
    "src/diagnostics_buffer.c"
    "src/diagnostics_stdio.c"
    "src/diagnostics_tracefile.c"
    "src/memory.c"
    "src/memory_malloc.c"
    "src/random.c"
    "src/ring_buffer.c"
    "src/rolling_arena.c"
    "src/shared_library.c"
    "src/string_format.c"
    "src/task_pool.c"
    "src/vector.c")

if (NOT VN_SDK_THREADS_CUSTOM)
    if (WIN32)
        list(APPEND SOURCES "src/threads_win32.c")
    else ()
        list(APPEND SOURCES "src/threads_pthread.c")
    endif ()
endif ()

list(APPEND HEADERS "src/string_format.h")

list(
    APPEND
    INTERFACES
    "include/LCEVC/common/acceleration.h"
    "include/LCEVC/common/bitutils.h"
    "include/LCEVC/common/check.h"
    "include/LCEVC/common/class_utils.hpp"
    "include/LCEVC/common/configure.hpp"
    "include/LCEVC/common/configure_members.hpp"
    "include/LCEVC/common/constants.h"
    "include/LCEVC/common/cpp_tools.h"
    "include/LCEVC/common/deque.h"
    "include/LCEVC/common/diagnostics.h"
    "include/LCEVC/common/diagnostics_buffer.h"
    "include/LCEVC/common/limit.h"
    "include/LCEVC/common/log.h"
    "include/LCEVC/common/memory.h"
    "include/LCEVC/common/neon.h"
    "include/LCEVC/common/platform.h"
    "include/LCEVC/common/printf_macros.h"
    "include/LCEVC/common/random.h"
    "include/LCEVC/common/return_code.h"
    "include/LCEVC/common/ring_buffer.h"
    "include/LCEVC/common/shared_library.h"
    "include/LCEVC/common/sse.h"
    "include/LCEVC/common/task_pool.h"
    "include/LCEVC/common/threads.h"
    "include/LCEVC/common/vector.h")

list(
    APPEND
    INTERFACES_DETAIL
    "include/LCEVC/common/detail/deque.h"
    "include/LCEVC/common/detail/diagnostics.h"
    "include/LCEVC/common/detail/diagnostics_buffer.h"
    "include/LCEVC/common/detail/ring_buffer.h"
    "include/LCEVC/common/detail/rolling_arena.h"
    "include/LCEVC/common/detail/task_pool.h"
    "include/LCEVC/common/detail/threads_pthread.h"
    "include/LCEVC/common/detail/threads_win32.h"
    "include/LCEVC/common/detail/vector.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
