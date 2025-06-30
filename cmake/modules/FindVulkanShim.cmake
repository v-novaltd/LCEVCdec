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

# A meta package that looks for conan, native CMake, or pkg-config

if (NOT TARGET VulkanShim::VulkanShim)
    add_library(VulkanShim::VulkanShim INTERFACE IMPORTED)

    if (ANDROID)
        # Use the CMake package
        find_package(Vulkan)
        if (TARGET Vulkan::Vulkan)
            message(STATUS "Found Vulkan packages via default CMake package")
            target_link_libraries(VulkanShim::VulkanShim INTERFACE Vulkan::Vulkan)
        else ()
            message(FATAL_ERROR "Cannot find Android Vulkan")
        endif ()
    else ()
        # Try the conan package names
        find_package("vulkan-loader" QUIET)
        find_package("vulkan-headers" QUIET)

        if (TARGET vulkan-loader::vulkan-loader AND TARGET vulkan-headers::vulkan-headers)
            message(STATUS "Found Vulkan packages via find_package")
            target_link_libraries(VulkanShim::VulkanShim INTERFACE vulkan-loader::vulkan-loader
                                                                   vulkan-headers::vulkan-headers)
        else ()
            # Try pkg-config
            find_package(PkgConfig)
            pkg_check_modules(Vulkan vulkan)

            if (Vulkan_FOUND)
                # Populate interface library from package vars
                message(STATUS "Found Vulkan packages via pkg-config")

                set_property(TARGET VulkanShim::VulkanShim PROPERTY INTERFACE_VERSION
                                                                    ${Vulkan_VERSION})
                set_property(TARGET VulkanShim::VulkanShim PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                                    "${Vulkan_INCLUDE_DIRS}")
                set_property(TARGET VulkanShim::VulkanShim PROPERTY INTERFACE_LINK_LIBRARIES
                                                                    "${Vulkan_LIBRARIES}")
                set_property(TARGET VulkanShim::VulkanShim PROPERTY INTERFACE_LINK_DIRECTORIES
                                                                    "${Vulkan_LIBRARY_DIRS}")
                set_property(
                    TARGET VulkanShim::VulkanShim
                    PROPERTY INTERFACE_COMPILE_OPTIONS "${Vulkan_CFLAGS}" "${Vulkan_CFLAGS_OTHER}")
            else ()
                find_package(Vulkan)
                if (TARGET Vulkan::Vulkan)
                    message(STATUS "Found Vulkan packages via default CMake package")
                    target_link_libraries(VulkanShim::VulkanShim INTERFACE Vulkan::Vulkan)
                else ()
                    message(FATAL_ERROR "Cannot find Vulkan packages")
                endif ()
            endif ()
        endif ()
    endif ()
endif ()
