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

include(Sources.cmake)

find_package(range-v3 REQUIRED)
find_package(GTest REQUIRED)
find_package(fmt REQUIRED)

add_executable(lcevc_dec_pixel_processing_test_unit)
add_executable(lcevc_dec::test_pixel_processing_unit ALIAS lcevc_dec_pixel_processing_test_unit)
target_sources(lcevc_dec_pixel_processing_test_unit PRIVATE ${SOURCES} ${HEADERS})
lcevc_set_properties(lcevc_dec_pixel_processing_test_unit)

target_include_directories(
    lcevc_dec_pixel_processing_test_unit
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include" "${CMAKE_CURRENT_LIST_DIR}/../../src"
            "${CMAKE_SOURCE_DIR}/src/enhancement/src")

target_link_libraries(
    lcevc_dec_pixel_processing_test_unit
    PRIVATE lcevc_dec::platform
            lcevc_dec::compiler
            lcevc_dec::common
            lcevc_dec::pixel_processing
            lcevc_dec::pipeline
            lcevc_dec::gtest_main
            lcevc_dec::unit_test_utilities
            lcevc_dec::enhancement
            GTest::gtest
            range-v3::range-v3
            fmt::fmt)

install(TARGETS lcevc_dec_pixel_processing_test_unit)
