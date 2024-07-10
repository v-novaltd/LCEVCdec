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
# rapidjson::rapidjson
# ##################################################################################################
# todo handle version, set dep>=ver or dep==ver in module spec here Find the package
pkg_check_modules(rapidjson rapidjson)
find_package_handle_standard_args(rapidjson DEFAULT_MSG rapidjson_FOUND rapidjson_VERSION)

# Propagate version
set(rapidjson_VERSION rapidjson_VERSION)

# Create interface library
add_library(rapidjson::rapidjson INTERFACE IMPORTED)
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_VERSION ${rapidjson_VERSION})
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                  "${rapidjson_INCLUDE_DIRS}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_LINK_LIBRARIES "${rapidjson_LIBRARIES}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_LINK_DIRECTORIES
                                                  "${rapidjson_LIBRARY_DIRS}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_COMPILE_OPTIONS "${rapidjson_CFLAGS}"
                                                  "${rapidjson_CFLAGS_OTHER}")
