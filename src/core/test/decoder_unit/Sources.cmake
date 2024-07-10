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
    "src/unit_api.cpp"
    "src/unit_bitstream.cpp"
    "src/unit_blit.cpp"
    "src/unit_bytestream.cpp"
    "src/unit_dither.cpp"
    "src/unit_fixture.h"
    "src/unit_fixture.cpp"
    "src/unit_rng.h"
    "src/unit_rng.cpp"
    "src/unit_sharpen.cpp"
    "src/unit_transform.cpp"
    "src/unit_utility.h"
    "src/unit_utility.cpp")

set(ALL_FILES ${SOURCE_ROOT})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
source_group("generated" FILES ${CONFIG})

# Convenience
set(SOURCES "CMakeLists.txt" "Sources.cmake" ${ALL_FILES} ${CONFIG})
