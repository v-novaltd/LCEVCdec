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

include("Sources.cmake")

find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)
find_package(xxHash REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(lcevc_dec_test_harness ${SOURCES})
lcevc_set_properties(lcevc_dec_test_harness)

target_link_libraries(
    lcevc_dec_test_harness
    lcevc_dec::api
    lcevc_dec::common
    lcevc_dec::api_utility
    lcevc_dec::utility
    fmt::fmt
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    $<TARGET_NAME_IF_EXISTS:xxHash::xxHash>
    $<TARGET_NAME_IF_EXISTS:xxhash::xxhash>)

# Tests can include internal headers from helper sources
target_include_directories(
    lcevc_dec_test_harness
    PUBLIC "${CMAKE_SOURCE_DIR}/src/utility/src"
    PRIVATE "${CMAKE_SOURCE_DIR}/src")

add_executable(lcevc_dec::test_harness ALIAS lcevc_dec_test_harness)

install(TARGETS lcevc_dec_test_harness)
