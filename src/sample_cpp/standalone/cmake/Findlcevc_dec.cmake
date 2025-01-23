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

#
find_package(PkgConfig)

# ##################################################################################################
# lcevc_dec::lcevc_dec
# ##################################################################################################
# todo handle version, set dep>=ver or dep==ver in module spec here Find the package
pkg_check_modules(lcevc_dec lcevc_dec)
find_package_handle_standard_args(lcevc_dec DEFAULT_MSG lcevc_dec_FOUND lcevc_dec_VERSION)

# Propagate version
set(lcevc_dec_VERSION lcevc_dec_VERSION)

# Create interface library
add_library(lcevc_dec::lcevc_dec INTERFACE IMPORTED)
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_VERSION ${lcevc_dec_VERSION})
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                  "${lcevc_dec_INCLUDE_DIRS}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_LINK_LIBRARIES "${lcevc_dec_LIBRARIES}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_LINK_DIRECTORIES
                                                  "${lcevc_dec_LIBRARY_DIRS}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_COMPILE_OPTIONS "${lcevc_dec_CFLAGS}"
                                                  "${lcevc_dec_CFLAGS_OTHER}")
