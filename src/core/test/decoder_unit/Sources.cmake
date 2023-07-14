# Copyright 2022 - V-Nova Ltd.

set(CONFIG
	${CMAKE_BINARY_DIR}/generated/lcevc_config.h
)

set(SOURCE_ROOT
    "src/unit_apply_cmdbuffer.cpp"
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
    "src/unit_utility.cpp"
)

set(ALL_FILES ${SOURCE_ROOT})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
source_group("generated" FILES ${CONFIG})

# Convience
set(SOURCES
    "CMakeLists.txt"
    "Sources.cmake"
    ${ALL_FILES}
    ${CONFIG}
)