# Copyright 2022 - V-Nova Ltd.
#
include("Sources.cmake")

if(VN_SDK_EXECUTABLES)

	add_executable(lcevc_core_test_harness ${SOURCES})
	lcevc_set_properties(lcevc_core_test_harness)

	target_link_libraries(lcevc_core_test_harness
		lcevc_dec::integration_utility
		lcevc_dec::core
		lcevc_dec::platform
		lcevc_dec::compiler)

	add_executable(lcevc_dec::core_test_harness ALIAS lcevc_core_test_harness)

	source_group("Source Files" FILES ${SOURCES} ${HEADERS})

	install(TARGETS lcevc_core_test_harness DESTINATION "bin")
endif()
