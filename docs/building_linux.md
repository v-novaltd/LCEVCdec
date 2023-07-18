# Building LCEVC Decoder SDK - Linux

This SDK is built via CMake. Conan is used to resolve the dependencies.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the recipes below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_windows`, `build_linux`).

Most of these steps only need to be done once. The exceptions are: when you *edit* files, you will obviously need to *build* again (`cmake . --build`, or click 'build' in an IDE); and, when you *add* files, you will typically need to update `Sources.cmake` and then *cmake and build* again.

---

## Requirements

See [Getting Started - Linux](getting_started_linux.md).

## Conan

These intstructions assume that the Conan installation has it's default configuration, with the remote 'conancenter'.
If there is an existing configuration, that can be isolated by setting the CONAN_USER_HOME environment variable.

From a shell prompt in `LCEVCdec`:

```shell
mkdir _conan_home
export CONAN_USER_HOME=$PWD/_conan_home
conan remote list   # will create a new configuration and cache
```

THe supplied `conanfile.py` will build and cache a mininal version of the `ffmpeg` package suitable for decoding
base video in samples and tests.

## Native build using Unix Makefiles

From a shell prompt in `LCEVCdec`:

Release build (leaving artefacts in `install` directory):

```shell
mkdir _build
cd _build
conan install -r conancenter -u .. --build=missing -s build_type=Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cmake --install .
```

Debug build (leaving artefacts in `install` directory):

```shell
mkdir _build
cd _build
conan install -r conancenter -u .. --build=missing -s build_type=Debug
cmake -DCMAKE_BUILD_TYPE=Debug .. 
cmake --build .
cmake --install .
``
