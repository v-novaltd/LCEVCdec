
find_package(PkgConfig)

####################################
# tclap::tclap
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(tclap tclap)
find_package_handle_standard_args(tclap DEFAULT_MSG
    tclap_FOUND tclap_VERSION)

# Propagate version
set(tclap_VERSION tclap_VERSION)

# Create interface library
add_library(tclap::tclap INTERFACE IMPORTED)
set_property(TARGET tclap::tclap PROPERTY INTERFACE_VERSION
            ${tclap_VERSION})
set_property(TARGET tclap::tclap PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${tclap_INCLUDE_DIRS}")
set_property(TARGET tclap::tclap PROPERTY INTERFACE_LINK_LIBRARIES
            "${tclap_LIBRARIES}")
set_property(TARGET tclap::tclap PROPERTY INTERFACE_LINK_DIRECTORIES
             "${tclap_LIBRARY_DIRS}")
set_property(TARGET tclap::tclap PROPERTY INTERFACE_COMPILE_OPTIONS
             "${tclap_CFLAGS}" "${tclap_CFLAGS_OTHER}")

