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
    "src/approximate_pa.c"
    "src/bitstream.c"
    "src/bytestream.c"
    "src/chunk.c"
    "src/chunk_parser.c"
    "src/cmdbuffer_cpu.c"
    "src/cmdbuffer_gpu.c"
    "src/config_parser.c"
    "src/config_pool.c"
    "src/decode.c"
    "src/dequant.c"
    "src/dimensions.c"
    "src/entropy.c"
    "src/huffman.c"
    "src/log_utilities.c"
    "src/tile_parser.c"
    "src/transform.c"
    "src/transform_unit.c")

list(
    APPEND
    HEADERS
    "src/bitstream.h"
    "src/bytestream.h"
    "src/chunk.h"
    "src/chunk_parser.h"
    "src/config_parser_types.h"
    "src/dequant.h"
    "src/entropy.h"
    "src/huffman.h"
    "src/log_utilities.h"
    "src/tile_parser.h"
    "src/transform.h")

list(
    APPEND
    INTERFACES
    "include/LCEVC/enhancement/approximate_pa.h"
    "include/LCEVC/enhancement/bitstream_types.h"
    "include/LCEVC/enhancement/cmdbuffer_cpu.h"
    "include/LCEVC/enhancement/cmdbuffer_gpu.h"
    "include/LCEVC/enhancement/config_parser.h"
    "include/LCEVC/enhancement/config_pool.h"
    "include/LCEVC/enhancement/config_types.h"
    "include/LCEVC/enhancement/decode.h"
    "include/LCEVC/enhancement/dimensions.h"
    "include/LCEVC/enhancement/hdr_types.h"
    "include/LCEVC/enhancement/transform_unit.h")

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
