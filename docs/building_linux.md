# Building LCEVC Decoder SDK - Linux

This SDK is built via CMake. Conan is used to resolve the dependencies.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the recipes below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_windows`, `build_android`).

Most of these steps only need to be done once. The exceptions are: when you *edit* files, you will obviously need to *build* again (`cmake . --build`, or click 'build' in an IDE); and, when you *add* files, you will typically need to update `Sources.cmake` and then *cmake and build* again.

If you get stuck or make a typo during the conan step, it's usually sufficient to clear the build directory and the conan data directory and try the conan install again. That is, from `lcevc_dec`, run `rm -rf _build ~/.conan/data`.

---

## Requirements

See [Getting Started - Linux](getting_started_linux.md).

## Native build using Ninja

The below instructions use `[CONAN_HOME]` as a placeholder. This is typically found at `~/.conan`.

From a shell prompt in `lcevc_dec`:

```shell
mkdir _build
cd _build
conan install .. -g cmake_find_package_multi -pr=gcc-11-Release # AND
conan install .. -g cmake_find_package_multi -pr=gcc-11-Debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. # OR
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Cross Compiled Build to Android using Ninja

The profiles currently available on conan are: ABIs of `armeabi-v7a`, `arm64-v8a`, `x86`, and `x86_64`; Android APIs of 21 through 30, inclusive; and build types of Release, RelWithDebugInfo, and Debug. Below, we use `armeabi-v7a`, API 30, and Release.

This assumes you already have `ANDROID_SDK_HOME` set to the location of your Android SDK (that is, `Android/Sdk`). This may also be called `ANDROID_HOME`.

You may wish to add the `export` steps to your `~/.bashrc` file to avoid doing them every time.

From a shell prompt:

```shell
python3 [CONAN_HOME]/android/create_all_android_conan_profiles.py

mkdir _build
cd _build
export ANDROID_NDK=${ANDROID_SDK_HOME}/ndk/25.2.9519653 # cmake needs this
export ANDROID_NDK_PATH=${ANDROID_SDK_HOME}/ndk/25.2.9519653 # conan needs this
conan install .. --profile=android-armeabi-v7a-api-30-Release --build=missing
cmake -G Ninja -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=30 -DCMAKE_BUILD_TYPE=Release --toolchain=../cmake/toolchains/android_ndk.toolchain.cmake ..
cmake --build .
```

Note the 'build=missing'. This is different from other conan install steps. It usually is harmless but unnecessary, but here it is necessary.

## Cross Compiled Build to ARM64 for Linux

It is also possible to cross-compile for ARM64 on Linux. The big differences are (1) you use a different conan profile, and (2) certain features are absent on ARM64, so they are disabled at the cmake step.

Additionally, if you didn't do the [native build](#native-build-using-ninja) steps above, then you'll need to copy those first two steps here, because they are needed during `conan install`. That is, you'll need to install GL and X11 libs, and then create all linux conan profiles.

From a shell prompt:

```shell
	mkdir _build
	cd _build
	conan install -pr aarch64-linux-gnu-gcc-11-Release ..
	cmake -G Ninja -DCMAKE_BUILD_TYPE=Release --toolchain=../cmake/toolchains/aarch64-linux-gnu-gcc-11.toolchain.cmake -DVN_INTEGRATION_WORKAROUND_NO_GL_X11_LIBS=ON ..
	cmake --build .
```

That 'GL and X11' workaround, in particular, is likely to change in the near future. It is effectively saying: 'those libraries that were *required* for the conan install step, are also *unusable* in the cmake step'.

---

## Export build for use in integrations/players

This is relevant regardless of what prior steps you followed. This is needed if/when you want to use your freshly-built decoder in integrations and players such as ExoPlayer, FFmpeg, and AVPlayer, as well as our internal testing tools like Windows Playground.

From a shell prompt, after building, go to the `_build` directory where you built, and then:

```shell
cmake --install . --config Debug # OR
cmake --install . --config Release
cd ..
conan remove lcevc_dec
conan export .
```

This will produce dynamic libaries (`.so` files) in `_build/install/lib`, and will also allow you to add `lcevc_dec` as a conan depedency in your player (such as in a `conanfile.py` or `conanfile.txt`). By default, shared libraries will be produced for the API and core targets and static libraries for all other targets. To force static libraries for all targets use the CMake option `-DBUILD_SHARED_LIBS=OFF`. 
