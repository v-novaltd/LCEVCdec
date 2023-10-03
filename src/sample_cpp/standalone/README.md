Copyright (c) V-Nova International Limited 2023. All rights reserved.

# LCEVC SDK C++ Sample

This sample shows a basic usage of the LCEVCdec SDK Component.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the example scripts below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_windows`, `build_android`).

Command line examples are given assuming that from the sample code directory, where this `README.md` file lives.
 
# Dependencies

## Tools

 - python (3.7 or above)
 - conan (1.36)
 - cmake (3.18.1 or above)
 - C++ toolchain (Visual Studio 2019 , GCC or clang)

## Libraries

 - fmt (8.0.1)
 - cli11 (2.3.2)
 - ffmpeg (5.1)

## Installing Conan

The library dependencies can be satisfied using `conan` - to ensure the required version is installed:

```shell
    pip install -r requirements.txt
```

These instructions assume that the Conan installation has it's default configuration, using the remote  'conancenter'.
If there is an existing configuration, that can be isolated by setting the `CONAN_USER_HOME` environment variable,
and creating new default configuration and empty cache:

### Isolating Conan - Windows

```shell
    mkdir _conan_home
    set CONAN_USER_HOME=%CD%\_conan_home
    conan remote list
```

### Isolating Conan - Linux

```shell
    mkdir _conan_home
    export CONAN_USER_HOME=$PWD/_conan_home
    conan remote list
```

# Compilation - Windows

Whatever you use to build, you'll need to start with the following to install dependencies:

```shell
    mkdir _build
    cd _build
    conan install -r conancenter -u --build=missing -s compiler.runtime=MT -s compiler.version=16 -s build_type=Release ..
    conan install -r conancenter -u --build=missing -s compiler.runtime=MTd -s compiler.version=16 -s build_type=Debug ..
```
The first time the above `conan install ...` commands are run, a minimal version of the `ffmpeg` libraries will be built. This
will be cached locally for future installs.

The *Debug* install is only needed if you intend to debug the code.

Instructions are provided for your choice of Visual Studio, and Ninja (a command-line-only build tool).

## Visual Studio

```shell
    cd _build
    cmake -G "Visual Studio 16 2019" ..
```
This will generate a solution file, `LCEVCdec_Sample_Cpp.sln`, which you can open in the Visual Studio IDE. You can develop AND build there. Alternately, you can remain on the command line and build like so:

### Release build

Artefacts in `install` directory:

```shell
    cd _build
    cmake --build . --config Release
    cmake --install . --config Release
```

### Debug build

Artefacts in `install` directory:

```shell
    cd _build
    cmake --build . --config Debug
    cmake --install . --config Debug
```

## Ninja

Identical to the above, but with `-G Ninja`.

```shell
    cd _build
    cmake -G Ninja ..
    cmake --build . --config Debug # OR
    cmake --build . --config Release
```

# Compilation - Linux

#### Release Build

```shell
    mkdir _build
    cd _build
    conan install -s build_type=Release .. 
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .
```

#### Debug Build

```shell
    mkdir _build
    cd _build
    conan install -s build_type=Debug ..
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
    cmake --build .
```

## Execution

The sample will play a stream and dump the output to a file.
The file extension, e.g. yuv, determines the color format of the output frames
yuv - I420 10/8bit output, the bit depth is dependent on the input stream
rgb - RGB 8bit output

```shell
    lcevc_dec_api_sample_cpp <path to lcevc stream> dump.yuv
```
