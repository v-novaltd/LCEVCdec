# Copyright 2022 - V-Nova Ltd.
#

list(APPEND SOURCES
	"src/lcevc_container.c"
	"src/predict_timehandle.c"
	"src/predict_timehandle.h"
)

set(INTERFACES
	"include/lcevc_container.h"
)

set(ALL_FILES ${SOURCES} ${INTERFACES})

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})

# Convenience
set(SOURCES
	"CMakeLists.txt"
	"Sources.cmake"
	${ALL_FILES}
)