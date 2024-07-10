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

set(CONFIG ${CMAKE_BINARY_DIR}/generated/lcevc_config.h)

set(SOURCE_ROOT
    "src/bench_apply_cmdbuffer.cpp"
    "src/bench_blit.cpp"
    "src/bench_dither.cpp"
    "src/bench_entropy.cpp"
    "src/bench_fixture.h"
    "src/bench_fixture.cpp"
    "src/bench_sharpen.cpp"
    "src/bench_transform.cpp"
    "src/bench_upscale.cpp"
    "src/bench_utility.h"
    "src/bench_utility.cpp"
    "src/bench_utility_entropy.h"
    "src/bench_utility_entropy.cpp")

set(ALL_FILES ${SOURCE_ROOT})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
source_group("generated" FILES ${CONFIG})

# Convenience
set(SOURCES "CMakeLists.txt" "Sources.cmake" ${ALL_FILES} ${CONFIG})
