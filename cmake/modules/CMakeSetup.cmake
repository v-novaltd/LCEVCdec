# Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

# State that needs to be set before calling project() or add_xxxx()

# The rest of the target specific setup happens after 'project()'
set(CMAKE_PROJECT_INCLUDE "${CMAKE_SOURCE_DIR}/cmake/modules/CMakeProject.cmake")

# Local modules, usually from conan CMake generators - searched first
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

# Build hygiene
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "No in-source builds - use a build directory - see docs/building.md.")
endif ()

# Default build type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug" "MinSizeRel"
                                                 "RelWithDebInfo")
endif ()

# Binary destination
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)

# Look for "Config" packages before "Module" ones.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Packages for build
find_package(Git REQUIRED)
find_package(Python3 REQUIRED)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Git version variables - either from files (conan & ci), or from git
#
if (EXISTS "${CMAKE_SOURCE_DIR}/.githash")
    file(READ "${CMAKE_SOURCE_DIR}/.githash" GIT_HASH)
    file(READ "${CMAKE_SOURCE_DIR}/.gitlonghash" GIT_LONG_HASH)
    file(READ "${CMAKE_SOURCE_DIR}/.gitdate" GIT_DATE)
    file(READ "${CMAKE_SOURCE_DIR}/.gitbranch" GIT_BRANCH)
    file(READ "${CMAKE_SOURCE_DIR}/.gitversion" GIT_VERSION)
    file(READ "${CMAKE_SOURCE_DIR}/.gitshortversion" GIT_SHORT_VERSION)
