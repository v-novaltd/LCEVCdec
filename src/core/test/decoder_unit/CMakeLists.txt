# Copyright 2022 - V-Nova Ltd.
#
if(VN_SDK_UNIT_TESTS)
    include(Sources.cmake)

    find_package(GTest REQUIRED)
    find_package(range-v3 REQUIRED)

    add_executable(lcevc_core_test_unit)
    lcevc_set_properties(lcevc_core_test_unit)

    target_sources(lcevc_core_test_unit PRIVATE ${SOURCES})

    target_compile_features(lcevc_core_test_unit 
        PRIVATE
            cxx_std_17
    )

    target_link_libraries(lcevc_core_test_unit
        PRIVATE lcevc_dec::core_objects
                lcevc_dec::integration_utility
                lcevc_dec::platform
                lcevc_dec::compiler
                GTest::GTest
                range-v3::range-v3
                #-lgcov
    )

    add_executable(lcevc_dec::core_test_unit ALIAS lcevc_core_test_unit)

    install(TARGETS lcevc_core_test_unit)

    # @todo: remove -lgov.
    # add_test(NAME ${PP_DEC_SHARED_UNIT_NEW_TARGET} COMMAND $<TARGET_FILE:${PP_DEC_SHARED_UNIT_NEW_TARGET}>)
endif()
