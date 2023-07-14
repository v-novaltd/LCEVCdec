
find_package(PkgConfig)

####################################
# glad::glad
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(glad glad)
find_package_handle_standard_args(glad DEFAULT_MSG
    glad_FOUND glad_VERSION)

# Propagate version
set(glad_VERSION glad_VERSION)

# Create interface library
add_library(glad::glad INTERFACE IMPORTED)
set_property(TARGET glad::glad PROPERTY INTERFACE_VERSION
            ${glad_VERSION})
set_property(TARGET glad::glad PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${glad_INCLUDE_DIRS}")
set_property(TARGET glad::glad PROPERTY INTERFACE_LINK_LIBRARIES
            "${glad_LIBRARIES}")
set_property(TARGET glad::glad PROPERTY INTERFACE_LINK_DIRECTORIES
             "${glad_LIBRARY_DIRS}")
set_property(TARGET glad::glad PROPERTY INTERFACE_COMPILE_OPTIONS
             "${glad_CFLAGS}" "${glad_CFLAGS_OTHER}")

