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

if(VN_SDK_JSON_CONFIG)
    find_package(rapidjson NAMES RapidJSON rapidjson REQUIRED)
endif()
find_package(fmt REQUIRED)
if(VN_SDK_BASE_DECODER)
    find_package(ffmpeg-libs REQUIRED)
endif()
if(VN_SDK_USE_GHC_FILESYSTEM)
    find_package(ghc_filesystem REQUIRED)
endif()

add_library(lcevc_dec_utility STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
lcevc_set_properties(lcevc_dec_utility)

target_include_directories(lcevc_dec_utility PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_include_directories(lcevc_dec_utility PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../api/include")

target_link_libraries(
    lcevc_dec_utility
    PUBLIC lcevc_dec::platform lcevc_dec::compiler $<TARGET_NAME_IF_EXISTS:ffmpeg-libs::ffmpeg-libs>
    PRIVATE fmt::fmt $<TARGET_NAME_IF_EXISTS:rapidjson::rapidjson>
            $<TARGET_NAME_IF_EXISTS:RapidJSON::RapidJSON>
            $<TARGET_NAME_IF_EXISTS:ghcFilesystem::ghc_filesystem>)

add_library(lcevc_dec::utility ALIAS lcevc_dec_utility)

include(GNUInstallDirs)

install(TARGETS lcevc_dec_utility)
install(FILES ${INTERFACES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LCEVC/utility)
install(FILES ${SOURCES} ${HEADERS} DESTINATION ${CMAKE_INSTALL_DOCDIR}/src/utility)
