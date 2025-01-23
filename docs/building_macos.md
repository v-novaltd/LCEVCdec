# Building LCEVC Decoder SDK -  macOS

This SDK is built via CMake. Conan is used to resolve the dependencies.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the recipes below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_ios`, `build_tvos`).

Most of these steps only need to be done once. The exceptions are: when you *edit* files, you will obviously need to *build* again (`cmake . --build`, or click 'build' in an IDE); and, when you *add* files, you will typically need to update `Sources.cmake` and then *cmake and build* again.

If you get stuck or make a typo during the conan step, it's usually sufficient to clear the build directory and the conan data directory and try the conan install again. That is, from `LCEVCdec`, run `rm -rf _build ~/.conan/data`.

---

## Requirements

See [Getting Started - macOS](getting_started_macos.md).

## General information

Firstly, Apple builds are missing several features. These are the features you may see disabled:

`VN_SDK_EXECUTABLES`, `VN_SDK_TESTS`, `VN_SDK_UNIT_TESTS`

Secondly, Apple is quite different from other operating systems. In particular:
1. We typically produce *all our libraries* as [static libraries](#static-library). Why? Because many of the integrations which *use* these libraries are Dynamic [xcframeworks](#xcframework), and Apple does not allow Dynamic xcframeworks to use other Dynamic xcframeworks, so it must use Static libraries instead.
2. Rather than building for *one specific platform*, we typically build for *all* target platforms, then bundle all of these static libraries together into a static xcframework.

By contrast, on Windows and Linux, we usually produce a mix of static and dynamic libraries, and only build for one platform at a time.

## Native build on macOS

The below instructions use `[CONAN_HOME]` as a placeholder. This is typically found at `~/.conan`.

The "install all profiles" step will have installed armv8 and x86_64 profiles, each with 3 configurations. It doesn't hurt to `conan install` both Release and Debug, so you can swap build types without changing directories. Don't bother with `RelWithDebInfo` as that isn't fully implemented yet.

The following builds for macOS on x86_64, from a shell prompt in `lcevc_dec`:

```shell
mkdir _build
cd _build
conan install .. -g  cmake_find_package_multi -pr=macos-11.0-x86_64-apple-clang-14.0-Release -o executables=False # And
conan install .. -g  cmake_find_package_multi -pr=macos-11.0-x86_64-apple-clang-14.0-Debug -o executables=False

cmake -G 'Unix Makefiles' -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/ios.toolchain.cmake -DPLATFORM=MAC -DVN_SDK_EXECUTABLES=OFF -DVN_SDK_UNIT_TESTS=OFF -DVN_SDK_FFMPEG_LIBS_PACKAGE="" -DVN_SDK_TESTS=OFF -DVN_INTEGRATION_OPENGL=OFF ..

cmake --build .
cmake --install .
```

The build step will populate the `bin` directory. However, the more useful directory is `install`. This will have [static libraries](#static-library) in the `lib` directory, and *headers* in the `include` directory. If passing this on to an integrator, it is generally alright to give them the `install` directory in one big zip file.

### Native build using Xcode

If you want to try this, then simply follow the same steps above (in a fresh directory), replacing `'Unix Makefiles'` with `Xcode`. This will produce a file called `LCEVC_DEC_SDK.xcodeproj`. Then you can open this `xcodeproj` using Xcode.

Then you are able to build in Xcode, or using `xcodebuild`, or using `cmake --build .`.

## Cross Compiled Build to iOS, tvOS, and Simulators of iOS and tvOS

This is pretty similar to macOS. The differences are:
- We disable executables and unit tests at the conan install step, as well as at the cmake step. This is because some platforms are still building for C++11, and our executables and unit tests require some C++14-only libraries at the conan step.
- We use a `CMAKE_TOOLCHAIN_FILE` and a `PLATFORM` at the cmake step. This is how the compiler builds for the correct target platform. Change the `PLATFORM` to build for iOS, tvOS, etc.

Below are the instructions for an iOS Release build, from `lcevc_dec`:

```shell
mkdir _build
cd _build
conan install .. --profile=ios-13.0-armv8-apple-clang-14.0-Release -o executables=False -o unit_tests=False # AND
conan install .. --profile=ios-13.0-armv8-apple-clang-14.0-Debug -o executables=False -o unit_tests=False

cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/ios.toolchain.cmake -DPLATFORM=OS64 -DVN_SDK_EXECUTABLES=OFF -DVN_SDK_TESTS=OFF -DVN_SDK_UNIT_TESTS=OFF -DBUILD_SHARED_LIBS=OFF ..

cmake --build .
cmake --install .
```

As with the macos build, this produces a handy `install` directory with [static libraries](#static-library) in `lib` and headers in `include`.

To produce builds for other platforms, the most important thing is to change the `PLATFORM`. `OS64` can be replaced by `TVOS`, `SIMULATOR64`, or `SIMULATOR_TVOS` as needed. You also SHOULD change the conan profile accordingly: turn "ios" to "tvos" for tvs, and turn "armv8" to "x86_64" for simulators. However, the conan profile is less important, and sometimes it'll just give you a warning rather than an error.

To produce a Debug build, simply change the `CMAKE_BUILD_TYPE`. `Release` can be replaced by `Debug` (there is currently no intermediate option).

### Bundling different architectures into a [fat binary](#fat-binary)

A fat binary is a multi-architecture static library. That is, it is a `.a` file made of other `.a` files. It should have one static library for each architectures of a given device. For example, a fat binary for ios might contain the arm64 and x86_64 versions of the same static library, which would allow it to be used on real devices as well as simulators.

Below is an example of making a fat binary for ios architectures.

```shell
lipo build_ios/bin/liblcevc_dec_integration.a build_ios_simulator/bin/ liblcevc_dec_integration.a -create -output liblcevc_dec_integration_ios.a
```

## Building for ios lcevc vtds decoder

Recall that we typically produce static libraries, because integrations are usually dynamic xcframeworks, and they cannot contain other dynamic xcframeworks.

However, for the ios lcevc vtds decoder project, you may want dynamic libraries for your own purposes. In this case, simply remove `-DBUILD_SHARED_LIBS=OFF` from the command line above. This will give you `.framework` files for the main libraries, although some will still be `.a` because they are only produced as static libraries.

### Bundling different architectures into an xcframework

An [xcframework](#xcframework) is a bundle of different libraries/frameworks, one for each architecture and device. While it is *possible* to create xcframeworks out of static libraries (`.a` files), the typical use is to make them out of dynamic frameworks.

For example, here's how you might bundle together `lcevc_dec_api.framework` files for iOS, tvOS, and simulators of iOS and tvOS.

```shell
xcodebuild -create-xcframework \
-framework build_iOS/bin/lcevc_dec_api.framework \
-framework build_tvOS/bin/lcevc_dec_api.framework \
-framework build_simulator_iOS/bin/lcevc_dec_api.framework \
-framework build_simulator_tvOS/bin/lcevc_dec_api.framework \
-output bin/lcevc_dec_api.xcframework
```

## Definitions

### framework
An Apple wrapper for a library. A framework may hold EITHER a dynamic library OR a static library. To keep things totally clear, it may help to use the term "dynamic framework" when discussing a framework which wraps a dynamic library.

That said, Apple advises us NOT to put static libraries into frameworks [here](https://developer.apple.com/documentation/xcode/creating-a-multi-platform-binary-framework-bundle#Avoid-issues-when-using-alternate-build-systems). So, you will only see dynamic frameworks and static libraries, no static frameworks.

### xcframework
A set of libraries (either all `.framework` files, or all `.a` files), one for each architecture and device that you wish to build for. For example, you might have ios arm64, ios x86_64, tvos arm64, and tvos x86_64, all in one xcframework. The x86_64 builds would typically be for simulators.

We use the term "dynamic xcframework" for an xcframework that contains only dynamic [frameworks](#framework).

### static library
This has the same meaning as in other operating systems. In particular, a static library (as stored in a `.a` file) is a set of object files. Object files are the things that the Linker links.

### fat binary
This is like an xcframework, in the sense that it bundles together libraries for different architectures. *However*, there are two differences. Firstly, fat binaries are for different architectures, but the *same device*. For instance, you might have a tvos fat binary, with both arm64 and x86_64 on it. Secondly, fat binaries are *only* for `.a` files, and are ALSO stored in `.a` files.
