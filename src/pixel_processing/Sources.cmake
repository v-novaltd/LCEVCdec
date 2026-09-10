# Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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
    "src/add.c"
    "src/add_neon.c"
    "src/add_sse.c"
    "src/apply_cmdbuffer.c"
    "src/apply_cmdbuffer_neon.c"
    "src/apply_cmdbuffer_sse.c"
    "src/convert_neon.c"
    "src/convert_sse.c"
    "src/convert.c"
    "src/convert_scalar.c"
    "src/dither.c"
    "src/fp_types.c"
    "src/upscale.c"
    "src/upscale_avx.c"
    "src/upscale_neon.c"
    "src/sharpen.c"
    "src/sharpen_neon.c"
    "src/sharpen_sse.c")

if (VN_SDK_SIMD AND TARGET_ARCH MATCHES "^x86")
    if (MSVC)
        set_source_files_properties("src/upscale_avx.c" PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    else ()
        set(SSE_SOURCES "src/add_sse.c" "src/apply_cmdbuffer_sse.c" "src/convert_sse.c"
                        "src/sharpen_sse.c")
        if (NOT VN_SDK_MINIMUM_LIB_SIZE)
            list(APPEND SSE_SOURCES "src/upscale_sse.c")
        endif ()
        set_source_files_properties(${SSE_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse4.1")
        set_source_files_properties("src/upscale_avx.c" PROPERTIES COMPILE_OPTIONS "-mavx2")
    endif ()
endif ()

if (NOT VN_SDK_MINIMUM_LIB_SIZE)
    list(
        APPEND
        SOURCES
        "src/add_scalar.c"
        "src/apply_cmdbuffer_scalar.c"
        "src/upscale_scalar.c"
        "src/upscale_sse.c"
        "src/sharpen_scalar.c")
endif ()

list(
    APPEND
    HEADERS
    "src/add_common.h"
    "src/apply_cmdbuffer_applicator.h"
    "src/apply_cmdbuffer_common.h"
    "src/convert_common.h"
    "src/fp_types.h"
    "src/upscale_common.h"
    "src/sharpen_common.h")

list(
    APPEND
    INTERFACES
    "include/LCEVC/pixel_processing/add.h"
    "include/LCEVC/pixel_processing/apply_cmdbuffer.h"
    "include/LCEVC/pixel_processing/dither.h"
    "include/LCEVC/pixel_processing/convert.h"
    "include/LCEVC/pixel_processing/upscale.h"
    "include/LCEVC/pixel_processing/sharpen.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
