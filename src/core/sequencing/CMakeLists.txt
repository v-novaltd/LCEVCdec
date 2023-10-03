# Copyright 2022 - V-Nova Ltd.
#
include("Sources.cmake")

add_library(lcevc_dec_core_sequencing STATIC)
add_library(lcevc_dec::sequencing ALIAS lcevc_dec_core_sequencing)
lcevc_set_properties(lcevc_dec_core_sequencing)

target_sources(lcevc_dec_core_sequencing PRIVATE ${SOURCES})

target_link_libraries(lcevc_dec_core_sequencing
	PUBLIC
		lcevc_dec::platform
		lcevc_dec::compiler
)

target_include_directories(lcevc_dec_core_sequencing
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src"
)

install(FILES ${INTERFACES} DESTINATION "include/LCEVC")
