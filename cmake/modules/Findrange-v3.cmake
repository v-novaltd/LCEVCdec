# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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
# range-v3::range-v3
# ##################################################################################################

pkg_check_modules(range-v3 range-v3)
find_package_handle_standard_args(range-v3 DEFAULT_MSG range-v3_FOUND range-v3_VERSION)

# Propagate version
set(range-v3_VERSION range-v3_VERSION)

# Create interface library
add_library(range-v3::range-v3 INTERFACE IMPORTED)
set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_VERSION ${range-v3_VERSION})
set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                "${range-v3_INCLUDE_DIRS}")
set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_LINK_LIBRARIES "${range-v3_LIBRARIES}")
set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_LINK_DIRECTORIES
                                                "${range-v3_LIBRARY_DIRS}")
set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_COMPILE_OPTIONS "${range-v3_CFLAGS}"
                                                "${range-v3_CFLAGS_OTHER}")

# ##################################################################################################
# range-v3::meta
# ##################################################################################################
# Find the package component
pkg_check_modules(range-v3 range-v3 range-v3-meta)
find_package_handle_standard_args(range-v3 DEFAULT_MSG range-v3_FOUND range-v3_range-v3_VERSION)

# Update the version name as modules seem to have it if not
if (range-v3_range-v3_VERSION AND NOT range-v3_VERSION)
    set(range-v3_VERSION range-v3_range-v3_VERSION)
    set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_VERSION ${range-v3_VERSION})
endif ()

# Create interface library for component
add_library(range-v3::meta INTERFACE IMPORTED)
set_property(TARGET range-v3::meta PROPERTY INTERFACE_VERSION ${range-v3_range-v3_VERSION})
set_property(TARGET range-v3::meta PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                            "${range-v3_meta_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET range-v3::meta PROPERTY INTERFACE_LINK_LIBRARIES "${range-v3_LIBRARIES};")
set_property(TARGET range-v3::meta PROPERTY INTERFACE_LINK_DIRECTORIES "${range-v3_meta_LIBDIR}")
set_property(TARGET range-v3::meta PROPERTY INTERFACE_COMPILE_OPTIONS "${range-v3_meta_CFLAGS}"
                                            "${range-v3_CFLAGS_OTHER}")

# ##################################################################################################
# range-v3::concepts
# ##################################################################################################
# Find the package component
pkg_check_modules(range-v3 range-v3 range-v3-concepts)
find_package_handle_standard_args(range-v3 DEFAULT_MSG range-v3_FOUND range-v3_range-v3_VERSION)

# Update the version name as modules seem to have it if not
if (range-v3_range-v3_VERSION AND NOT range-v3_VERSION)
    set(range-v3_VERSION range-v3_range-v3_VERSION)
    set_property(TARGET range-v3::range-v3 PROPERTY INTERFACE_VERSION ${range-v3_VERSION})
endif ()

# Create interface library for component
add_library(range-v3::concepts INTERFACE IMPORTED)
set_property(TARGET range-v3::concepts PROPERTY INTERFACE_VERSION ${range-v3_range-v3_VERSION})
set_property(TARGET range-v3::concepts PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                "${range-v3_concepts_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET range-v3::concepts PROPERTY INTERFACE_LINK_LIBRARIES "${range-v3_LIBRARIES};")
set_property(TARGET range-v3::concepts PROPERTY INTERFACE_LINK_DIRECTORIES
                                                "${range-v3_concepts_LIBDIR}")
set_property(
    TARGET range-v3::concepts PROPERTY INTERFACE_COMPILE_OPTIONS "${range-v3_concepts_CFLAGS}"
                                       "${range-v3_CFLAGS_OTHER}")
