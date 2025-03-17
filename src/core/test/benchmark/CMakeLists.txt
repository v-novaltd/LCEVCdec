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

find_package(benchmark REQUIRED)

add_executable(lcevc_core_test_benchmark)
lcevc_set_properties(lcevc_core_test_benchmark)

target_sources(lcevc_core_test_benchmark PRIVATE ${SOURCES})

target_compile_features(lcevc_core_test_benchmark PRIVATE cxx_std_17)

target_link_libraries(
    lcevc_core_test_benchmark
    PRIVATE lcevc_dec::core_static
            lcevc_dec::core
            lcevc_dec::utility
            lcevc_dec::unit_test_utilities
            lcevc_dec::platform
            lcevc_dec::compiler
            benchmark::benchmark)

target_include_directories(lcevc_core_test_benchmark PRIVATE ../core/src/)

add_executable(lcevc_dec::core_benchmark ALIAS lcevc_core_test_benchmark)

install(TARGETS lcevc_core_test_benchmark)
