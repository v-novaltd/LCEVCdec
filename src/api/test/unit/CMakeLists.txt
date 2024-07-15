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

include(Sources.cmake)

find_package(GTest REQUIRED)

add_executable(lcevc_dec_test_unit)
add_executable(lcevc_dec::test_unit ALIAS lcevc_dec_test_unit)
target_sources(lcevc_dec_test_unit PRIVATE ${SOURCES} ${HEADERS})
lcevc_set_properties(lcevc_dec_test_unit)

target_include_directories(lcevc_dec_test_unit
                           PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../utility/include")

target_link_libraries(
    lcevc_dec_test_unit PRIVATE lcevc_dec::api_static lcevc_dec::compiler lcevc_dec::platform
                                lcevc_dec::utility GTest::gtest)

install(TARGETS lcevc_dec_test_unit DESTINATION "bin")
