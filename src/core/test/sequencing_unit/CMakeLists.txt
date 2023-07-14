# Copyright 2023 - V-Nova Ltd.
#
include(Sources.cmake)

if(VN_SDK_UNIT_TESTS)

    add_executable(lcevc_core_sequencing_test_unit)
    lcevc_set_properties(lcevc_core_sequencing_test_unit)
    
    target_sources(lcevc_core_sequencing_test_unit PRIVATE ${SOURCES})
    
    find_package(GTest REQUIRED CONFIG)
    target_link_libraries(lcevc_core_sequencing_test_unit
        PRIVATE lcevc_dec::sequencing
                GTest::GTest
    )

    add_executable(lcevc_dec::sequencing_test_unit ALIAS lcevc_core_sequencing_test_unit)

    add_compile_definitions(VN_INTEGRATION_OPENGL)

    install(TARGETS lcevc_core_sequencing_test_unit DESTINATION "bin")

endif()
