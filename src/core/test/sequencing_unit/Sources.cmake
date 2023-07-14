# Copyright 2022 - V-Nova Ltd.

set(SOURCE_ROOT
    "src/constants.h"
    "src/test_lcevc_container.cpp"
)

set(ALL_FILES ${SOURCE_ROOT})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})

# Convenience
set(SOURCES
	"CMakeLists.txt"
	"Sources.cmake"
	${ALL_FILES}
)
