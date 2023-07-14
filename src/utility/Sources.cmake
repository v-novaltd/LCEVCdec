# Copyright 2022 - V-Nova Ltd.
#
list(APPEND SOURCES
	"src/base_decoder.cpp"
	"src/base_decoder_libav.cpp"
	"src/base_decoder_bin.cpp"
	"src/bin_reader.cpp"
	"src/check.cpp"
	"src/configure.cpp"
	"src/extract.c"
	"src/get_program_dir.cpp"
	"src/md5.cpp"
	"src/parse_raw_name.cpp"
	"src/picture_functions.cpp"
	"src/picture_layout.cpp"
	"src/picture_lock.cpp"
	"src/raw_reader.cpp"
	"src/raw_writer.cpp"
	"src/string_utils.cpp"
	"src/types_convert.cpp"
	"src/types_cli11.cpp"
	"src/types_stream.cpp"
	)

list(APPEND HEADERS
	"src/bin_reader.h"
	"src/parse_raw_name.h"
	"src/string_utils.h"
	)

list(APPEND INTERFACES
	"include/LCEVC/utility/base_decoder.h"
	"include/LCEVC/utility/byte_order.h"
	"include/LCEVC/utility/check.h"
	"include/LCEVC/utility/configure.h"
	"include/LCEVC/utility/extract.h"
	"include/LCEVC/utility/get_program_dir.h"
	"include/LCEVC/utility/picture_functions.h"
	"include/LCEVC/utility/picture_layout.h"
	"include/LCEVC/utility/picture_layout_inl.h"
	"include/LCEVC/utility/raw_reader.h"
	"include/LCEVC/utility/raw_writer.h"
	"include/LCEVC/utility/types_cli11.h"
	"include/LCEVC/utility/types_convert.h"
	"include/LCEVC/utility/types_fmt.h"
	"include/LCEVC/utility/types.h"
	"include/LCEVC/utility/types_stream.h"
   )

# Android NDK 21.4 is missing object files for libc++ std::filesystem.
#
# Include them here explicitly
#
if(ANDROID AND (ANDROID_NDK_MAJOR LESS_EQUAL 21) OR ($ENV{CONAN_CMAKE_ANDROID_NDK} MATCHES "/21\.4\.7075529$") )

message(STATUS "Using Android NDK 21: Adding missing libc++ std::filesystem sources.")
list(APPEND HEADERS
	"src/android_filesystem/src/filesystem/filesystem_common.h"
	"src/android_filesystem/src/include/apple_availability.h"
	)

list(APPEND SOURCES
	"src/android_filesystem/src/filesystem/int128_builtins.cpp"
	"src/android_filesystem/src/filesystem/directory_iterator.cpp"
	"src/android_filesystem/src/filesystem/operations.cpp"
	)
endif()

set(ALL_FILES ${SOURCES} ${HEADERS} ${INTERFACES} "Sources.cmake")

# IDE groups
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_FILES})
