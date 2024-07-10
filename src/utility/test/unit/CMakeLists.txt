# Copyright (c) V-Nova International Limited 2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

include("Sources.cmake")

find_package(GTest REQUIRED)
find_package(fmt REQUIRED)
find_package(xxHash REQUIRED)
find_package(CLI11 REQUIRED)
if(VN_SDK_USE_GHC_FILESYSTEM)
    find_package(ghc_filesystem REQUIRED)
endif()

add_executable(lcevc_dec_utility_test ${SOURCES})
lcevc_set_properties(lcevc_dec_utility_test)

target_link_libraries(
    lcevc_dec_utility_test
    lcevc_dec::utility
    lcevc_dec::unit_test_utilities
    lcevc_dec::api
    GTest::gtest
    GTest::gtest_main
    fmt::fmt
    CLI11::CLI11
    xxHash::xxHash
    $<TARGET_NAME_IF_EXISTS:ghcFilesystem::ghc_filesystem>)

# Tests can include internal headers from helper sources
target_include_directories(lcevc_dec_utility_test PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../src")

add_executable(lcevc_dec::utility_test ALIAS lcevc_dec_utility_test)

install(TARGETS lcevc_dec_utility_test DESTINATION bin)
