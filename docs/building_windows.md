# Building LCEVC Decoder SDK - Windows

This SDK is built via CMake. Conan is used to resolve the dependencies.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `_build` in the recipes below. The exact name or location is not important, and can be chosen as appropriate. Note, however, that you may want to use different names for different targets (e.g. `build_windows`, `build_android`).

Most of these steps only need to be done once. The exceptions are: when you *edit* files, you will obviously need to *build* again (`cmake . --build`, or click 'build' in an IDE); and, when you *add* files, you will typically need to update `Sources.cmake` and then *cmake and build* again.

If you get stuck or make a typo during the conan step, it's usually sufficient to clear the build directory and the conan data directory and try the conan install again. That is, from `LCEVCdec`, run `rm -rf _build ~/.conan/data`.

---

## Requirements

See [Getting Started - Windows](getting_started_windows.md).

---

## Native Build

The below instructions use `[CONAN_HOME]` as a placeholder. This is typically found at `C:/Users/your.name/.conan`.

Whatever you use to build, you'll need to start with the following, in the root `lcevc_dec` repo:

```shell
python [CONAN_HOME]/windows/create_all_windows_conan_profiles.py

mkdir _build
cd _build
conan install .. -g cmake_find_package_multi -pr vs-2022-MT-Debug # AND
conan install .. -g cmake_find_package_multi -pr vs-2022-MT-Release
```

Instructions are provided for your choice of Visual Studio, Visual Studio Code, and Ninja (a command-line-only build tool).

### Visual Studio

From `_build`, simply call:

```shell
cmake ..
```

Note that you can simply say `cmake ..` rather than `cmake -G "Visual Studio 16 2019" ..`. This is because Visual Studio is our default build tool.

This will generate a solution file, `LCEVC_DEC_SDK.sln`

#### Building and running *within* the Visual Studio IDE

To develop and build in the Visual Studio IDE, simply open `LCEVC_DEC_SDK.sln`. To build, select Build > Build Solution.

If you want to *run* the code, there are three additional steps required:

1. Choose an executable to run, and set it as the "Startup Project":
   1. Choose an executable in the sidebar. Typically this will be the Sample or the Harness (if you want to do a decode), but you can also just run unit tests if you like.
   2. Right-click on that executable
   3. Select "Set as Startup Project"
2. For the Sample and Harness, you need to provide an input and an output if you want to see an image.
   1. Right-click again on the executable
   2. Select "Properties"
   3. Select "Debugging" in the window which pops up
      1. For the Sample, simply provide a path to an input and an output, in the correct order, such as `input.ts output.yuv`. OR
      2. For the Harness, provide the input and output in any order, with labels, such as `-o output.yuv -i input.mp4`. Note that ts and mp4 files should both work.
3. Finally, there is, *temporarily*, an additional step. There are plans to fix this, because it didn't *used* to be necessary. However, for now it is. This step is to tell Visual Studio where the .dll files are
   1. After a build, you will find the dlls in `_build/bin`. Copy the full path, ending in `/bin`. (NOTE: don't skip to the [export section](#export-build-for-use-in-integrationsplayers), but if you HAVE,  then you will ALSO find the dlls in `_build/install` and you can use that path just as well as the `/bin` path).
   2. Back in Visual Studio, *again* go to the "Properties" window for your chosen executable, and *again* go to the "Debugging" section
   3. In "Environment", add `PATH=%PATH%;C:/THAT/PATH/YOU/JUST/COPIED`. This means that, while you're running, your `PATH` environment variable will temporarily have the dll path added at the end.

Now, clicking the "Run" button should perform a decode.

Incidentally, if your destination file was a bare file name (as opposed to a full path), then you'll have to go fishing for it. It will end up in `_build/src/[executable_name]`.

#### Building and running on the command line

Alternately, you can remain on the command line. This will mean that you cannot do standard debugging things (like "stepping through" code line-by-line, or setting breakpoints). That is, unless you *also* install command-line debugger.

Cmake will then build *using* Visual Studio. That is done like so:

```shell
cmake --build . --config Debug # OR
cmake --build . --config Release
```

To run, you will need to do the command-line version of step 3 from the Visual Studio IDE instructions above. You can modify your `PATH` variable directly (begin by searching "edit the environment variables" in the Windows search bar). Alternately, you can set the `PATH` variable *only* for that particular command. For example, in cmd:

```shell
PATH=%PATH%;C:/THAT/PATH/YOU/JUST/COPIED lcevc_api_sample_cpp.exe input.ts output.yuv
```

Something similar will likely be possible in powershell too.

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

## Cross Compiled Build to Android using Ninja

The profiles currently available on conan are: ABIs of `armeabi-v7a`, `arm64-v8a`, `x86`, and `x86_64`; Android APIs of 21 through 30, inclusive; and build types of Release, RelWithDebugInfo, and Debug. Below, we use `arm64-v8a`, API 26, and Release.

Note, as well, that we now create *Android* conan profiles. When you do this, you should see every profile getting successfully created: these are the profiles that will be used at the `conan install` step.

The last difference is that you actually *can't* use Visual Studio as your generator here.

From command prompt, in the root `lcevc_dec` repo:

```shell
python [CONAN_HOME]/android/create_all_android_conan_profiles.py

mkdir _build
cd _build
conan install .. -pr android-arm64-v8a-api-26-Release
cmake -G Ninja -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=26 -DCMAKE_BUILD_TYPE=Release --toolchain=../cmake/toolchains/android_ndk.toolchain.cmake ..
cmake --build . --config Release
```

---

## Emscripten Build Using Ninja (optional)

This is used for web applications. The cmake for this is quite elaborate, but it allows you to build without any conan packages. It is liable to change, so I've alphabetised the 'DVN' defines to make it easier to keep track. You'll also need to find your Emscripten directory, which you set as `EMSCRIPTEN` and/or `EMSCRIPTEN_ROOT_PATH` during the 'Getting Started - Windows' instructions. Use it below, in place of `[EMSCRIPTEN]`.

From command prompt, in the root `lcevc_dec` repo:

```shell
mkdir _build
cd _build

cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=[EMSCRIPTEN]/cmake/Modules/Platform/Emscripten.cmake .. -DVN_CORE_SIMD=OFF -DVN_SDK_API_LAYER=OFF -DVN_SDK_EXECUTABLES=OFF -DVN_SDK_INTEGRATION_LAYER=OFF -DVN_SDK_TESTS=OFF -DVN_SDK_UNIT_TESTS=OFF -DVN_SDK_UTILITY=OFF

ninja # or cmake . --build
```

---

## Export build for use in integrations/players

This is relevant regardless of what prior steps you followed. This is needed if/when you want to use your freshly-built decoder in integrations and players such as ExoPlayer, FFmpeg, and AVPlayer, as well as our internal testing tools like Windows Playground.

From command prompt, after building, go to the `_build` directory where you built, and then:

```shell
cmake --install . --config Debug # OR
cmake --install . --config Release
cd ..
conan remove lcevc_dec
conan export .
```

This will produce dynamic libraries (`.dll` files) in `_build/install/bin`, static libraries (`.lib` files) in `_build/install/lib`, and will also allow you to add `lcevc_dec` as a conan dependency in your player (such as in a `conanfile.py` or `conanfile.txt`).
