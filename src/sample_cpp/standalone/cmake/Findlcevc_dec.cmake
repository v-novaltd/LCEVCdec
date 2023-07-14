#
#
find_package(PkgConfig)

####################################
# lcevc_dec::lcevc_dec
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(lcevc_dec lcevc_dec)
find_package_handle_standard_args(lcevc_dec DEFAULT_MSG
    lcevc_dec_FOUND lcevc_dec_VERSION)

# Propagate version
set(lcevc_dec_VERSION lcevc_dec_VERSION)

# Create interface library
add_library(lcevc_dec::lcevc_dec INTERFACE IMPORTED)
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_VERSION
            ${lcevc_dec_VERSION})
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${lcevc_dec_INCLUDE_DIRS}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_LINK_LIBRARIES
            "${lcevc_dec_LIBRARIES}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_LINK_DIRECTORIES
             "${lcevc_dec_LIBRARY_DIRS}")
set_property(TARGET lcevc_dec::lcevc_dec PROPERTY INTERFACE_COMPILE_OPTIONS
             "${lcevc_dec_CFLAGS}" "${lcevc_dec_CFLAGS_OTHER}")

