enable_testing()

# Check if the jenkins environment variables are defined, and if they're
# not then take a good guess at what the paths should be in a local build environment.
if (DEFINED ENV{SOURCE_DIR} AND DEFINED ENV{TARGET_DIR})
	set (TEST_SCRIPT_DIR "$ENV{SOURCE_DIR}/src/func_tests")
	set (TEST_BIN_DIR "$ENV{TARGET_DIR}/bin/")
else()
	set (TEST_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	set (TEST_BIN_DIR "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/bin")
endif()

set (BUILD_TYPE "$ENV{BUILD_TYPE}")
if ("${BUILD_TYPE}" STREQUAL "nightly")
    set (mark "nightly or mr")
else()
    set (mark "mr")
endif()
set (parallelism 6)

if (DEFINED ENV{ADB_PATH})
    set(pytest_command
        Python3::Interpreter -m pytest ${TEST_SCRIPT_DIR}/tests/
               --bin-dir ${TEST_BIN_DIR}
               --resource-dir $ENV{ASSETS}
               --json-report
               --json-report-file dec-aarch64-pytest-results.json
               -k "test_integration_gpu_tolerance[Cactus_CONF-1_v-nova_v03-buffer-residuals] or \
                   test_integration_gpu_tolerance[Cactus_CONF-2_v-nova_v03-texture-residuals] or \
                   test_integration_gpu_tolerance[Cactus_TT-1_v-nova_v03-buffer-residuals] or \
                   test_integration_gpu_tolerance[Cactus_QUANT-5-3_v-nova_v03-geometry-residuals] or \
                   test_integration_gpu_tolerance[Cactus_UPS-1_v-nova_v03-texture-residuals] or \
                   test_integration_gpu_tolerance[Cactus_ECS-1_v-nova_v05-buffer-residuals] or \
                   test_integration_gpu_tolerance[Cactus_HDUD-4_v-nova_v03-buffer-residuals] or \
                   test_integration_gpu_tolerance[CanYouReadThis_QUANT-1-5_v-nova_v03-texture-residuals] or \
                   test_integration_gpu_tolerance[Cactus_MP_SET-1_v-nova_v03-buffer-residuals] or \
                   test_integration_gpu_tolerance[ParkRunning3_10bit_CONF-1_v-nova_v03-buffer-residuals] or \
                   test_integration_gpu_tolerance[Cactus_QUANT-5-2_v-nova_v03-texture-residuals]"
    )
else()
    set(pytest_command
        Python3::Interpreter -m pytest ${TEST_SCRIPT_DIR}/tests/
               --bin-dir ${TEST_BIN_DIR}
               --resource-dir $ENV{ASSETS}
               -rfE
               -n ${parallelism}
               --basetemp cpu
               --json-report
               --json-report-file dec-x86-pytest-results.json
               -m ${mark}
    )
endif()

add_test(
	NAME funcTests
	COMMAND ${pytest_command}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
