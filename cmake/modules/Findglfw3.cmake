
find_package(PkgConfig)

####################################
# glfw::glfw
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(glfw glfw3)
find_package_handle_standard_args(glfw DEFAULT_MSG
    glfw_FOUND glfw_VERSION)

# Propagate version
set(glfw_VERSION glfw_VERSION)

# Create interface library
add_library(glfw::glfw INTERFACE IMPORTED)
set_property(TARGET glfw::glfw PROPERTY INTERFACE_VERSION
            ${glfw_VERSION})
set_property(TARGET glfw::glfw PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${glfw_INCLUDE_DIRS}")
set_property(TARGET glfw::glfw PROPERTY INTERFACE_LINK_LIBRARIES
            "${glfw_LIBRARIES}")
set_property(TARGET glfw::glfw PROPERTY INTERFACE_LINK_DIRECTORIES
             "${glfw_LIBRARY_DIRS}")
set_property(TARGET glfw::glfw PROPERTY INTERFACE_COMPILE_OPTIONS
             "${glfw_CFLAGS}" "${glfw_CFLAGS_OTHER}")

