# Copyright (c) V-Nova International Limited 2022. All rights reserved.
#
# Find some ffmpeg or libav libraries
#
include (FindPackageHandleStandardArgs)

if(NOT TARGET ffmpeg-libs::ffmpeg-libs)

  add_library(ffmpeg-libs::ffmpeg-libs INTERFACE IMPORTED)
  if(VN_SDK_FFMPEG_LIBS_PACKAGE STREQUAL "custom")
    target_include_directories(ffmpeg-libs::ffmpeg-libs INTERFACE
      "/home/sam/work/ocd/fork-ffmpeg"
      )

    target_link_libraries(ffmpeg-libs::ffmpeg-libs INTERFACE
      "/home/sam/work/ocd/fork-ffmpeg/libavcodec/libavcodec.a"
      "/home/sam/work/ocd/fork-ffmpeg/libavformat/libavformat.a"
      "/home/sam/work/ocd/fork-ffmpeg/libavfilter/libavfilter.a"
      "/home/sam/work/ocd/fork-ffmpeg/libswscale/libswscale.a"
      "/home/sam/work/ocd/fork-ffmpeg/libswresample/libswresample.a"
      "/home/sam/work/ocd/fork-ffmpeg/libavutil/libavutil.a"
      "-lxml2"
    )
  elseif(VN_SDK_FFMPEG_LIBS_PACKAGE STREQUAL "pkgconfig")
    # pkgconfig
    find_package(PkgConfig)
    pkg_check_modules(libavcodec REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(libavformat REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(libavutil REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(libavfilter REQUIRED IMPORTED_TARGET libavfilter)
    pkg_check_modules(libswscale REQUIRED IMPORTED_TARGET libswscale)

    target_link_libraries(ffmpeg-libs::ffmpeg-libs INTERFACE
      PkgConfig::libavcodec
      PkgConfig::libavformat
      PkgConfig::libavutil
      PkgConfig::libavfilter
      PkgConfig::libswscale
    )
  else()
    # conan
    find_package(${VN_SDK_FFMPEG_LIBS_PACKAGE} REQUIRED)

    target_link_libraries(ffmpeg-libs::ffmpeg-libs INTERFACE ${VN_SDK_FFMPEG_LIBS_PACKAGE}::${VN_SDK_FFMPEG_LIBS_PACKAGE} )
  endif()
endif()
