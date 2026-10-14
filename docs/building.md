# Building

This repository contains two main libraries, `core` and `api`, as well as a suite of test executables, Sphinx documentation and additional supplements. If your aim is to build LCEVCdec as a dependency for another player or tool such as FFmpeg, only the main libraries are required; this is the default CMake configuration. Development work and the test harness require additional CMake options and their corresponding dependencies, as listed here.

## Build Options

| Name                       | Default | Dependencies                           | Description                                                                                                                                   |
| -------------------------- | ------- | -------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| BUILD_SHARED_LIBS          | ON      | -                                      | Builds `api` and pipeline libraries as shared libraries, else static                                                                          |
| VN_SDK_SAMPLE_SOURCE       | ON      | -                                      | Install option for packaging the sample code and standalone CMakeLists                                                                        |
| VN_SDK_API_LAYER           | ON      | -                                      | Some integrations only require the core functionality or cannot use the C++ API                                                               |
| VN_SDK_PIPELINE            | ON      | -                                      | Build the pipeline module; disable to build the core enhancement library only                                                                 |
| VN_SDK_EXECUTABLES         | OFF     | ffmpeg libraries, xxhash, cli11, fmt   | Builds the test harness and sample code executables requiring a base decoder. Use this option to enable standalone decoding of LCEVC streams. |
| VN_SDK_UNIT_TESTS          | OFF     | gtest, range-v3                        | Build the google test unit test executables for each target, for development purposes only                                                    |
| VN_SDK_JSON_CONFIG         | OFF     | nlohmann_json                          | Allow the API to be configured with a JSON string, required for `VN_SDK_EXECUTABLES`                                                          |
| VN_SDK_PIPELINE_CPU        | ON      | -                                      | Build the default CPU pipeline                                                                                                                |
| VN_SDK_PIPELINE_VULKAN     | OFF     | vulkan-loader, vulkan-headers, glslang | Build the experimental Vulkan GPU pipeline                                                                                                    |
| VN_SDK_PIXEL_PROCESSING    | ON      | -                                      | Build the pixel processing module for format conversion, upscaling and image operations                                                       |
| VN_SDK_DOCS                | OFF     | doxygen, sphinx, plantuml              | Builds formatted HTML documentation for API integration                                                                                       |
| VN_SDK_SIMD                | ON      | -                                      | Build accelerated SSE4.1 & AVX2 functions for x86 platforms or NEON functions for ARM platforms                                               |
| VN_SDK_LTO                 | OFF     | -                                      | Build with link time optimisation and interprocedural optimization if supported                                                               |
| VN_SDK_MINIMUM_LIB_SIZE    | OFF     | zlib when with VN_SDK_PIPELINE_VULKAN  | Apply aggressive size library optimisations                                                                                                   |
| VN_SDK_STD_FILE            | ON      | -                                      | Enable use of the C standard library FILE struct                                                                                              |
| VN_SDK_THREADING           | ON      | -                                      | Enable multithreaded decoding                                                                                                                 |
| VN_SDK_COVERAGE            | OFF     | -                                      | Generate gcov code coverage statistics                                                                                                        |
| VN_SDK_TRACING             | ON      |                                        | Enable diagnostics tracing via `trace_file` output. Does not depend on Perfetto SDK.                                                          |
| VN_SDK_TRACING_SYSTEM_SINK | OFF     | perfetto, fmt                          | Sink diagnostics events to Perfetto SDK TrackEvent output for more precise tracing. Currently supports Android only. Requires VN_SDK_TRACING. |
| VN_SDK_METRICS             | ON      | -                                      | Configurable metrics recording in Perfetto format - CPU pipeline only                                                                         |
| VN_SDK_DIAGNOSTICS_ASYNC   | ON      | -                                      | Enable asynchronous output of logging, tracing and metrics - CPU pipeline only                                                                |
| VN_SDK_MEMORY_DIAGNOSTICS  | OFF     | -                                      | Enable logging events for all memory allocations.                                                                                             |
| VN_SDK_LDE_FLOATS          | ON      | -                                      | Enable float manipulations in LDE/common code. In particular, allows use of floorf and printf format strings for floats and doubles.          |
| VN_SDK_MAXIMUM_LOG_LEVEL   | VERBOSE | -                                      | Allow log levels down to this level, increase to reduce library sizes significantly                                                           |
| VN_SDK_WARNINGS_FAIL       | OFF     | -                                      | Enable build flags such as `Werror`, use for development and CI to maintain clean builds                                                      |
| VN_SDK_BUILD_DETAILS       | OFF     | -                                      | Include build time and origin in libraries - non-reproducible build                                                                           |
| VN_SDK_SYSTEM_INSTALL      | ON      | -                                      | Gives correct install paths for a Linux system, disable for installing to a portable package                                                  |
| VN_SDK_STATIC_LIB_TYPE     | STATIC  | -                                      | Override the library type for static libraries. OBJECT libraries will compile but not produce a library. INTERFACE libraries will do neither. |
| VN_SDK_FFMPEG_LIBS_PACKAGE | ''      | external ffmpeg package path           | Specify a local path to ffmpeg libraries if they cannot be found via a package manager                                                        |
| VN_SDK_ATOMIC_CUSTOM       | OFF     |                                        | Use a custom implementation of our common/atomic.h. Must be called "atomic_custom.h" and be accessible in the include path                    |
| VN_SDK_CLOCK_CUSTOM        | OFF     |                                        | Use a custom implementation of our common/clock.h. Must be called "clock_custom.h" and be accessible in the include path                      |
| VN_SDK_STDLIB_CUSTOM       | OFF     |                                        | Use a custom implementation of our common/stdlib.h. Must be called "stdlib_custom.h" and be accessible in the include path                    |
| VN_SDK_THREADS_CUSTOM      | OFF     |                                        | Use a custom implementation of our common/threads.h. Must be called "threads_custom.h" and be accessible in the include path                  |

