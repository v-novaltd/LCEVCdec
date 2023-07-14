# Copyright 2022 - V-Nova Ltd.

# CMAKE 3.17.1 and android breaks this -so EGL has been moved to platform libraries on Android
#

include (FindPackageHandleStandardArgs)

find_library(egl_LIBRARIES NAMES EGL DOC "The EGL library")

# find_package_handle_standard_args(egl REQUIRE_VARS egl_LIBRARIES)

mark_as_advanced(egl_LIBRARIES)

if(egl_FOUND AND NOT TARGET egl::egl)
	message(STATUS "EGL")

	add_library(egl::egl INTERFACE IMPORTED)
	set_target_properties(egl::egl PROPERTIES
						  INTERFACE_LINK_LIBRARIES "${egl_LIBRARIES}" )
endif()
