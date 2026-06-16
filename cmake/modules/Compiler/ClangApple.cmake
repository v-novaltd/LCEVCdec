# Copyright (c) V-Nova International Limited 2022-2026. All rights reserved.
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

if (VN_SDK_SIMD AND TARGET_ARCH MATCHES "^x86")
    target_compile_options(lcevc_dec::compiler INTERFACE -msse4.1)
endif ()

if (VN_SDK_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif ()

if (VN_SDK_MINIMUM_LIB_SIZE)
    set(CMAKE_C_FLAGS_RELEASE
        "-Oz -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections"
    )
    set(CMAKE_CXX_FLAGS_RELEASE
        "-Oz -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections"
    )
endif ()

target_compile_options(
    lcevc_dec::compiler
    INTERFACE $<$<BOOL:${VN_SDK_WARNINGS_FAIL}>:-Werror>
              $<$<BOOL:${VN_SDK_MINIMUM_LIB_SIZE}>:-fno-exceptions>
              $<$<AND:$<BOOL:${VN_SDK_MINIMUM_LIB_SIZE}>,$<COMPILE_LANGUAGE:CXX>>:-fno-rtti>
              -Wall
              -Wshadow
              -Wstrict-prototypes
              -Wwrite-strings
              -Wpointer-arith
              -Wtautological-unsigned-zero-compare
              -Wno-error=missing-field-initializers
              -Wno-error=unused-parameter
              -Wno-error=type-limits
              -Wno-error=sign-compare)

set(CMAKE_MACOSX_BUNDLE NO)
