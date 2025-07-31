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

lcevc_version_files(
    core_version
    "Core"
    "LCEVCdec-core"
    "lcevc_dec_core"
    "dll"
    "LCEVC Decoder Core"
    c)
include("Sources.cmake")

# Required libraries
# -------------------------------------------------------------------------------------------------

set(PUBLIC_LIBRARIES lcevc_dec::platform lcevc_dec::compiler)
set(PRIVATE_LIBRARIES lcevc_dec::core_version)

# Explicit static library for legacy unit tests
# -------------------------------------------------------------------------------------------------
if (VN_SDK_UNIT_TESTS)
    add_library(lcevc_dec_legacy_static STATIC)
    lcevc_set_properties(lcevc_dec_legacy_static)

    target_sources(lcevc_dec_legacy_static PRIVATE ${SOURCES})

    target_include_directories(lcevc_dec_legacy_static PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
                                                              "${CMAKE_CURRENT_LIST_DIR}/src")

    target_compile_definitions(lcevc_dec_legacy_static PUBLIC VNDisablePublicAPI)

    target_link_libraries(
        lcevc_dec_legacy_static
        PUBLIC "${PUBLIC_LIBRARIES}"
        PRIVATE "${PRIVATE_LIBRARIES}")

    if (VN_CORE_OVERLAY_IMAGE)
        target_link_libraries(lcevc_dec_legacy_static PRIVATE lcevc_dec::overlay_images)
    endif ()

    add_library(lcevc_dec::legacy_static ALIAS lcevc_dec_legacy_static)
endif ()

# Linked legacy library
# -------------------------------------------------------------------------------------------------

add_library(lcevc_dec_legacy)
lcevc_set_properties(lcevc_dec_legacy)
set_target_properties(lcevc_dec_legacy PROPERTIES SOVERSION ${PIPELINE_LEGACY_VERSION})

target_sources(lcevc_dec_legacy PRIVATE ${SOURCES})
if (VN_SDK_GENERATE_PGO)
    if (TARGET_COMPILER STREQUAL "MSVC")
        target_compile_options(lcevc_dec_legacy PRIVATE /GL)
        target_link_options(lcevc_dec_legacy PRIVATE /LTCG /GENPROFILE)
    elseif (TARGET_COMPILER MATCHES "[cC]lang")
        target_compile_options(lcevc_dec_legacy PRIVATE -fprofile-generate=. -mllvm
                                                        -enable-name-compression=false)
        target_link_options(lcevc_dec_legacy PRIVATE -fprofile-generate=. -mllvm
                            -enable-name-compression=false)
    else ()
        message(
            WARNING
                "Using gcc with profile-guided optimization is not recommended. The
            profile-generating build will work, but the profile-using build will only work if you
            use every file when profiling (since GCC generates profiles per-source-file, rather
            than per-library.")
    endif ()
elseif (VN_SDK_USE_PGO)
    if (TARGET_COMPILER STREQUAL "MSVC")
        target_compile_options(lcevc_dec_legacy PRIVATE /GL)
        target_link_options(lcevc_dec_legacy PRIVATE /LTCG
                            /USEPROFILE:PGD=${CMAKE_SOURCE_DIR}/lcevc_dec_legacy.pgd)
    elseif (TARGET_COMPILER MATCHES "[cC]lang")
        target_compile_options(lcevc_dec_legacy
                               PRIVATE -fprofile-use=${CMAKE_SOURCE_DIR}/default.profdata)
        target_link_options(lcevc_dec_legacy PRIVATE
                            -fprofile-use=${CMAKE_SOURCE_DIR}/default.profdata)
    else ()
        message(WARNING "Using gcc with profile-guided optimization is not recommended. This
            profile-using build will only work if you used every file when profiling (since GCC
            generates profiles per-source-file, rather than per-library.")
    endif ()
endif ()

target_include_directories(
    lcevc_dec_legacy
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")

target_compile_definitions(lcevc_dec_legacy PRIVATE VNEnablePublicAPIExport=1)

target_link_libraries(
    lcevc_dec_legacy
    PUBLIC "${PUBLIC_LIBRARIES}"
    PRIVATE "${PRIVATE_LIBRARIES}")

if (VN_CORE_OVERLAY_IMAGE)
    target_link_libraries(lcevc_dec_legacy PRIVATE lcevc_dec::overlay_images)
endif ()

add_library(lcevc_dec::legacy ALIAS lcevc_dec_legacy)

# -------------------------------------------------------------------------------------------------

lcevc_install(lcevc_dec_legacy)
install(FILES ${INTERFACES} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC/legacy")
