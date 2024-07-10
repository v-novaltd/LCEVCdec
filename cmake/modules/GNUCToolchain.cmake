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

# Common variables set for cross compiling with GNUC
#

# Compilers
set(CMAKE_CXX_COMPILER "${TARGET_TRIPLE}-g++${TARGET_GCC_VERSION}")
set(CMAKE_CXX_COMPILER_AR "${TARGET_TRIPLE}-gcc-ar${TARGET_GCC_VERSION}")
set(CMAKE_CXX_COMPILER_RANLIB "${TARGET_TRIPLE}-gcc-ranlib${TARGET_GCC_VERSION}")
set(CMAKE_C_COMPILER "${TARGET_TRIPLE}-gcc${TARGET_GCC_VERSION}")
set(CMAKE_C_COMPILER_AR "${TARGET_TRIPLE}-gcc-ar${TARGET_GCC_VERSION}")
set(CMAKE_C_COMPILER_RANLIB "${TARGET_TRIPLE}-gcc-ranlib${TARGET_GCC_VERSION}")

# Binutils
set(CMAKE_ADDR2LINE "${TARGET_TRIPLE}-addr2line")
set(CMAKE_AR "${TARGET_TRIPLE}-ar")
set(CMAKE_LINKER "${TARGET_TRIPLE}-ld")
set(CMAKE_NM "${TARGET_TRIPLE}-nm")
set(CMAKE_OBJCOPY "${TARGET_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP "${TARGET_TRIPLE}-objdump")
set(CMAKE_RANLIB "${TARGET_TRIPLE}-ranlib")
set(CMAKE_READELF "${TARGET_TRIPLE}-readelf")
set(CMAKE_STRIP "${TARGET_TRIPLE}-strip")

# where is the target environment located
set(CMAKE_FIND_ROOT_PATH "/usr/${TARGET_TRIPLE}")

# adjust the default behavior of the FIND_XXX() commands: search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
