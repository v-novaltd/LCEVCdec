
find_package(PkgConfig)

####################################
# xorg::xorg
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(xorg xorg)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_VERSION)

# Propagate version
set(xorg_VERSION xorg_VERSION)

# Create interface library
add_library(xorg::xorg INTERFACE IMPORTED)
set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION
            ${xorg_VERSION})
set_property(TARGET xorg::xorg PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_INCLUDE_DIRS}")
set_property(TARGET xorg::xorg PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES}")
set_property(TARGET xorg::xorg PROPERTY INTERFACE_LINK_DIRECTORIES
             "${xorg_LIBRARY_DIRS}")
set_property(TARGET xorg::xorg PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_CFLAGS}" "${xorg_CFLAGS_OTHER}")




####################################
# xorg::x11
####################################
# Find the package component
pkg_check_modules(xorg xorg x11)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::x11 INTERFACE IMPORTED)
set_property(TARGET xorg::x11 PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::x11 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_x11_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::x11 PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::x11 PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_x11_LIBDIR}")
set_property(TARGET xorg::x11 PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_x11_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::x11-xcb
####################################
# Find the package component
pkg_check_modules(xorg xorg x11-xcb)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::x11-xcb INTERFACE IMPORTED)
set_property(TARGET xorg::x11-xcb PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::x11-xcb PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_x11-xcb_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::x11-xcb PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::x11-xcb PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_x11-xcb_LIBDIR}")
set_property(TARGET xorg::x11-xcb PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_x11-xcb_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::fontenc
####################################
# Find the package component
pkg_check_modules(xorg xorg fontenc)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::fontenc INTERFACE IMPORTED)
set_property(TARGET xorg::fontenc PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::fontenc PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_fontenc_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::fontenc PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::fontenc PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_fontenc_LIBDIR}")
set_property(TARGET xorg::fontenc PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_fontenc_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::ice
####################################
# Find the package component
pkg_check_modules(xorg xorg ice)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::ice INTERFACE IMPORTED)
set_property(TARGET xorg::ice PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::ice PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_ice_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::ice PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::ice PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_ice_LIBDIR}")
set_property(TARGET xorg::ice PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_ice_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::sm
####################################
# Find the package component
pkg_check_modules(xorg xorg sm)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::sm INTERFACE IMPORTED)
set_property(TARGET xorg::sm PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::sm PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_sm_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::sm PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::sm PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_sm_LIBDIR}")
set_property(TARGET xorg::sm PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_sm_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xau
####################################
# Find the package component
pkg_check_modules(xorg xorg xau)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xau INTERFACE IMPORTED)
set_property(TARGET xorg::xau PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xau PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xau_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xau PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xau PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xau_LIBDIR}")
set_property(TARGET xorg::xau PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xau_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xaw7
####################################
# Find the package component
pkg_check_modules(xorg xorg xaw7)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xaw7 INTERFACE IMPORTED)
set_property(TARGET xorg::xaw7 PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xaw7 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xaw7_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xaw7 PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xaw7 PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xaw7_LIBDIR}")
set_property(TARGET xorg::xaw7 PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xaw7_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcomposite
####################################
# Find the package component
pkg_check_modules(xorg xorg xcomposite)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcomposite INTERFACE IMPORTED)
set_property(TARGET xorg::xcomposite PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcomposite PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcomposite_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcomposite PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcomposite PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcomposite_LIBDIR}")
set_property(TARGET xorg::xcomposite PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcomposite_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcursor
####################################
# Find the package component
pkg_check_modules(xorg xorg xcursor)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcursor INTERFACE IMPORTED)
set_property(TARGET xorg::xcursor PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcursor PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcursor_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcursor PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcursor PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcursor_LIBDIR}")
set_property(TARGET xorg::xcursor PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcursor_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xdamage
####################################
# Find the package component
pkg_check_modules(xorg xorg xdamage)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xdamage INTERFACE IMPORTED)
set_property(TARGET xorg::xdamage PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xdamage PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xdamage_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xdamage PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xdamage PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xdamage_LIBDIR}")
set_property(TARGET xorg::xdamage PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xdamage_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xdmcp
####################################
# Find the package component
pkg_check_modules(xorg xorg xdmcp)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xdmcp INTERFACE IMPORTED)
set_property(TARGET xorg::xdmcp PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xdmcp PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xdmcp_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xdmcp PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xdmcp PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xdmcp_LIBDIR}")
set_property(TARGET xorg::xdmcp PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xdmcp_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xext
####################################
# Find the package component
pkg_check_modules(xorg xorg xext)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xext INTERFACE IMPORTED)
set_property(TARGET xorg::xext PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xext PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xext_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xext PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xext PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xext_LIBDIR}")
set_property(TARGET xorg::xext PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xext_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xfixes
####################################
# Find the package component
pkg_check_modules(xorg xorg xfixes)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xfixes INTERFACE IMPORTED)
set_property(TARGET xorg::xfixes PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xfixes PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xfixes_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xfixes PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xfixes PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xfixes_LIBDIR}")
set_property(TARGET xorg::xfixes PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xfixes_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xft
####################################
# Find the package component
pkg_check_modules(xorg xorg xft)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xft INTERFACE IMPORTED)
set_property(TARGET xorg::xft PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xft PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xft_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xft PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xft PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xft_LIBDIR}")
set_property(TARGET xorg::xft PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xft_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xi
####################################
# Find the package component
pkg_check_modules(xorg xorg xi)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xi INTERFACE IMPORTED)
set_property(TARGET xorg::xi PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xi PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xi_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xi PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xi PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xi_LIBDIR}")
set_property(TARGET xorg::xi PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xi_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xinerama
####################################
# Find the package component
pkg_check_modules(xorg xorg xinerama)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xinerama INTERFACE IMPORTED)
set_property(TARGET xorg::xinerama PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xinerama PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xinerama_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xinerama PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xinerama PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xinerama_LIBDIR}")
set_property(TARGET xorg::xinerama PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xinerama_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xkbfile
####################################
# Find the package component
pkg_check_modules(xorg xorg xkbfile)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xkbfile INTERFACE IMPORTED)
set_property(TARGET xorg::xkbfile PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xkbfile PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xkbfile_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xkbfile PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xkbfile PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xkbfile_LIBDIR}")
set_property(TARGET xorg::xkbfile PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xkbfile_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xmu
####################################
# Find the package component
pkg_check_modules(xorg xorg xmu)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xmu INTERFACE IMPORTED)
set_property(TARGET xorg::xmu PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xmu PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xmu_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xmu PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xmu PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xmu_LIBDIR}")
set_property(TARGET xorg::xmu PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xmu_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xmuu
####################################
# Find the package component
pkg_check_modules(xorg xorg xmuu)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xmuu INTERFACE IMPORTED)
set_property(TARGET xorg::xmuu PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xmuu PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xmuu_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xmuu PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xmuu PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xmuu_LIBDIR}")
set_property(TARGET xorg::xmuu PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xmuu_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xpm
####################################
# Find the package component
pkg_check_modules(xorg xorg xpm)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xpm INTERFACE IMPORTED)
set_property(TARGET xorg::xpm PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xpm PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xpm_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xpm PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xpm PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xpm_LIBDIR}")
set_property(TARGET xorg::xpm PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xpm_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xrandr
####################################
# Find the package component
pkg_check_modules(xorg xorg xrandr)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xrandr INTERFACE IMPORTED)
set_property(TARGET xorg::xrandr PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xrandr PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xrandr_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xrandr PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xrandr PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xrandr_LIBDIR}")
set_property(TARGET xorg::xrandr PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xrandr_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xrender
####################################
# Find the package component
pkg_check_modules(xorg xorg xrender)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xrender INTERFACE IMPORTED)
set_property(TARGET xorg::xrender PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xrender PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xrender_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xrender PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xrender PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xrender_LIBDIR}")
set_property(TARGET xorg::xrender PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xrender_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xres
####################################
# Find the package component
pkg_check_modules(xorg xorg xres)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xres INTERFACE IMPORTED)
set_property(TARGET xorg::xres PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xres PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xres_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xres PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xres PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xres_LIBDIR}")
set_property(TARGET xorg::xres PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xres_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xscrnsaver
####################################
# Find the package component
pkg_check_modules(xorg xorg xscrnsaver)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xscrnsaver INTERFACE IMPORTED)
set_property(TARGET xorg::xscrnsaver PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xscrnsaver PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xscrnsaver_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xscrnsaver PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xscrnsaver PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xscrnsaver_LIBDIR}")
set_property(TARGET xorg::xscrnsaver PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xscrnsaver_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xt
####################################
# Find the package component
pkg_check_modules(xorg xorg xt)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xt INTERFACE IMPORTED)
set_property(TARGET xorg::xt PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xt PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xt_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xt PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xt PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xt_LIBDIR}")
set_property(TARGET xorg::xt PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xt_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xtst
####################################
# Find the package component
pkg_check_modules(xorg xorg xtst)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xtst INTERFACE IMPORTED)
set_property(TARGET xorg::xtst PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xtst PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xtst_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xtst PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xtst PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xtst_LIBDIR}")
set_property(TARGET xorg::xtst PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xtst_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xv
####################################
# Find the package component
pkg_check_modules(xorg xorg xv)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xv INTERFACE IMPORTED)
set_property(TARGET xorg::xv PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xv PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xv_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xv PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xv PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xv_LIBDIR}")
set_property(TARGET xorg::xv PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xv_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xvmc
####################################
# Find the package component
pkg_check_modules(xorg xorg xvmc)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xvmc INTERFACE IMPORTED)
set_property(TARGET xorg::xvmc PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xvmc PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xvmc_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xvmc PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xvmc PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xvmc_LIBDIR}")
set_property(TARGET xorg::xvmc PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xvmc_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xxf86vm
####################################
# Find the package component
pkg_check_modules(xorg xorg xxf86vm)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xxf86vm INTERFACE IMPORTED)
set_property(TARGET xorg::xxf86vm PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xxf86vm PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xxf86vm_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xxf86vm PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xxf86vm PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xxf86vm_LIBDIR}")
set_property(TARGET xorg::xxf86vm PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xxf86vm_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xtrans
####################################
# Find the package component
pkg_check_modules(xorg xorg xtrans)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xtrans INTERFACE IMPORTED)
set_property(TARGET xorg::xtrans PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xtrans PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xtrans_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xtrans PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xtrans PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xtrans_LIBDIR}")
set_property(TARGET xorg::xtrans PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xtrans_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-xkb
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-xkb)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-xkb INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-xkb PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-xkb PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-xkb_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-xkb PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-xkb PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-xkb_LIBDIR}")
set_property(TARGET xorg::xcb-xkb PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-xkb_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-icccm
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-icccm)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-icccm INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-icccm PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-icccm PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-icccm_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-icccm PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-icccm PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-icccm_LIBDIR}")
set_property(TARGET xorg::xcb-icccm PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-icccm_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-image
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-image)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-image INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-image PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-image PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-image_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-image PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-image PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-image_LIBDIR}")
set_property(TARGET xorg::xcb-image PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-image_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-keysyms
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-keysyms)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-keysyms INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-keysyms PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-keysyms PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-keysyms_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-keysyms PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-keysyms PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-keysyms_LIBDIR}")
set_property(TARGET xorg::xcb-keysyms PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-keysyms_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-randr
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-randr)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-randr INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-randr PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-randr PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-randr_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-randr PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-randr PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-randr_LIBDIR}")
set_property(TARGET xorg::xcb-randr PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-randr_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-render
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-render)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-render INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-render PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-render PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-render_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-render PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-render PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-render_LIBDIR}")
set_property(TARGET xorg::xcb-render PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-render_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-renderutil
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-renderutil)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-renderutil INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-renderutil PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-renderutil PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-renderutil_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-renderutil PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-renderutil PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-renderutil_LIBDIR}")
set_property(TARGET xorg::xcb-renderutil PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-renderutil_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-shape
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-shape)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-shape INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-shape PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-shape PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-shape_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-shape PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-shape PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-shape_LIBDIR}")
set_property(TARGET xorg::xcb-shape PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-shape_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-shm
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-shm)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-shm INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-shm PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-shm PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-shm_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-shm PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-shm PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-shm_LIBDIR}")
set_property(TARGET xorg::xcb-shm PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-shm_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-sync
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-sync)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-sync INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-sync PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-sync PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-sync_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-sync PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-sync PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-sync_LIBDIR}")
set_property(TARGET xorg::xcb-sync PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-sync_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-xfixes
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-xfixes)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-xfixes INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-xfixes PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-xfixes PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-xfixes_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-xfixes PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-xfixes PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-xfixes_LIBDIR}")
set_property(TARGET xorg::xcb-xfixes PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-xfixes_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-xinerama
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-xinerama)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-xinerama INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-xinerama PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-xinerama PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-xinerama_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-xinerama PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-xinerama PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-xinerama_LIBDIR}")
set_property(TARGET xorg::xcb-xinerama PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-xinerama_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb INTERFACE IMPORTED)
set_property(TARGET xorg::xcb PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb_LIBDIR}")
set_property(TARGET xorg::xcb PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xkeyboard-config
####################################
# Find the package component
pkg_check_modules(xorg xorg xkeyboard-config)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xkeyboard-config INTERFACE IMPORTED)
set_property(TARGET xorg::xkeyboard-config PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xkeyboard-config PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xkeyboard-config_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xkeyboard-config PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xkeyboard-config PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xkeyboard-config_LIBDIR}")
set_property(TARGET xorg::xkeyboard-config PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xkeyboard-config_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-atom
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-atom)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-atom INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-atom PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-atom PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-atom_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-atom PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-atom PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-atom_LIBDIR}")
set_property(TARGET xorg::xcb-atom PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-atom_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-aux
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-aux)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-aux INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-aux PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-aux PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-aux_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-aux PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-aux PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-aux_LIBDIR}")
set_property(TARGET xorg::xcb-aux PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-aux_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-event
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-event)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-event INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-event PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-event PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-event_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-event PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-event PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-event_LIBDIR}")
set_property(TARGET xorg::xcb-event PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-event_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-util
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-util)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-util INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-util PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-util PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-util_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-util PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-util PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-util_LIBDIR}")
set_property(TARGET xorg::xcb-util PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-util_CFLAGS}" "${xorg_CFLAGS_OTHER}")





####################################
# xorg::xcb-dri3
####################################
# Find the package component
pkg_check_modules(xorg xorg xcb-dri3)
find_package_handle_standard_args(xorg DEFAULT_MSG
    xorg_FOUND xorg_xorg_VERSION)

# Update the version name as modules seem to have it if not
if (xorg_xorg_VERSION AND NOT xorg_VERSION)
    set(xorg_VERSION xorg_xorg_VERSION)
    set_property(TARGET xorg::xorg PROPERTY INTERFACE_VERSION ${xorg_VERSION})
endif()

# Create interface library for component
add_library(xorg::xcb-dri3 INTERFACE IMPORTED)
set_property(TARGET xorg::xcb-dri3 PROPERTY INTERFACE_VERSION
            ${xorg_xorg_VERSION})
set_property(TARGET xorg::xcb-dri3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xorg_xcb-dri3_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xorg::xcb-dri3 PROPERTY INTERFACE_LINK_LIBRARIES
            "${xorg_LIBRARIES};")
set_property(TARGET xorg::xcb-dri3 PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xorg_xcb-dri3_LIBDIR}")
set_property(TARGET xorg::xcb-dri3 PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xorg_xcb-dri3_CFLAGS}" "${xorg_CFLAGS_OTHER}")


