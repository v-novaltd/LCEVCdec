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

# Find correct pkg-config libs
if (NOT BUILD_SHARED_LIBS)
    if (CMAKE_SYSTEM_NAME STREQUAL "Android")
        set(PC_LIBS "-lc++ -llog")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PC_LIBS "-lstdc++")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Further check if libc++ is used instead of libstdc++
        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles(
            "
        #include <cstddef>
        int main() {
            #if defined(_LIBCPP_VERSION)
            return 0;
            #else
            return 1;
            #endif
        }"
            USING_LIBCXX)

        if (USING_LIBCXX)
            set(PC_LIBS "-lc++")
        else ()
            set(PC_LIBS "-lstdc++")
        endif ()
    endif ()
endif ()

list(APPEND PC_LIBS "-lm")

if (VN_SDK_API_LAYER)
    if (BUILD_SHARED_LIBS)
        list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_api_utility")
    else ()
        list(APPEND PC_LIBS "-llcevc_dec_api_utility")
    endif ()
endif ()
if (VN_SDK_EXECUTABLES OR VN_SDK_UNIT_TESTS)
    if (BUILD_SHARED_LIBS)
        list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_utility")
    else ()
        list(APPEND PC_LIBS "-llcevc_dec_utility")
    endif ()
endif ()
if (BUILD_SHARED_LIBS)
    list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_common")
else ()
    list(APPEND PC_LIBS "-llcevc_dec_common")
endif ()
if (VN_SDK_PIPELINE_CPU
    OR VN_SDK_PIPELINE_VULKAN
    OR VN_SDK_PIPELINE_LEGACY)
    if (BUILD_SHARED_LIBS)
        list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_pipeline")
    else ()
        list(APPEND PC_LIBS "-llcevc_dec_pipeline")
    endif ()
endif ()
if (BUILD_SHARED_LIBS)
    list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_enhancement -llcevc_dec_pixel_processing")
else ()
    list(APPEND PC_LIBS "-llcevc_dec_enhancement -llcevc_dec_pixel_processing")
endif ()
if (VN_SDK_PIPELINE_CPU)
    list(APPEND PC_LIBS "-llcevc_dec_pipeline_cpu")
endif ()
if (VN_SDK_PIPELINE_VULKAN)
    list(APPEND PC_LIBS "-llcevc_dec_pipeline_vulkan")
endif ()
if (VN_SDK_PIPELINE_LEGACY)
    if (BUILD_SHARED_LIBS)
        list(APPEND PC_LIBS_PRIVATE "-llcevc_dec_sequencer")
    else ()
        list(APPEND PC_LIBS "-llcevc_dec_sequencer")
    endif ()
    list(APPEND PC_LIBS "-llcevc_dec_legacy -llcevc_dec_pipeline_legacy")
endif ()
if (VN_SDK_API_LAYER)
    list(APPEND PC_LIBS "-llcevc_dec_api")
endif ()

if (VN_SDK_BASE_DECODER)
    list(APPEND PC_REQUIRES_PRIVATE "libavcodec libavformat libavfilter libavutil")
    if (VN_SDK_EXECUTABLES)
        list(APPEND PC_REQUIRES_PRIVATE "xxhash cli11 fmt")
    endif ()
    if (VN_SDK_UNIT_TESTS)
        list(APPEND PC_REQUIRES_PRIVATE "libgtest libgmock")
    endif ()
endif ()
if (VN_SDK_PIPELINE_VULKAN)
    list(APPEND PC_REQUIRES_PRIVATE "vulkan-loader vulkan-headers")
endif ()

string(REPLACE ";" " " PC_LIBS "${PC_LIBS}")
if (BUILD_SHARED_LIBS)
    string(REPLACE ";" " " PC_LIBS_PRIVATE "${PC_LIBS_PRIVATE}")
    set(PC_LIBS_PRIVATE "Libs.private: ${PC_LIBS_PRIVATE}")
endif ()
if (PC_REQUIRES_PRIVATE)
    string(REPLACE ";" " " PC_REQUIRES_PRIVATE "${PC_REQUIRES_PRIVATE}")
    set(PC_REQUIRES_PRIVATE "Requires.private: ${PC_REQUIRES_PRIVATE}")
endif ()

# Configure templates for install
configure_file("cmake/templates/build_config.h.in" "generated/LCEVC/build_config.h")
configure_file("cmake/templates/README.md.in" "install_config/README.md")
configure_file("cmake/templates/lcevc_dec.pc.in" "generated/lcevc_dec.pc" @ONLY)

# Install templates and static files
install(FILES "${CMAKE_BINARY_DIR}/install_config/README.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES "${CMAKE_BINARY_DIR}/generated/lcevc_dec.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
install(FILES "${CMAKE_BINARY_DIR}/generated/LCEVC/build_config.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC")
install(FILES "include/LCEVC/api_defs.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LCEVC")
if (VN_SDK_SYSTEM_INSTALL)
    install(FILES "COPYING" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")
    install(FILES "LICENSE.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")
else ()
    install(FILES "COPYING" DESTINATION "licenses")
    install(DIRECTORY "licenses" DESTINATION ".")
    install(FILES "LICENSE.md" DESTINATION "licenses/LCEVCdec")
endif ()

# Package install into a ZIP file
set(CPACK_PACKAGE_VERSION ${GIT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
