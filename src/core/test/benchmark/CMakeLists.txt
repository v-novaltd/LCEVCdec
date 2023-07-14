# Copyright 2022 - V-Nova Ltd.
#
if(VN_SDK_BENCHMARK)
	include(Sources.cmake)

	find_package(benchmark REQUIRED)

	add_executable(lcevc_core_test_benchmark)
	lcevc_set_properties(lcevc_core_test_benchmark)

	target_sources(lcevc_core_test_benchmark PRIVATE ${SOURCES})

	target_compile_features(lcevc_core_test_benchmark 
		PRIVATE
			cxx_std_17
	)

	target_link_libraries(lcevc_core_test_benchmark
		PRIVATE lcevc_dec::core_objects
				lcevc_dec::integration_utility
				lcevc_dec::platform
				lcevc_dec::compiler
				benchmark::benchmark
	)

	target_include_directories(lcevc_core_test_benchmark
		PRIVATE ../core/src/
	)

	add_executable(lcevc_dec::core_test_benchmark ALIAS lcevc_core_test_benchmark)

	install(TARGETS lcevc_core_test_benchmark)
endif()