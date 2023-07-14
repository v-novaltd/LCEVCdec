
find_package(PkgConfig)

####################################
# xxHash::xxHash
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(xxHash libxxhash)
find_package_handle_standard_args(xxHash DEFAULT_MSG
    xxHash_FOUND xxHash_VERSION)

# Propagate version
set(xxHash_VERSION xxHash_VERSION)

# Create interface library
add_library(xxHash::xxHash INTERFACE IMPORTED)
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_VERSION
            ${xxHash_VERSION})
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xxHash_INCLUDE_DIRS}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_LINK_LIBRARIES
            "${xxHash_LIBRARIES}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_LINK_DIRECTORIES
             "${xxHash_LIBRARY_DIRS}")
set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xxHash_CFLAGS}" "${xxHash_CFLAGS_OTHER}")




####################################
# xxHash::xxhash
####################################
# Find the package component
pkg_check_modules(xxHash libxxhash libxxhash)
find_package_handle_standard_args(xxHash DEFAULT_MSG
    xxHash_FOUND xxHash_libxxhash_VERSION)

# Update the version name as modules seem to have it if not
if (xxHash_libxxhash_VERSION AND NOT xxHash_VERSION)
    set(xxHash_VERSION xxHash_libxxhash_VERSION)
    set_property(TARGET xxHash::xxHash PROPERTY INTERFACE_VERSION ${xxHash_VERSION})
endif()

# Create interface library for component
add_library(xxHash::xxhash INTERFACE IMPORTED)
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_VERSION
            ${xxHash_libxxhash_VERSION})
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${xxHash_xxhash_INCLUDEDIR}")
# This one is special as it comes from root package name
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_LINK_LIBRARIES
            "${xxHash_LIBRARIES};")
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_LINK_DIRECTORIES
            "${xxHash_xxhash_LIBDIR}")
set_property(TARGET xxHash::xxhash PROPERTY INTERFACE_COMPILE_OPTIONS
             "${xxHash_xxhash_CFLAGS}" "${xxHash_CFLAGS_OTHER}")


