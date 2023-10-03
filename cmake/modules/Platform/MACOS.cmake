# Copyright 2022-2023 - V-Nova Ltd.
#
# Enable building with RPATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Do not use CMAKE_INSTALL_RPATH during build process
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Set the installation RPATH to the 'lib' directory under CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_RPATH "@executable_path/../lib")

# Include link path directories in the RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Check if the 'lib' directory under CMAKE_INSTALL_PREFIX is one of the system's implicit link directories
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "@executable_path/../lib" isSystemDir)

# If it's not one of the system's implicit link directories, add it to the RPATH
if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
endif("${isSystemDir}" STREQUAL "-1")
