# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

# Enable building with RPATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Do not use CMAKE_INSTALL_RPATH during build process
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Set the installation RPATH to the 'lib' directory under CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_RPATH "@executable_path/../lib")

# Include link path directories in the RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Check if the 'lib' directory under CMAKE_INSTALL_PREFIX is one of the system's implicit link
# directories
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "@executable_path/../lib" isSystemDir)

# If it's not one of the system's implicit link directories, add it to the RPATH
if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
endif("${isSystemDir}" STREQUAL "-1")
