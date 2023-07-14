# Copyright 2022 - V-Nova Ltd.
#
if(TARGET_ARCH MATCHES "^x86")
	target_compile_options(lcevc_dec::compiler INTERFACE -mavx)
endif()

if(TARGET_ARCH STREQUAL "wasm")
	target_compile_options(lcevc_dec::compiler INTERFACE -pthread -msimd128 -mavx)
endif()

target_compile_options(lcevc_dec::compiler INTERFACE
	-Werror
	-Wall
	-Wshadow
	-Wstrict-prototypes
	-Wwrite-strings
	-Wpointer-arith
	-Wtautological-unsigned-zero-compare
)
