# Copyright (c) V-Nova International Limited 2025. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
# software may be incorporated into a project under a compatible license provided the requirements
# of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
# licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
# ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
# THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

include("Sources.cmake")

add_library(lcevc_dec_emscripten_wrapper STATIC ${SOURCES} ${INTERFACES})
lcevc_set_properties(lcevc_dec_emscripten_wrapper)

target_include_directories(lcevc_dec_emscripten_wrapper PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(
    lcevc_dec_emscripten_wrapper PUBLIC lcevc_dec::platform lcevc_dec::compiler lcevc_dec::common
                                        lcevc_dec::enhancement lcevc_dec::pixel_processing)

add_library(lcevc_dec::emscripten_wrapper ALIAS lcevc_dec_emscripten_wrapper)

# -------------------------------------------------------------------------------------------------

set(WASM_EXPORT_FUNCTIONS "_free" "_malloc" "_demo")
string(REPLACE ";" "," WASM_EXPORT_FUNCTIONS "${WASM_EXPORT_FUNCTIONS}")

set(OUTPUT_PREFIX ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}${CMAKE_STATIC_LIBRARY_PREFIX})
set(LIB_PREFIX ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}${CMAKE_STATIC_LIBRARY_PREFIX})
file(MAKE_DIRECTORY ${OUTPUT_PREFIX})

set(WASM_LIB_DEPENDENCIES "${LIB_PREFIX}lcevc_dec_emscripten_wrapper${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(WASM_OUTPUT_JS_FILE ${OUTPUT_PREFIX}lcevc_dec.js)
set(WASM_OUTPUT_WASM_FILE ${OUTPUT_PREFIX}lcevc_dec.wasm)

list(
    APPEND
    EMCC_ARGS
    -sMODULARIZE=1
    -sEXPORT_NAME=LCEVCdecModule
    -sENVIRONMENT=web,worker
    -sALLOW_MEMORY_GROWTH=1
    -msimd128
    -msse4.1)
if (VN_SDK_TRACING)
    list(APPEND EMCC_ARGS --memoryprofiler --tracing -sEMSCRIPTEN_TRACING=1)
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(
        APPEND
        EMCC_ARGS
        -g3
        -fdebug-compilation-dir=.
        -sASSERTIONS=2
        -sSAFE_HEAP=1
        -sSTACK_OVERFLOW_CHECK=1)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND EMCC_ARGS -O3 --closure 1)
else ()
    message(FATAL_ERROR "Unsupported build type for emscripten: ${CMAKE_BUILD_TYPE}")
endif ()

add_custom_target(
    emscripten ALL
    DEPENDS lcevc_dec::emscripten_wrapper
    COMMAND emcc ${WASM_LIB_DEPENDENCIES} -sEXPORTED_FUNCTIONS=${WASM_EXPORT_FUNCTIONS} ${EMCC_ARGS}
            -o ${WASM_OUTPUT_JS_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Linking lcevc_dec.wasm with emcc")

install(FILES ${WASM_OUTPUT_JS_FILE} ${WASM_OUTPUT_WASM_FILE} DESTINATION ${CMAKE_INSTALL_BINDIR})
if (NOT VN_SDK_SYSTEM_INSTALL)
    install(FILES ${WASM_OUTPUT_JS_FILE} ${WASM_OUTPUT_WASM_FILE}
                  ${CMAKE_CURRENT_LIST_DIR}/sample.html DESTINATION "sample")
endif ()