else ()
    execute_process(
        COMMAND git rev-parse --verify --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --verify --short=10 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_LONG_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND git log -1 --format=%cd --date=format:%Y-%m-%d
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND git describe --match "*.*.*" --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND git describe --match "*.*.*" --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_SHORT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif ()

if (GIT_VERSION STREQUAL ""
    OR GIT_SHORT_VERSION STREQUAL ""
    OR GIT_HASH STREQUAL "")
    message(WARNING "Cannot determine git version, please clone from a git repo")
else ()
    message(STATUS "Git: Version=${GIT_VERSION} ShortVersion=${GIT_SHORT_VERSION} Hash=${GIT_HASH}")
endif ()

# Default MSVC runtime library, this has to before the 'project()' command
cmake_policy(SET CMP0091 NEW)
if (VN_MSVC_RUNTIME_STATIC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# Function to generate component version info from git into: <TARGET_NAME>.<SRC_SUFFIX>
# <TARGET_NAME>.h <TARGET_NAME>.rc
#
function (
    lcevc_version_files
    TARGET
    COMPONENT
    NAME
    BINARY
    TYPE
    DESC
    SRC_SUFFIX)
    set(PATH_SRC "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SRC_SUFFIX}")
    file(TOUCH "${PATH_SRC}")

    set(PATH_H "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.h")
    file(TOUCH "${PATH_H}")

    set(TARGET_SOURCES "${PATH_H}" "${PATH_SRC}")

    if (MSVC)
        set(PATH_RC "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.rc")
        file(TOUCH "${PATH_RC}")
        set(OPT_RC "--output_rc=${PATH_RC}")
        list(APPEND TARGET_SOURCES "${PATH_RC}")
    endif ()

    # Target for dependants
    add_library(${TARGET} INTERFACE)
    target_sources(${TARGET} INTERFACE ${TARGET_SOURCES})
    target_include_directories(${TARGET} INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

    if (VN_SDK_BUILD_DETAILS)
        # Add build details to the target
        if (EXISTS "${CMAKE_SOURCE_DIR}/.githash")
            set(OPT_GIT_VERSION "--git_version=${GIT_VERSION}")
            set(OPT_GIT_HASH "--git_hash=${GIT_HASH}")
            set(OPT_GIT_DATE "--git_date=${GIT_DATE}")
        endif ()

        source_group("version_info" FILES ${TARGET_SOURCES})
        add_custom_target(
            ${TARGET}_generate
            DEPENDS "${CMAKE_SOURCE_DIR}/cmake/tools/version_files.py"
            SOURCES "${CMAKE_SOURCE_DIR}/cmake/tools/version_files.py"
            COMMAND
                Python3::Interpreter "${CMAKE_SOURCE_DIR}/cmake/tools/version_files.py" --component
                "${COMPONENT}" --name "${NAME}" --output_src "${PATH_SRC}" --output_h "${PATH_H}"
                --binary_name "${BINARY}" --binary_type "${TYPE}" --description "${DESC}"
                "${OPT_RC}" "${OPT_GIT_VERSION}" "${OPT_GIT_HASH}" "${OPT_GIT_DATE}"
            BYPRODUCTS ${TARGET_SOURCES})
        set_target_properties(${TARGET}_generate PROPERTIES FOLDER "Version Files")
        add_dependencies(${TARGET} ${TARGET}_generate)
        set_target_properties(${TARGET} PROPERTIES FOLDER "Version Files")
    endif ()

    add_library(lcevc_dec::${TARGET} ALIAS ${TARGET})

endfunction ()

function (lcevc_set_properties TARGET)
    get_target_property(TARGET_TYPE ${TARGET} TYPE)

    if (TARGET_TYPE STREQUAL EXECUTABLE)
        set_target_properties(${TARGET} PROPERTIES FOLDER "Executables")

        if (((TARGET_PLATFORM STREQUAL "Android") OR (TARGET_PLATFORM STREQUAL "Linux")))
            # Set up ld.so RPATH on out test executables so that they can can find libav shared
            # libraries transitively - DT_RPATH vs. DT_RUNPATH
            set_target_properties(${TARGET} PROPERTIES BUILD_RPATH_USE_ORIGIN true
                                                       LINK_OPTIONS "-Wl,--disable-new-dtags")
        endif ()

    else ()
        set_target_properties(${TARGET} PROPERTIES FOLDER "Libraries")
        if ((IOS OR MACOS) AND BUILD_SHARED_LIBS)
            set_target_properties(${TARGET} PROPERTIES FRAMEWORK TRUE MACOSX_FRAMEWORK_IDENTIFIER
                                                                      "com.v-nova.${TARGET}")
        endif ()

        if ((TARGET_TYPE STREQUAL "SHARED_LIBRARY")
            AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/exports.txt"
            AND ((TARGET_PLATFORM STREQUAL "Android") OR (TARGET_PLATFORM STREQUAL "Linux")))
            # Restrict symbols exported by shared library
            target_link_options(${TARGET} PRIVATE
                                -Wl,--version-script,${CMAKE_CURRENT_LIST_DIR}/exports.txt)
        endif ()

    endif ()

    if (VN_STRIP_RELEASE
        AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release"
        AND NOT "${CMAKE_GENERATOR}" MATCHES "^Visual Studio.*")
        # Note: "stripped" is the default on MSVC Release builds, and it doesn't recognize -s. But,
        # if you DID want to handle this, you couldn't use "CMAKE_BUILD_TYPE", since MSVC doesn't
        # use that to determine Release vs Debug builds. Instead it uses `--config` at build time
        # (and it defaults to `--config Debug`)
        target_link_options(${TARGET} PRIVATE -s)
    endif ()

    target_compile_features(${TARGET} PRIVATE cxx_std_17)
    target_compile_features(${TARGET} PRIVATE c_std_11)

    set_target_properties(
        ${TARGET}
        PROPERTIES C_STANDARD 11
                   CXX_STANDARD 17
                   VS_GLOBAL_EnableMicrosoftCodeAnalysis false
                   VS_GLOBAL_EnableClangTidyCodeAnalysis true)

endfunction ()

macro (lcevc_add_subdirectory DIR)
    if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}")
        add_subdirectory("${DIR}")
    endif ()
endmacro ()

macro (lcevc_add_subdirectory_if DIR PREDICATE)
    if ((${PREDICATE}) AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}")
        add_subdirectory("${DIR}")
    endif ()
endmacro ()

function (lcevc_install PROJECT_TARGET)
    include(GNUInstallDirs)

    get_target_property(IS_APPLE_FRAMEWORK "${PROJECT_TARGET}" FRAMEWORK)
    if (IS_APPLE_FRAMEWORK)
        install(TARGETS ${PROJECT_TARGET} DESTINATION lib)
    else ()
        install(TARGETS ${PROJECT_TARGET})
    endif ()
endfunction ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
