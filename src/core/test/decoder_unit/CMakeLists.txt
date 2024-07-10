# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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
find_package(range-v3 REQUIRED)
if(VN_SDK_USE_GHC_FILESYSTEM)
    find_package(ghc_filesystem REQUIRED)
endif()

add_executable(lcevc_core_test_unit)
lcevc_set_properties(lcevc_core_test_unit)

target_sources(lcevc_core_test_unit PRIVATE ${SOURCES})

target_compile_features(lcevc_core_test_unit PRIVATE cxx_std_17)

# Disable DLL import as core is statically linked
target_compile_definitions(lcevc_core_test_unit PRIVATE VNDisablePublicAPI)

target_link_libraries(
    lcevc_core_test_unit
    PRIVATE lcevc_dec::core_static
            lcevc_dec::utility
            lcevc_dec::unit_test_utilities
            lcevc_dec::platform
            lcevc_dec::compiler
            GTest::GTest
            range-v3::range-v3
            $<TARGET_NAME_IF_EXISTS:ghcFilesystem::ghc_filesystem>)

add_executable(lcevc_dec::core_test_unit ALIAS lcevc_core_test_unit)

install(TARGETS lcevc_core_test_unit)
