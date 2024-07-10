# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

find_package(PkgConfig)

# ##################################################################################################
# GTest::GTest
# ##################################################################################################
# todo handle version, set dep>=ver or dep==ver in module spec here Find the package
pkg_check_modules(GTest gtest)
find_package_handle_standard_args(GTest DEFAULT_MSG GTest_FOUND GTest_VERSION)

# Propagate version
set(GTest_VERSION GTest_VERSION)

# Create interface library
add_library(GTest::GTest INTERFACE IMPORTED)
set_property(TARGET GTest::GTest PROPERTY INTERFACE_VERSION ${GTest_VERSION})
set_property(TARGET GTest::GTest PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTest_INCLUDE_DIRS}")
set_property(TARGET GTest::GTest PROPERTY INTERFACE_LINK_LIBRARIES "${GTest_LIBRARIES}")
set_property(TARGET GTest::GTest PROPERTY INTERFACE_LINK_DIRECTORIES "${GTest_LIBRARY_DIRS}")
set_property(TARGET GTest::GTest PROPERTY INTERFACE_COMPILE_OPTIONS "${GTest_CFLAGS}"
                                          "${GTest_CFLAGS_OTHER}")

# ##################################################################################################
# GTest::gtest
# ##################################################################################################
# Find the package component
pkg_check_modules(GTest gtest gtest)
find_package_handle_standard_args(GTest DEFAULT_MSG GTest_FOUND GTest_gtest_VERSION)

# Update the version name as modules seem to have it if not
if(GTest_gtest_VERSION AND NOT GTest_VERSION)
    set(GTest_VERSION GTest_gtest_VERSION)
    set_property(TARGET GTest::GTest PROPERTY INTERFACE_VERSION ${GTest_VERSION})
endif()

# Create interface library for component
add_library(GTest::gtest INTERFACE IMPORTED)
set_property(TARGET GTest::gtest PROPERTY INTERFACE_VERSION ${GTest_gtest_VERSION})
set_property(TARGET GTest::gtest PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTest_gtest_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET GTest::gtest PROPERTY INTERFACE_LINK_LIBRARIES "${GTest_LIBRARIES};")
set_property(TARGET GTest::gtest PROPERTY INTERFACE_LINK_DIRECTORIES "${GTest_gtest_LIBDIR}")
set_property(TARGET GTest::gtest PROPERTY INTERFACE_COMPILE_OPTIONS "${GTest_gtest_CFLAGS}"
                                          "${GTest_CFLAGS_OTHER}")

# ##################################################################################################
# GTest::gtest_main
# ##################################################################################################
# Find the package component
pkg_check_modules(GTest gtest gtest_main)
find_package_handle_standard_args(GTest DEFAULT_MSG GTest_FOUND GTest_gtest_VERSION)

# Update the version name as modules seem to have it if not
if(GTest_gtest_VERSION AND NOT GTest_VERSION)
    set(GTest_VERSION GTest_gtest_VERSION)
    set_property(TARGET GTest::GTest PROPERTY INTERFACE_VERSION ${GTest_VERSION})
endif()

# Create interface library for component
add_library(GTest::gtest_main INTERFACE IMPORTED)
set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_VERSION ${GTest_gtest_VERSION})
set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                               "${GTest_gtest_main_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_LINK_LIBRARIES "${GTest_LIBRARIES};")
set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_LINK_DIRECTORIES
                                               "${GTest_gtest_main_LIBDIR}")
set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_COMPILE_OPTIONS
                                               "${GTest_gtest_main_CFLAGS}" "${GTest_CFLAGS_OTHER}")

# ##################################################################################################
# GTest::gmock
# ##################################################################################################
# Find the package component
pkg_check_modules(GTest gtest gmock)
find_package_handle_standard_args(GTest DEFAULT_MSG GTest_FOUND GTest_gtest_VERSION)

# Update the version name as modules seem to have it if not
if(GTest_gtest_VERSION AND NOT GTest_VERSION)
    set(GTest_VERSION GTest_gtest_VERSION)
    set_property(TARGET GTest::GTest PROPERTY INTERFACE_VERSION ${GTest_VERSION})
endif()

# Create interface library for component
add_library(GTest::gmock INTERFACE IMPORTED)
set_property(TARGET GTest::gmock PROPERTY INTERFACE_VERSION ${GTest_gtest_VERSION})
set_property(TARGET GTest::gmock PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTest_gmock_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET GTest::gmock PROPERTY INTERFACE_LINK_LIBRARIES "${GTest_LIBRARIES};")
set_property(TARGET GTest::gmock PROPERTY INTERFACE_LINK_DIRECTORIES "${GTest_gmock_LIBDIR}")
set_property(TARGET GTest::gmock PROPERTY INTERFACE_COMPILE_OPTIONS "${GTest_gmock_CFLAGS}"
                                          "${GTest_CFLAGS_OTHER}")

# ##################################################################################################
# GTest::gmock_main
# ##################################################################################################
# Find the package component
pkg_check_modules(GTest gtest gmock_main)
find_package_handle_standard_args(GTest DEFAULT_MSG GTest_FOUND GTest_gtest_VERSION)

# Update the version name as modules seem to have it if not
if(GTest_gtest_VERSION AND NOT GTest_VERSION)
    set(GTest_VERSION GTest_gtest_VERSION)
    set_property(TARGET GTest::GTest PROPERTY INTERFACE_VERSION ${GTest_VERSION})
endif()

# Create interface library for component
add_library(GTest::gmock_main INTERFACE IMPORTED)
set_property(TARGET GTest::gmock_main PROPERTY INTERFACE_VERSION ${GTest_gtest_VERSION})
set_property(TARGET GTest::gmock_main PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                               "${GTest_gmock_main_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET GTest::gmock_main PROPERTY INTERFACE_LINK_LIBRARIES "${GTest_LIBRARIES};")
set_property(TARGET GTest::gmock_main PROPERTY INTERFACE_LINK_DIRECTORIES
                                               "${GTest_gmock_main_LIBDIR}")
set_property(TARGET GTest::gmock_main PROPERTY INTERFACE_COMPILE_OPTIONS
                                               "${GTest_gmock_main_CFLAGS}" "${GTest_CFLAGS_OTHER}")