All build options can be added to the CMake generation step, e.g. `-DVN_SDK_EXECUTABLES=ON`.

## Core requirements

### Compiler

- **GCC** - use GCC 9 or later
- **Clang** - use Clang 14 or later
- **MSVC** - use Visual Studio 16 (2019) or later with MSVC 19

### Other requirements (all platforms)

- Git
- Python >= 3.9 - use of a venv is recommended
- CMake >= 3.19.0
- Ninja, recommended but not required - replace `-G Ninja` with `-G "Unix Makefiles"` on Linux and macOS or remove it on Windows from the following CMake commands if not using Ninja

### Windows requirements

[Visual Studio](https://visualstudio.microsoft.com/downloads/) with the 'Desktop development with C++' option is required. Ensure you use the 'x64 Native Tools Command Prompt' for all CMake operations.

### CPU runtime requirements

On x86, the SSE implementations enabled by `VN_SDK_SIMD` require SSE4.1. AVX2 upscalers are used when AVX2 is available. CPU capabilities are detected at runtime and scalar implementations are used as a fallback when available. `VN_SDK_MINIMUM_LIB_SIZE` builds omit scalar and SSE implementations; the x86 minimum-size build therefore requires AVX2.

## Building libraries only - native

To build the main libraries only for integration to other players and tools, use the following commands for Linux, Windows or Mac

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release
```

Libraries, headers and licenses will be installed to the system unless `CMAKE_INSTALL_PREFIX` is specified. A .pc pkg-config file is also installed to `<install_prefix>/lib/pkgconfig` to use LCEVCdec as a dependency in downstream projects.

## Building with executables and unit tests - native

Building with executables allows you to decode LCEVC-enhanced streams to YUV using the test harness or sample executable. This requires `libav*` libraries from the FFmpeg project to decode the base component of the stream, as well as logging, CLI and JSON parsing utilities. On Linux, these can be installed with APT or a similar package manager and discovered with pkg-config. Conan can resolve the dependencies on all platforms.

### Linux

To gather dependencies with APT use:

```bash
sudo apt-get install -y pkg-config nlohmann-json3-dev libfmt-dev libcli11-dev libavcodec-dev libavformat-dev libavutil-dev libavfilter-dev libgtest-dev libgmock-dev libxxhash-dev librange-v3-dev
```

Then build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON
cmake --build build
cmake --install build
```

Alternatively, use Conan for the dependencies:

```bash
pip install -r requirements.txt
./conan/ffmpeg/create.sh
conan install -u . --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11 --install-folder=build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON
cmake --build build
cmake --install build
```

### macOS

```bash
pip install -r requirements.txt
./conan/ffmpeg/create.sh
conan install -u . --build=missing -s build_type=Release --install-folder=build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON
cmake --build build
cmake --install build
```

### Windows

```bat
pip install -r requirements.txt
conan\ffmpeg\create.bat
conan install -u . --build=missing -s build_type=Release --install-folder=build
cmake -S . -B build -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON
cmake --build build --config Release
cmake --install build --config Release
```

The `conan create` step builds FFmpeg from source, which is non-trivial; see the [FFmpeg compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) for tips. Alternatively, download pre-built libraries for your platform from [ffmpeg.org](https://ffmpeg.org/download.html), ensure they are shared libraries, use `-o base_decoder=manual` with `conan install`, and pass `-DVN_SDK_FFMPEG_LIBS_PACKAGE=path/to/downloaded/libav*` when configuring CMake.

## Building with Vulkan - Linux only

Vulkan is currently an experimental pipeline with limited bitstream support and unoptimised performance. Build it by installing the following dependencies and adding the `-DVN_SDK_PIPELINE_VULKAN=ON` CMake flag to the previous instructions.

```bash
sudo apt-get install -y libvulkan-dev glslang-tools
```

Alternatively, install the Vulkan dependencies with Conan:

```bash
conan install -u . --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11 -o vulkan=True --install-folder=build
```

To build and run the Vulkan unit tests from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVN_SDK_PIPELINE_VULKAN=ON -DVN_SDK_UNIT_TESTS=ON
cmake --build build --target lcevc_dec_pipeline_vulkan_test_unit
./build/bin/lcevc_dec_pipeline_vulkan_test_unit
```

## Building for Android - cross compiled from Linux

Download Android NDK r25c from [here](https://github.com/android/ndk/wiki/Unsupported-Downloads), extract it, and follow the commands below. Match `ANDROID_ABI` to the Conan architecture using `armeabi-v7a`/`armv7`, `arm64-v8a`/`armv8`, `x86`/`x86`, or `x86_64`/`x86_64`. Set both API-level arguments to the required [API level](https://apilevels.com/); Clang 14 corresponds to NDK r25c.

### Dependencies from APT

Similar to the instructions for Vulkan on desktop, install the following dependencies to compile the Android Vulkan pipeline:

```bash
sudo apt-get install -y libvulkan-dev glslang-tools
```

And build with:

```bash
export ANDROID_NDK=path_to_ndk_install
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/android_ndk.toolchain.cmake \
    -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=30
cmake --build build
```

### Dependencies from Conan

Alternatively, Conan can install the required dependencies instead of APT. This also enables trace events to be sent to the system Perfetto service with the Conan option `--options=tracing=True` and the CMake option `-DVN_SDK_TRACING_SYSTEM_SINK=ON`.

```bash
export ANDROID_NDK=path_to_ndk_install
conan install -u . --build=missing \
    --profile:build=default --settings:host=os=Android --settings:host=os.api_level=30 --settings:host=arch=armv7 \
    --settings:host=compiler=clang --settings:host=compiler.version=14 --settings:host=compiler.libcxx=c++_static \
    --options=base_decoder=none --options=vulkan=True --options=tracing=True \
    --conf:host=tools.android:ndk_path="$ANDROID_NDK" \
    --install-folder=build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/android_ndk.toolchain.cmake \
    -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=30 -DVN_SDK_TRACING=ON -DVN_SDK_TRACING_SYSTEM_SINK=ON
cmake --build build
```

With Conan, it is also possible to create FFmpeg libraries for Android and build the test harness, however that is not included in this guide.

## Building for iOS

iOS builds use the external [iOS toolchain](https://github.com/leetal/ios-cmake/tree/06465b27698424cf4a04a5ca4904d50a3c966c45) to build for various Apple targets, see it's help section for full options. Use `OS64` for iOS devices or `SIMULATOR64` for the simulator. Add `-DBUILD_SHARED_LIBS=OFF` for static libraries, or `-DBUILD_SHARED_LIBS=ON` with the Xcode generator for frameworks:

```bash
cmake -S . -B build-ios -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios.toolchain.cmake -DPLATFORM=OS64 \
    -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios --config Release
cmake --install build-ios --config Release
```
