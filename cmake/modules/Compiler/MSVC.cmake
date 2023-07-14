# Copyright 2022 - V-Nova Ltd.
#

target_compile_options(lcevc_dec::compiler INTERFACE
	/arch:AVX)

# XXX move to toolchain/platform ?
target_compile_definitions(lcevc_dec::compiler INTERFACE
	_SCL_SECURE_NO_WARNINGS
	_CRT_SECURE_NO_WARNINGS
	_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	_VARIADIC_MAX=10
	NOMINMAX)

target_compile_options(lcevc_dec::compiler INTERFACE # /W4 /wd4146 /wd4245 /D_CRT_SECURE_NO_WARNINGS)
        /nologo
        /Zc:wchar_t
        /Zc:forScope
        /GF
        /WX
        /W3
        /wd5105 # Disable warning about 'defined' operator in some macros, Windows SDK headers do this, which is UB in C code.
        )
