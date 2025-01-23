# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

find_package(PkgConfig)

# ##################################################################################################
# xxHash::xxHash
# ##################################################################################################
# todo handle version, set dep>=ver or dep==ver in module spec here Find the package
pkg_check_modules(xxHash libxxhash)
find_package_handle_standard_args(xxHash DEFAULT_MSG xxHash_FOUND xxHash_VERSION)

# Propagate version
set(xxHash_VERSION xxHash_VERSION)

# Create interface library
add_library(xxHash::xxHash INTERFACE IMPORTED)
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_VERSION ${xxHash_VERSION})
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${xxHash_INCLUDE_DIRS}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_LINK_LIBRARIES "${xxHash_LIBRARIES}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_LINK_DIRECTORIES "${xxHash_LIBRARY_DIRS}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_COMPILE_OPTIONS "${xxHash_CFLAGS}"
                                            "${xxHash_CFLAGS_OTHER}")

# ##################################################################################################
# xxHash::xxhash
# ##################################################################################################
# Find the package component
pkg_check_modules(xxHash libxxhash libxxhash)
find_package_handle_standard_args(xxHash DEFAULT_MSG xxHash_FOUND xxHash_libxxhash_VERSION)

# Update the version name as modules seem to have it if not
if (xxHash_libxxhash_VERSION AND NOT xxHash_VERSION)
    set(xxHash_VERSION xxHash_libxxhash_VERSION)
    set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_VERSION ${xxHash_VERSION})
endif ()

# Create interface library for component
add_library(xxHash::xxhash INTERFACE IMPORTED)
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_VERSION ${xxHash_libxxhash_VERSION})
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                            "${xxHash_xxhash_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_LINK_LIBRARIES "${xxHash_LIBRARIES};")
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_LINK_DIRECTORIES "${xxHash_xxhash_LIBDIR}")
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_COMPILE_OPTIONS "${xxHash_xxhash_CFLAGS}"
                                            "${xxHash_CFLAGS_OTHER}")
