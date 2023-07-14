# Copyright 2023 - V-Nova Ltd.
#
set(SOURCES
    "src/test_decoder.cpp"
    "src/test_event_manager.cpp"
    "src/test_picture.cpp"
    "src/test_pool.cpp"
    "src/test_misc.cpp"
    "src/utils.cpp"
)

set(HEADERS
	"src/test_decoder.h"
	"src/utils.h"
)

# Convenience
set(ALL_FILES
	"CMakeLists.txt"
	"Sources.cmake"
	${HEADERS}
	${SOURCES}
	${CONFIG}
)

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
