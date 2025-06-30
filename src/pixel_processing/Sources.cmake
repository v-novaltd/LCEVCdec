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
    "src/apply_cmdbuffer.c"
    "src/apply_cmdbuffer_neon.c"
    "src/apply_cmdbuffer_scalar.c"
    "src/apply_cmdbuffer_sse.c"
    "src/dither.c"
    "src/dither.c"
    "src/blit_neon.c"
    "src/blit_scalar.c"
    "src/blit_sse.c"
    "src/blit.c"
    "src/upscale_neon.c"
    "src/upscale_scalar.c"
    "src/upscale_sse.c"
    "src/upscale_common.c"
    "src/upscale.c"
    "src/fp_types.c")

list(
    APPEND
    HEADERS
    "src/apply_cmdbuffer_applicator.h"
    "src/apply_cmdbuffer_common.h"
    "src/blit_common.h"
    "src/fp_types.h"
    "src/upscale_common.h"
    "src/upscale_neon.h"
    "src/upscale_scalar.h"
    "src/upscale_sse.h")

list(APPEND INTERFACES "include/LCEVC/pixel_processing/apply_cmdbuffer.h"
     "include/LCEVC/pixel_processing/dither.h" "include/LCEVC/pixel_processing/blit.h"
     "include/LCEVC/pixel_processing/upscale.h")

list(APPEND INTERFACES_DETAIL "include/LCEVC/pixel_processing/detail/apply_dither_scalar.h"
     "include/LCEVC/pixel_processing/detail/apply_dither_sse.h"
     "include/LCEVC/pixel_processing/detail/apply_dither_neon.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
