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

target_compile_options(lcevc_dec::compiler INTERFACE /arch:AVX)

# XXX move to toolchain/platform ?
target_compile_definitions(
    lcevc_dec::compiler
    INTERFACE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS
              _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
              _VARIADIC_MAX=10 NOMINMAX)

target_compile_options(
    lcevc_dec::compiler
    INTERFACE # /W4 /wd4146 /wd4245 /D_CRT_SECURE_NO_WARNINGS)
              /nologo
              /Zc:wchar_t
              /Zc:forScope
              /GF
              /WX
              /W3
              /wd5105 # Disable warning about 'defined' operator in some macros, Windows SDK headers
                      # do this, which is UB in C code.
)
