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

add_library(lcevc_dec_pipeline_legacy ${SOURCES} ${HEADERS} ${INTERFACES})
lcevc_set_properties(lcevc_dec_pipeline_legacy)
set_target_properties(lcevc_dec_pipeline_legacy PROPERTIES SOVERSION ${PIPELINE_LEGACY_VERSION})

target_compile_definitions(lcevc_dec_pipeline_legacy PRIVATE VNEnablePipelineAPIExport)

target_include_directories(lcevc_dec_pipeline_legacy PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(
    lcevc_dec_pipeline_legacy
    PUBLIC lcevc_dec::platform lcevc_dec::compiler lcevc_dec::pipeline
    PRIVATE lcevc_dec::legacy lcevc_dec::sequencer lcevc_dec::common lcevc_dec::api_utility)

add_library(lcevc_dec::pipeline_legacy ALIAS lcevc_dec_pipeline_legacy)

lcevc_install(lcevc_dec_pipeline_legacy)

if (VN_SDK_UNIT_TESTS)
    add_library(lcevc_dec_pipeline_legacy_static STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
    lcevc_set_properties(lcevc_dec_pipeline_legacy_static)

    target_compile_definitions(lcevc_dec_pipeline_legacy_static PRIVATE VNDisablePipelineAPI)

    target_include_directories(lcevc_dec_pipeline_legacy_static
                               PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

    target_link_libraries(
        lcevc_dec_pipeline_legacy_static
        PUBLIC lcevc_dec::platform lcevc_dec::compiler lcevc_dec::pipeline
        PRIVATE lcevc_dec::legacy_static lcevc_dec::sequencer lcevc_dec::common
                lcevc_dec::api_utility)

    add_library(lcevc_dec::pipeline_legacy_static ALIAS lcevc_dec_pipeline_legacy_static)
endif ()
