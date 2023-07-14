# Copyright 2023 - V-Nova Ltd.
#
if(VN_SDK_UNIT_TESTS)
    include(Sources.cmake)

    find_package(GTest REQUIRED)

    add_executable(lcevc_api_test_unit)
    add_executable(lcevc_dec::api_test_unit ALIAS lcevc_api_test_unit)
    target_sources(lcevc_api_test_unit PRIVATE ${SOURCES} ${HEADERS})
    lcevc_set_properties(lcevc_api_test_unit)

    # Unclear what compiler and platform do, but other unit tests have them.
    target_link_libraries(lcevc_api_test_unit
        PRIVATE lcevc_dec::api_static
                lcevc_dec::compiler
                lcevc_dec::platform
                GTest::GTest
    )

	install(TARGETS lcevc_api_test_unit DESTINATION "bin")

endif()
