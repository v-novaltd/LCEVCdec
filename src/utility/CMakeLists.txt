# Copyright 2023 - V-Nova Ltd.
#
include("Sources.cmake")

find_package(RapidJSON NAMES RapidJSON rapidjson REQUIRED)
find_package(fmt REQUIRED)
find_package(span-lite REQUIRED)
find_package(ffmpeg-libs REQUIRED)
if(VN_SDK_USE_GHC_FILESYSTEM)
	find_package(ghc_filesystem REQUIRED)
endif()

add_library(lcevc_dec_utility STATIC ${SOURCES} ${HEADERS} ${INTERFACES})
lcevc_set_properties(lcevc_dec_utility)

target_include_directories(lcevc_dec_utility PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_include_directories(lcevc_dec_utility PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../api/include")

target_link_libraries(lcevc_dec_utility
	PUBLIC
	lcevc_dec::platform
	lcevc_dec::compiler
	ffmpeg-libs::ffmpeg-libs
	PRIVATE
	nonstd::span-lite
	fmt::fmt
	$<TARGET_NAME_IF_EXISTS:rapidjson::rapidjson>
	$<TARGET_NAME_IF_EXISTS:RapidJSON::RapidJSON>
	$<TARGET_NAME_IF_EXISTS:ghcFilesystem::ghc_filesystem>
	)

add_library(lcevc_dec::utility ALIAS lcevc_dec_utility)

install(TARGETS lcevc_dec_utility DESTINATION lib)
install(FILES ${INTERFACES} DESTINATION include/LCEVC/utility)
install(FILES ${SOURCES} ${HEADERS} DESTINATION src/utility)
