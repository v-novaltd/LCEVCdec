# Copyright 2022 - V-Nova Ltd.

set(CONFIG
	${CMAKE_BINARY_DIR}/generated/lcevc_config.h
)

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
    "src/bench_utility_entropy.cpp"
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