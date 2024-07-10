# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

list(
    APPEND
    SOURCES
    "src/mock_api.cpp"
    "src/test_base_decoder.cpp"
    "src/test_bin_reader.cpp"
    "src/test_bin_writer.cpp"
    "src/test_byte_order.cpp"
    "src/test_chrono.cpp"
    "src/test_configure.cpp"
    "src/test_convert.cpp"
    "src/test_extract.cpp"
    "src/test_get_program_dir.cpp"
    "src/test_md5.cpp"
    "src/test_parse_raw_name.cpp"
    "src/test_picture_layout.cpp"
    "src/test_string_utils.cpp")

set(ALL_FILES ${SOURCES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
