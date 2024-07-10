# Build Environment

Instructions for setting up your environment may be different depending on which OS you are using, but they all have basic dependencies:

- git
- python (3.7 or above)
- cmake (3.17 or above)
- ninja (1.8.2 or above)
- C and C++ compilers to build code

# Dependencies

This is the minimal list of required dependencies, with tested version numbers in parentheses:

- glad (0.1.33)
- glfw (3.3.2)
- gtest (1.12.1)
- ffmpeg-libs-minimal (n6.0-vnova-59)
- opengl (system)
- range-v3 (0.12.0)
- rapidjson (v1.1.0-709-g06d5)
- tclap (1.2.1)
- xxhash (0.8.1)
- zlib (1.2.11)

# Building

```
# Build and install SDK
mkdir _install
mkdir build
cd build
PKG_CONFIG_PATH="$(pwd)/../pkgconfig" cmake -DCMAKE_BUILD_TYPE=Release -G Ninja ..
cmake --build .
cmake --install . --prefix "../_install"

# Build sample
mkdir _install/sample/build
cd _install/sample/build
PKG_CONFIG_PATH="../../pkgconfig" cmake ..
cmake --build .
```
