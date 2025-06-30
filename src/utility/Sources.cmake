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
    "src/bin_reader.cpp"
    "src/bin_writer.cpp"
    "src/check.cpp"
    "src/get_program_dir.cpp"
    "src/md5.cpp"
    "src/parse_raw_name.cpp"
    "src/picture_functions.cpp"
    "src/picture_lock.cpp"
    "src/raw_reader.cpp"
    "src/raw_writer.cpp"
    "src/string_utils.cpp"
    "src/types_convert.cpp"
    "src/types_cli11.cpp"
    "src/types_stream.cpp")

if (VN_SDK_BASE_DECODER)
    list(
        APPEND
        SOURCES
        "src/base_decoder.cpp"
        "src/base_decoder_libav.cpp"
        "src/base_decoder_bin.cpp"
        "src/base_decoder_bin_linear.cpp"
        "src/base_decoder_bin_non_linear.cpp")
endif ()
if (VN_SDK_JSON_CONFIG)
    list(APPEND SOURCES "src/configure.cpp")
endif ()

list(APPEND HEADERS "src/bin_writer.h" "src/enum_map.h" "src/parse_raw_name.h"
     "src/base_decoder_bin.h")

list(
    APPEND
    INTERFACES
    "include/LCEVC/utility/base_decoder.h"
    "include/LCEVC/utility/bin_reader.h"
    "include/LCEVC/utility/byte_order.h"
    "include/LCEVC/utility/check.h"
    "include/LCEVC/utility/get_program_dir.h"
    "include/LCEVC/utility/picture_functions.h"
    "include/LCEVC/utility/raw_reader.h"
    "include/LCEVC/utility/raw_writer.h"
    "include/LCEVC/utility/string_utils.h"
    "include/LCEVC/utility/timestamp.h"
    "include/LCEVC/utility/types.h"
    "include/LCEVC/utility/types_cli11.h"
    "include/LCEVC/utility/types_convert.h"
    "include/LCEVC/utility/types_fmt.h"
    "include/LCEVC/utility/types_stream.h")

if (VN_SDK_BASE_DECODER)
    list(APPEND INTERFACES "include/LCEVC/utility/base_decoder.h")
endif ()
if (VN_SDK_JSON_CONFIG)
    list(APPEND INTERFACES "include/LCEVC/utility/configure.h")
endif ()

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
