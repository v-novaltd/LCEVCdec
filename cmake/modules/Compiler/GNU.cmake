# Copyright 2022 - V-Nova Ltd.
#
if(TARGET_ARCH MATCHES "^x86")
	target_compile_options(lcevc_dec::compiler INTERFACE -mavx)
endif()

target_compile_options(lcevc_dec::compiler INTERFACE
    -Werror
    -Wall
#    -Wshadow
    -Wwrite-strings
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    $<$<COMPILE_LANGUAGE:C>:--std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:--std=c++17>
    )
