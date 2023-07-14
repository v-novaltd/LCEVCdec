# Copyright 2022 - V-Nova Ltd.

list(APPEND SOURCES
    "src/find_assets_dir.cpp"
    "src/mock_api.cpp"
    "src/test_base_decoder.cpp"
    "src/test_bin_reader.cpp"
    "src/test_byte_order.cpp"
    "src/test_configure.cpp"
    "src/test_convert.cpp"
    "src/test_extract.cpp"
    "src/test_get_program_dir.cpp"
    "src/test_md5.cpp"
    "src/test_parse_raw_name.cpp"
    "src/test_picture_layout.cpp"
    "src/test_string_utils.cpp"
    )

set(ALL_FILES ${SOURCES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
