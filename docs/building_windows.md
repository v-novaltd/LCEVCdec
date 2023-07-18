# Building LCEVC Decoder SDK - Windows

This SDK is built via CMake. Conan is used to resolve the dependencies.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the recipes below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_windows`, `build_android`).

Most of these steps only need to be done once. The exceptions are: when you *edit* files, you will obviously need to *build* again (`cmake . --build`, or click 'build' in an IDE); and, when you *add* files, you will typically need to update `Sources.cmake` and then *cmake and build* again.

---

## Requirements

See [Getting Started - Windows](getting_started_windows.md).

## Conan

These intstructions assume that the Conan installation has it's default configuration, with the remote 'conancenter' enabled.
If there is an existing configuration, that can be isolated by setting the CONAN_USER_HOME environment variable.

From the command prompt used for building, with `LCEVCdec` as the current directory:

```shell
mkdir _conan_home
set CONAN_USER_HOME=%CD%\_conan_home
conan remote list   # will create a new configuration and cache
```

---

## Native Build

Whatever you use to build, you'll need to start with the following, in the root `LCEVCdec` repo:

```shell

mkdir _build
cd _build
conan install -r conancenter -u --build=missing -s compiler.runtime=MT -s compiler.version=16 -s build_type=Release .. # And
conan install -r conancenter -u --build=missing -s compiler.runtime=MTd -s compiler.version=16 -s build_type=Debug .. # If debugging
```

Instructions are provided for your choice of Visual Studio, Visual Studio Code, and Ninja (a command-line-only build tool).

### Visual Studio

From `_build`, simply call:

```shell
cmake -G "Visual Studio 16 2019" ..
```

This will generate a solution file, `LCEVCdec.sln`, which you can open in the Visual Studio IDE. You can develop AND build there. Alternately, you can remain on the command line and build like so:

Release build (leaving artefacts in `install` directory):

```shell
cmake --build . --config Release
cmake --install . --config Release
```

Debug build (leaving artefacts in `install` directory):

```shell
cmake --build . --config Debug
cmake --install . --config Debug
```

### Ninja

Identical to the above, but with `-G Ninja`.

```shell
cmake -G Ninja ..
cmake --build . --config Debug # OR
cmake --build . --config Release
```

### Visual Studio Code

If you wish to use Visual Studio Code, you will need to do *either* of the above `cmake ..` steps, but also export compile commands. Here's how that looks, using Visual Studio as the generator:

```shell
cmake DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
```

This should allow you to develop and build within Visual Studio Code. These compile commands are also used for clang-format and clang-tidy.

---
