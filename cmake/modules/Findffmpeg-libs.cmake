# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

# Find some ffmpeg or libav libraries
#
include(FindPackageHandleStandardArgs)

if (NOT TARGET ffmpeg-libs::ffmpeg-libs)
    add_library(ffmpeg-libs::ffmpeg-libs INTERFACE IMPORTED)

    # Search for libav packages, first from conan via package names 'ffmpeg-libs-minimal' and then
    # 'ffmpeg', then search via pkg-config, then finally using the set path
    find_package("ffmpeg-libs-minimal" QUIET)
    find_package("ffmpeg" QUIET)
    get_cmake_property(PACKAGE_LIST PACKAGES_FOUND)
    list(
        APPEND
        REQUIRED_LIBAV_LIBS
        "libavcodec"
        "libavformat"
        "libavfilter"
        "libavutil"
        "libavdevice")
    if ("ffmpeg-libs-minimal" IN_LIST PACKAGE_LIST)
        message("Found libav packages from conan package 'ffmpeg-libs-minimal'")
        target_link_libraries(ffmpeg-libs::ffmpeg-libs
                              INTERFACE ffmpeg-libs-minimal::ffmpeg-libs-minimal)
        set(FFMPEG_SHARED_PATH
            "${ffmpeg-libs-minimal_LIB_DIRS_DEBUG}${ffmpeg-libs-minimal_LIB_DIRS_RELEASE}")
        if (WIN32)
            set(FFMPEG_SHARED_PATH
                "${ffmpeg-libs-minimal_LIB_DIRS_DEBUG}${ffmpeg-libs-minimal_LIB_DIRS_RELEASE}/../bin"
            )
        endif ()
        file(COPY ${FFMPEG_SHARED_PATH} DESTINATION "${CMAKE_BINARY_DIR}")
    elseif ("ffmpeg" IN_LIST PACKAGE_LIST)
        message("Found libav packages from conan package 'ffmpeg'")
        target_link_libraries(ffmpeg-libs::ffmpeg-libs INTERFACE ffmpeg::ffmpeg)
        set(FFMPEG_SHARED_PATH "${ffmpeg_LIBS_RELEASE}${ffmpeg_LIBS_DEBUG}")
        if (WIN32)
            set(FFMPEG_SHARED_PATH "${ffmpeg_LIBS_RELEASE}${ffmpeg_LIBS_DEBUG}/../bin")
        endif ()
        file(COPY ${FFMPEG_SHARED_PATH} DESTINATION "${CMAKE_BINARY_DIR}")
    else ()
        # Attempt to find via pkgconfig
        find_package(PkgConfig)
        foreach (_LIB ${REQUIRED_LIBAV_LIBS})
            pkg_check_modules(${_LIB} IMPORTED_TARGET ${_LIB})
        endforeach ()

        target_link_libraries(
            ffmpeg-libs::ffmpeg-libs INTERFACE PkgConfig::libavcodec PkgConfig::libavformat
                                               PkgConfig::libavutil PkgConfig::libavfilter)
        if (NOT libavcodec_FOUND
            OR NOT libavformat_FOUND
            OR NOT libavutil_FOUND
            OR NOT libavfilter_FOUND)
            # Still not found, so assume it's a custom file path
            target_include_directories(ffmpeg-libs::ffmpeg-libs
                                       INTERFACE ${VN_SDK_FFMPEG_LIBS_PACKAGE})

            target_link_libraries(
                ffmpeg-libs::ffmpeg-libs
                INTERFACE "${VN_SDK_FFMPEG_LIBS_PACKAGE}/libavcodec/libavcodec.a"
                          "${VN_SDK_FFMPEG_LIBS_PACKAGE}/libavformat/libavformat.a"
                          "${VN_SDK_FFMPEG_LIBS_PACKAGE}/libavfilter/libavfilter.a"
                          "${VN_SDK_FFMPEG_LIBS_PACKAGE}/libavutil/libavutil.a"
                          "-lxml2")
            if (libavcodec_FOUND
                AND libavformat_FOUND
                AND libavutil_FOUND
                AND libavfilter_FOUND)
                message("Found libav packages from path: ${VN_SDK_FFMPEG_LIBS_PACKAGE}")
            else ()
                message(
                    FATAL_ERROR
                        "Could not find libav packages, either install from conan, pkg-config or manually specify a path with VN_SDK_FFMPEG_LIBS_PACKAGE"
                )
            endif ()
        else ()
            message("Found libav packages from pkg-config")
        endif ()
    endif ()
endif ()
