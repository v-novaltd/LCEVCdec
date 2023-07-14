# Copyright 2022 - V-Nova Ltd.

include("Sources.cmake")

add_library(lcevc_dec_overlay_images STATIC ${SOURCES})
lcevc_set_properties(lcevc_dec_overlay_images)

target_include_directories(lcevc_dec_overlay_images PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(lcevc_dec_overlay_images
	PUBLIC
	lcevc_dec::platform
	lcevc_dec::compiler)

add_library(lcevc_dec::overlay_images ALIAS lcevc_dec_overlay_images)

if(NOT BUILD_SHARED_LIBS)
	install(TARGETS lcevc_dec_overlay_images DESTINATION lib)
endif()
