## Copyright 2022 - V-Nova Ltd.
#
if(TARGET_ARCH MATCHES "^x86")
	target_compile_options(lcevc_dec::compiler INTERFACE -mavx)
endif()

if(VN_SDK_COVERAGE)
	target_compile_options(lcevc_dec::compiler INTERFACE --coverage)
	target_link_libraries(lcevc_dec::compiler INTERFACE gcov)
endif()

target_compile_options(lcevc_dec::compiler INTERFACE
    -Werror
    -Wall
#    -Wshadow
    -Wwrite-strings
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    $<$<COMPILE_LANGUAGE:C>:--std=c11>
    $<$<COMPILE_LANGUAGE:CXX>:--std=c++17>
    )
