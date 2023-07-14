
find_package(PkgConfig)

####################################
# ZLIB::ZLIB
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(ZLIB zlib)
find_package_handle_standard_args(ZLIB DEFAULT_MSG
    ZLIB_FOUND ZLIB_VERSION)

# Propagate version
set(ZLIB_VERSION ZLIB_VERSION)

# Create interface library
add_library(ZLIB::ZLIB INTERFACE IMPORTED)
set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_VERSION
            ${ZLIB_VERSION})
set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${ZLIB_INCLUDE_DIRS}")
set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_LINK_LIBRARIES
            "${ZLIB_LIBRARIES}")
set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_LINK_DIRECTORIES
             "${ZLIB_LIBRARY_DIRS}")
set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_COMPILE_OPTIONS
             "${ZLIB_CFLAGS}" "${ZLIB_CFLAGS_OTHER}")

