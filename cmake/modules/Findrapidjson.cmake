
find_package(PkgConfig)

####################################
# rapidjson::rapidjson
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(rapidjson rapidjson)
find_package_handle_standard_args(rapidjson DEFAULT_MSG
    rapidjson_FOUND rapidjson_VERSION)

# Propagate version
set(rapidjson_VERSION rapidjson_VERSION)

# Create interface library
add_library(rapidjson::rapidjson INTERFACE IMPORTED)
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_VERSION
            ${rapidjson_VERSION})
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${rapidjson_INCLUDE_DIRS}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_LINK_LIBRARIES
            "${rapidjson_LIBRARIES}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_LINK_DIRECTORIES
             "${rapidjson_LIBRARY_DIRS}")
set_property(TARGET rapidjson::rapidjson PROPERTY INTERFACE_COMPILE_OPTIONS
             "${rapidjson_CFLAGS}" "${rapidjson_CFLAGS_OTHER}")

