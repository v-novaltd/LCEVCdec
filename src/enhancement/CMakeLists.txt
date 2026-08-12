# Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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

add_library(lcevc_dec_enhancement ${VN_SDK_STATIC_LIB_TYPE} ${SOURCES} ${HEADERS} ${INTERFACES})
lcevc_set_properties(lcevc_dec_enhancement)

target_include_directories(lcevc_dec_enhancement ${VN_PUBLIC_LINKAGE}
                           "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(lcevc_dec_enhancement ${VN_PUBLIC_LINKAGE} lcevc_dec::platform
                      lcevc_dec::compiler lcevc_dec::common)

# Configurable Huffman decode parameters:

if (NOT DEFINED VN_BIG_TABLE_LEADING_ZEROES_BITS)
    set(VN_BIG_TABLE_LEADING_ZEROES_BITS 4)
endif ()
target_compile_definitions(
    lcevc_dec_enhancement ${VN_PRIVATE_LINKAGE}
                          VN_BIG_TABLE_LEADING_ZEROES_BITS=${VN_BIG_TABLE_LEADING_ZEROES_BITS})

if (NOT DEFINED VN_BIG_TABLE_MAX_CODE_SIZE)
    set(VN_BIG_TABLE_MAX_CODE_SIZE 8)
endif ()
target_compile_definitions(
    lcevc_dec_enhancement ${VN_PRIVATE_LINKAGE}
                          VN_BIG_TABLE_MAX_CODE_SIZE=${VN_BIG_TABLE_MAX_CODE_SIZE})

if (NOT DEFINED VN_SMALL_TABLE_MAX_SIZE)
    set(VN_SMALL_TABLE_MAX_SIZE 10)
endif ()
target_compile_definitions(lcevc_dec_enhancement ${VN_PRIVATE_LINKAGE}
                                                 VN_SMALL_TABLE_MAX_SIZE=${VN_SMALL_TABLE_MAX_SIZE})

add_library(lcevc_dec::enhancement ALIAS lcevc_dec_enhancement)

if (NOT BUILD_SHARED_LIBS)
    lcevc_install(lcevc_dec_enhancement)
    install(FILES ${INTERFACES} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC/enhancement")
endif ()
