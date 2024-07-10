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

include("Sources.cmake")

add_library(lcevc_dec_core_sequencing STATIC)
add_library(lcevc_dec::sequencing ALIAS lcevc_dec_core_sequencing)
lcevc_set_properties(lcevc_dec_core_sequencing)

target_sources(lcevc_dec_core_sequencing PRIVATE ${SOURCES})

target_link_libraries(lcevc_dec_core_sequencing PUBLIC lcevc_dec::platform lcevc_dec::compiler)

target_include_directories(
    lcevc_dec_core_sequencing
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")

install(FILES ${INTERFACES} DESTINATION "include/LCEVC")
install(TARGETS lcevc_dec_core_sequencing DESTINATION lib)
