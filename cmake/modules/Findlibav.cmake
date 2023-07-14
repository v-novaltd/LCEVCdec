
find_package(PkgConfig)

####################################
# libav::libav
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(libav libav)
find_package_handle_standard_args(libav DEFAULT_MSG
    libav_FOUND libav_VERSION)

# Propagate version
set(libav_VERSION libav_VERSION)

# Create interface library
add_library(libav::libav INTERFACE IMPORTED)
set_property(TARGET libav::libav PROPERTY INTERFACE_VERSION
            ${libav_VERSION})
set_property(TARGET libav::libav PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${libav_INCLUDE_DIRS}")
set_property(TARGET libav::libav PROPERTY INTERFACE_LINK_LIBRARIES
            "${libav_LIBRARIES}")
set_property(TARGET libav::libav PROPERTY INTERFACE_LINK_DIRECTORIES
             "${libav_LIBRARY_DIRS}")
set_property(TARGET libav::libav PROPERTY INTERFACE_COMPILE_OPTIONS
             "${libav_CFLAGS}" "${libav_CFLAGS_OTHER}")

