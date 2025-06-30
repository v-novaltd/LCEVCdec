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

set(CONFIG ${CMAKE_BINARY_DIR}/generated/LCEVC/build_config.h)

set(SOURCE_COMMON
    "src/common/bitstream.c"
    "src/common/bitstream.h"
    "src/common/bytestream.c"
    "src/common/bytestream.h"
    "src/common/cmdbuffer.c"
    "src/common/cmdbuffer.h"
    "src/common/dither.c"
    "src/common/dither.h"
    "src/common/log.c"
    "src/common/log.h"
    "src/common/memory.c"
    "src/common/memory.h"
    "src/common/neon.h"
    "src/common/platform.h"
    "src/common/random.c"
    "src/common/random.h"
    "src/common/simd.c"
    "src/common/simd.h"
    "src/common/sse.h"
    "src/common/threading.c"
    "src/common/threading.h"
    "src/common/tile.c"
    "src/common/tile.h"
    "src/common/time.c"
    "src/common/time.h"
    "src/common/types.c"
    "src/common/types.h")

set(SOURCE_DECODE
    "src/decode/apply_cmdbuffer.c"
    "src/decode/apply_cmdbuffer.h"
    "src/decode/apply_cmdbuffer_applicator.h"
    "src/decode/apply_cmdbuffer_common.h"
    "src/decode/apply_cmdbuffer_neon.c"
    "src/decode/apply_cmdbuffer_scalar.c"
    "src/decode/apply_cmdbuffer_sse.c"
    "src/decode/apply_convert.c"
    "src/decode/apply_convert.h"
    "src/decode/decode_common.h"
    "src/decode/decode_parallel.c"
    "src/decode/decode_parallel.h"
    "src/decode/decode_serial.c"
    "src/decode/decode_serial.h"
    "src/decode/dequant.c"
    "src/decode/dequant.h"
    "src/decode/deserialiser.c"
    "src/decode/deserialiser.h"
    "src/decode/entropy.c"
    "src/decode/entropy.h"
    "src/decode/generate_cmdbuffer.c"
    "src/decode/generate_cmdbuffer.h"
    "src/decode/huffman.c"
    "src/decode/huffman.h"
    "src/decode/transform.c"
    "src/decode/transform.h"
    "src/decode/transform_coeffs.c"
    "src/decode/transform_coeffs.h"
    "src/decode/transform_unit.c"
    "src/decode/transform_unit.h")

set(SOURCE_SURFACE
    "src/surface/blit.c"
    "src/surface/blit.h"
    "src/surface/blit_common.h"
    "src/surface/blit_neon.c"
    "src/surface/blit_scalar.c"
    "src/surface/blit_sse.c"
    "src/surface/sharpen.c"
    "src/surface/sharpen.h"
    "src/surface/sharpen_common.h"
    "src/surface/sharpen_neon.c"
    "src/surface/sharpen_scalar.c"
    "src/surface/sharpen_sse.c"
    "src/surface/surface.c"
    "src/surface/surface.h"
    "src/surface/upscale.c"
    "src/surface/upscale.h"
    "src/surface/upscale_common.c"
    "src/surface/upscale_common.h"
    "src/surface/upscale_neon.c"
    "src/surface/upscale_neon.h"
    "src/surface/upscale_scalar.c"
    "src/surface/upscale_scalar.h"
    "src/surface/upscale_sse.c"
    "src/surface/upscale_sse.h")

if (VN_CORE_OVERLAY_IMAGE)
    list(APPEND SOURCE_SURFACE "src/surface/overlay.c" "src/surface/overlay.h")
endif ()

set(SOURCE_ROOT "src/api.c" "src/context.c" "src/context.h")

set(INTERFACES "include/LCEVC/legacy/PerseusDecoder.h")

set(ALL_FILES ${SOURCE_COMMON} ${SOURCE_DECODE} ${SOURCE_SURFACE} ${SOURCE_ROOT} ${INTERFACES})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
source_group("generated" FILES ${CONFIG})

# Convenience
set(SOURCES "CMakeLists.txt" "Sources.cmake" ${ALL_FILES} ${CONFIG})
