# Copyright 2022 - V-Nova Ltd.
#

## API Version
# XXX use SDK semver major instead?
#
set(VN_CORE_API_VERSION 17 PARENT_SCOPE)

lcevc_version_files(core_version "DPI" "lcevc_dec-core" "lcevc_dec_core" "dll" "LCEVC Decoder Core" c)
include("Sources.cmake")

#-------------------------------------------------------------------------------
# Library of objects for use by tests.

add_library(lcevc_dec_core_objects OBJECT)
lcevc_set_properties(lcevc_dec_core_objects)

target_sources(lcevc_dec_core_objects PRIVATE ${SOURCES})

target_include_directories(lcevc_dec_core_objects
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include"
		"${CMAKE_CURRENT_LIST_DIR}/src"
)

target_compile_definitions(lcevc_dec_core_objects
	PRIVATE
		VNEnablePublicAPIExport=1
)

target_link_libraries(lcevc_dec_core_objects
	PUBLIC
		lcevc_dec::platform
		lcevc_dec::compiler
	PRIVATE
		lcevc_dec::core_version
		lcevc_dec::overlay_images
)

add_library(lcevc_dec::core_objects ALIAS lcevc_dec_core_objects)

#-------------------------------------------------------------------------------

add_library(lcevc_dec_core)
lcevc_set_properties(lcevc_dec_core)

target_sources(lcevc_dec_core PRIVATE ${SOURCES})

target_include_directories(lcevc_dec_core
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src"
)

target_compile_definitions(lcevc_dec_core
	PRIVATE
		VNEnablePublicAPIExport=1
)

target_link_libraries(lcevc_dec_core
	PUBLIC
		lcevc_dec::compiler
		lcevc_dec::platform
	PRIVATE
		lcevc_dec::core_version
		lcevc_dec::overlay_images
)

add_library(lcevc_dec::core ALIAS lcevc_dec_core)

if(EMSCRIPTEN)
	target_compile_definitions(lcevc_dec_core
		PRIVATE
		VNEmscriptenBuild)

	# Compile the Javascript bytecode output into a .js file.
	set(LCEVC_CORE_EXPORT_FUNCTIONS
		"_perseus_decoder_parse"
		"_perseus_decoder_open_wrapper"
		"_perseus_decoder_close"
		"_perseus_decoder_decode_wrapper"
		"_perseus_decoder_decode_base_wrapper"
		"_perseus_decoder_decode_high_wrapper"
		"_perseus_decoder_get_surface"
		"_perseus_decoder_get_surface_size"
		"_perseus_decoder_clear_temporal"
		"_perseus_decoder_get_dither_strength"
		"_perseus_decoder_get_dither_type"
		"_perseus_decoder_get_base_hash"
		"_perseus_decoder_upscale_wrapper"
		"_perseus_decoder_parse_wrapper"
		"_perseus_get_version"
		"_perseus_start_tracing"
		"_perseus_end_tracing"
		"_perseus_decoder_get_last_error_wrapper"
		"_DPIVersion"
		"_DPIVersionFull"
		"_DPIHash"
		"_DPIHashShort" )

	set(LCEVC_CORE_BC_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}${CMAKE_STATIC_LIBRARY_PREFIX}lcevc_dec_core${CMAKE_STATIC_LIBRARY_SUFFIX})
	set(LCEVC_CORE_JS_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}${CMAKE_STATIC_LIBRARY_PREFIX}lcevc_dec_core.js)
	set(LCEVC_CORE_WASM_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}${CMAKE_STATIC_LIBRARY_PREFIX}lcevc_dec_core.wasm)
	set(LCEVC_CORE_PRE_JS_FILE ${CMAKE_CURRENT_LIST_DIR}/emscripten/init.js)
	set(LCEVC_CORE_WORKER_JS_FILE
		"${CMAKE_CURRENT_LIST_DIR}/emscripten/worker_utils.js"
		"${CMAKE_CURRENT_LIST_DIR}/emscripten/liblcevc_worker_impl.js")
	set(LCEVC_CORE_LICENSE_JS_FILE ${CMAKE_CURRENT_LIST_DIR}/emscripten/license.js)

	set(EMS_EXTRA_ARGS "")
	if(PP_ENABLE_EMS_TRACING)
		set(EMS_EXTRA_ARGS ${EMS_EXTRA_ARGS} "--tracing")
	endif()
	if (DEBUG)
		set(EMS_EXTRA_ARGS ${EMS_EXTRA_ARGS} "--debug")
	endif ()

	add_custom_command(
		TARGET lcevc_dec_core POST_BUILD
		COMMAND Python3::Interpreter
		${PROJECT_SOURCE_DIR}/cmake/tools/link_bytecode_to_js.py
			-i ${LCEVC_CORE_BC_FILE}
			-o ${LCEVC_CORE_JS_FILE}
			-f "\"${LCEVC_CORE_EXPORT_FUNCTIONS}\""
			-p ${LCEVC_CORE_PRE_JS_FILE}
			-w "\"${LCEVC_CORE_WORKER_JS_FILE}\""
			-l ${LCEVC_CORE_LICENSE_JS_FILE}
			-t ${CMAKE_BUILD_TYPE} ${EMS_EXTRA_ARGS}
			--link-library lcevc_dec_overlay_images
		BYPRODUCTS ${LCEVC_CORE_JS_FILE} ${LCEVC_CORE_WORKER_JS_FILE})

	install(FILES ${LCEVC_CORE_JS_FILE} ${LCEVC_CORE_WASM_FILE} DESTINATION "bin")
endif()

#-------------------------------------------------------------------------------

get_target_property(IS_APPLE_FRAMEWORK lcevc_dec_core FRAMEWORK)
if(IS_APPLE_FRAMEWORK)
	install(TARGETS lcevc_dec_core DESTINATION lib)
else()
	install(TARGETS lcevc_dec_core)
endif()

install(FILES ${INTERFACES} DESTINATION "include/LCEVC")

#-------------------------------------------------------------------------------

add_custom_target(ClangTidyDPI
    COMMAND 
        python ${CMAKE_SOURCE_DIR}/cmake/tools/clang_tidy_run.py -s . -b ${CMAKE_BINARY_DIR}/clang_tidy_run/ --without_colour -f .+src\/core\/.+
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_target_properties(ClangTidyDPI
    PROPERTIES
        FOLDER "Support")