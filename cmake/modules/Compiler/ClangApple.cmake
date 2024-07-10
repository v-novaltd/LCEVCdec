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

if(TARGET_ARCH MATCHES "^x86")
    target_compile_options(lcevc_dec::compiler INTERFACE -mavx)
endif()

target_compile_options(
    lcevc_dec::compiler
    INTERFACE -Werror
              -Wall
              -Wshadow
              -Wstrict-prototypes
              -Wwrite-strings
              -Wpointer-arith
              -Wtautological-unsigned-zero-compare)

set(CMAKE_MACOSX_BUNDLE NO)
