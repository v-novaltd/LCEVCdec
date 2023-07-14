# Copyright 2023 - V-Nova Ltd.
#
include("Sources.cmake")

# lcevc_api (for normal client usage)

add_library(lcevc_dec_api ${SOURCES} ${HEADERS} ${INTERFACES})
add_library(lcevc_dec::api ALIAS lcevc_dec_api)
lcevc_set_properties(lcevc_dec_api)

target_include_directories(lcevc_dec_api
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src"
		"${CMAKE_CURRENT_LIST_DIR}/utility/include"	)

target_compile_definitions(lcevc_dec_api PUBLIC VNInline=inline)
target_compile_definitions(lcevc_dec_api PUBLIC VNEnableLog=1)

target_link_libraries(lcevc_dec_api
	PUBLIC
	lcevc_dec::core
	lcevc_dec::sequencing )


# lcevc_api_static (for unit tests)

add_library(lcevc_dec_api_static STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
add_library(lcevc_dec::api_static ALIAS lcevc_dec_api_static)
lcevc_set_properties(lcevc_dec_api_static)

target_include_directories(lcevc_dec_api_static
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include"
		"${CMAKE_CURRENT_LIST_DIR}/src"
		"${CMAKE_CURRENT_LIST_DIR}/utility/include"	)

target_compile_definitions(lcevc_dec_api_static PUBLIC VNInline=inline)
target_compile_definitions(lcevc_dec_api_static PUBLIC VNEnableLog=1)

target_link_libraries(lcevc_dec_api_static
	PUBLIC
	lcevc_dec::core
	lcevc_dec::sequencing )

# Installation.
#
get_target_property(IS_APPLE_FRAMEWORK lcevc_dec_api FRAMEWORK)
if(IS_APPLE_FRAMEWORK)
	install(TARGETS lcevc_dec_api DESTINATION lib)
else()
	install(TARGETS lcevc_dec_api)
endif()

install(FILES ${INTERFACES} DESTINATION "include/LCEVC")
