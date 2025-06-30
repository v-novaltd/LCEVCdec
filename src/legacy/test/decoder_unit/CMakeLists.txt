# Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

include(Sources.cmake)

find_package(GTest REQUIRED)
find_package(range-v3 REQUIRED)

add_executable(lcevc_dec_legacy_test_unit)
lcevc_set_properties(lcevc_dec_legacy_test_unit)

target_sources(lcevc_dec_legacy_test_unit PRIVATE ${SOURCES})

# Disable DLL import as core is statically linked
target_compile_definitions(lcevc_dec_legacy_test_unit PRIVATE VNDisablePublicAPI)

target_link_libraries(
    lcevc_dec_legacy_test_unit
    PRIVATE lcevc_dec::legacy_static
            lcevc_dec::utility
            lcevc_dec::unit_test_utilities
            lcevc_dec::platform
            lcevc_dec::compiler
            GTest::gtest
            lcevc_dec::gtest_main
            range-v3::range-v3)

add_executable(lcevc_dec::legacy_test_unit ALIAS lcevc_dec_legacy_test_unit)

install(TARGETS lcevc_dec_legacy_test_unit)
