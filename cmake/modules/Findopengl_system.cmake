
find_package(PkgConfig)

####################################
# opengl::opengl
####################################
# todo handle version, set dep>=ver or dep==ver in module spec here
# Find the package
pkg_check_modules(opengl opengl)
find_package_handle_standard_args(opengl DEFAULT_MSG
    opengl_FOUND opengl_VERSION)

# Propagate version
set(opengl_VERSION opengl_VERSION)

# Create interface library
add_library(opengl::opengl INTERFACE IMPORTED)
set_property(TARGET opengl::opengl PROPERTY INTERFACE_VERSION
            ${opengl_VERSION})
set_property(TARGET opengl::opengl PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${opengl_INCLUDE_DIRS}")
set_property(TARGET opengl::opengl PROPERTY INTERFACE_LINK_LIBRARIES
            "${opengl_LIBRARIES}")
set_property(TARGET opengl::opengl PROPERTY INTERFACE_LINK_DIRECTORIES
             "${opengl_LIBRARY_DIRS}")
set_property(TARGET opengl::opengl PROPERTY INTERFACE_COMPILE_OPTIONS
             "${opengl_CFLAGS}" "${opengl_CFLAGS_OTHER}")

