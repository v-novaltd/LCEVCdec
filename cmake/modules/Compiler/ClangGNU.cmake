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

if (TARGET_ARCH STREQUAL "wasm")
    target_compile_options(lcevc_dec::compiler INTERFACE -pthread -msimd128 -mavx)
endif ()

if (VN_SDK_MINIMUM_LIB_SIZE)
    set(CMAKE_C_FLAGS_RELEASE
        "-Oz -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-plt -ffunction-sections -fdata-sections"
    )
    set(CMAKE_CXX_FLAGS_RELEASE
        "-Oz -DNDEBUG -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-plt -ffunction-sections -fdata-sections"
    )
    if (CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        string(APPEND CMAKE_CXX_FLAGS_RELEASE
               " -fvirtual-function-elimination -fwhole-program-vtables")
    endif ()
endif ()

if (VN_SDK_FUZZER)
    message(STATUS "VN_SDK_FUZZER enabled: building with libFuzzer + ASan + UBSan")
    set(VN_STRIP_RELEASE OFF)
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer
                        -fno-sanitize-recover=all -g)
    add_link_options(-fsanitize=fuzzer-no-link,address,undefined)
endif ()

if (VN_SDK_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
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
              -Wno-error=sign-compare
              -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.)
