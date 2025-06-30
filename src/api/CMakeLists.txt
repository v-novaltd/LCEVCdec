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

lcevc_version_files(
    api_version
    "API"
    "LCEVCdec-api"
    "lcevc_dec_api"
    "dll"
    "LCEVC Decoder API"
    cpp)
include("Sources.cmake")

# Required libraries
# -------------------------------------------------------------------------------------------------

set(PRIVATE_LIBRARIES
    lcevc_dec::api_utility
    lcevc_dec::api_version
    lcevc_dec::common
    lcevc_dec::pipeline
    $<$<BOOL:${VN_SDK_PIPELINE_CPU}>:lcevc_dec::pipeline_cpu>
    $<$<BOOL:${VN_SDK_PIPELINE_LEGACY}>:lcevc_dec::pipeline_legacy>
    $<$<BOOL:${VN_SDK_PIPELINE_VULKAN}>:lcevc_dec::pipeline_vulkan>)

# Linked lcevc_api (for normal client usage)
# -------------------------------------------------------------------------------------------------

add_library(lcevc_dec_api ${SOURCES} ${HEADERS} ${INTERFACES})
add_library(lcevc_dec::api ALIAS lcevc_dec_api)
lcevc_set_properties(lcevc_dec_api)

target_include_directories(
    lcevc_dec_api
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")

target_link_libraries(lcevc_dec_api PRIVATE "${PRIVATE_LIBRARIES}")

# Static lcevc_api_static (for unit tests)
# -------------------------------------------------------------------------------------------------
if (VN_SDK_UNIT_TESTS)
    add_library(lcevc_dec_api_static STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
    add_library(lcevc_dec::api_static ALIAS lcevc_dec_api_static)
    lcevc_set_properties(lcevc_dec_api_static)

    target_include_directories(lcevc_dec_api_static PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
                                                           "${CMAKE_CURRENT_LIST_DIR}/src")

    target_link_libraries(lcevc_dec_api_static PRIVATE "${PRIVATE_LIBRARIES}")
endif ()

# Installation
# -------------------------------------------------------------------------------------------------

lcevc_install(lcevc_dec_api)
install(FILES ${INTERFACES} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC")
