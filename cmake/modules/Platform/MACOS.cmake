# Copyright (c) V-Nova International Limited 2022-2026. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
# software may be incorporated into a project under a compatible license provided the requirements
# of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
# licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
# ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
# THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

# Enable building with RPATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Use install rpath during build to avoid embedding absolute build-directory paths in binaries
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# Set the installation RPATH to the 'lib' directory under CMAKE_INSTALL_PREFIX
set(CMAKE_INSTALL_RPATH "@executable_path/../lib")

# Include link path directories in the RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Check if the 'lib' directory under CMAKE_INSTALL_PREFIX is one of the system's implicit link
# directories
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "@executable_path/../lib" isSystemDir)

# If it's not one of the system's implicit link directories, add it to the RPATH
if ("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
endif ("${isSystemDir}" STREQUAL "-1")

# Deterministic static archives: BSD ar lacks -D support, so use libtool -static -D instead
set(CMAKE_C_ARCHIVE_CREATE "libtool -static -D -o <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "libtool -static -D -o <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "libtool -static -D -o <TARGET> <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_APPEND "libtool -static -D -o <TARGET> <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -D <TARGET>")
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -D <TARGET>")
