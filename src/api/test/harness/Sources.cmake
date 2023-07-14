# Copyright 2023 - V-Nova Ltd.

list(APPEND SOURCES
    "src/hash.cpp"
    "src/main.cpp" )

set(ALL_FILES ${SOURCES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})

