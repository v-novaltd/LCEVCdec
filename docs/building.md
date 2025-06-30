# Building

This repository contains two main libraries `core` and `api` as well as a suite of test executables, sphinx documentation and additional supplements. If your aim is to build LCEVCdec as a dependency for another player or tool such as ffmpeg, only the main libraries are required and these have no dependencies - this is the default cmake config. If building for development work or to use the test harness to decode an LCEVC stream as a standalone executable, other cmake options are required with corresponding dependencies as listed here.

## Build Options

| Name                       | Default | Dependencies                          | Description                                                                                                                                   |
|----------------------------|---------|---------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| BUILD_SHARED_LIBS          | ON      | -                                     | Builds `api` and pipeline libraries as shared libraries, else static                                                                          |
| VN_SDK_SAMPLE_SOURCE       | ON      | -                                     | Install option for packaging the sample code and standalone CMakeLists                                                                        |
| VN_SDK_API_LAYER           | ON      | -                                     | Some integrations only require the core functionality or cannot use the C++ API                                                               |
| VN_SDK_EXECUTABLES         | OFF     | ffmpeg libraries, xxhash, cli11, fmt  | Builds the test harness and sample code executables requiring a base decoder. Use this option to enable standalone decoding of LCEVC streams. |
| VN_SDK_UNIT_TESTS          | OFF     | gtest, range-v3                       | Build the google test unit test executables for each target, for development purposes only                                                    |
| VN_SDK_JSON_CONFIG         | OFF     | nlohmann_json                         | Allow the API to be configured with a JSON string, required for `VN_SDK_EXECUTABLES`                                                          |
| VN_SDK_PIPELINE_CPU        | ON      | -                                     | Build the default CPU pipeline                                                                                                                |
| VN_SDK_PIPELINE_LEGACY     | ON      | -                                     | Build the deprecated legacy pipeline                                                                                                          |
| VN_SDK_PIPELINE_VULKAN     | OFF     | vulkan-loader, vulkan-header, glslang | Build the experimental Vulkan GPU pipeline                                                                                                    |
| VN_SDK_DOCS                | OFF     | doxygen, sphinx, plantuml             | Builds formatted HTML documentation for API integration                                                                                       |
| VN_SDK_COVERAGE            | OFF     | -                                     | Generate gcov code coverage statistics                                                                                                        |
| VN_SDK_TRACING             | ON      | -                                     | Configurable tracing information recorder for function entry/exit in perfetto format. Required for `trace_file` - CPU pipeline only           |
| VN_SDK_METRICS             | ON      | -                                     | Configurable metrics recording in perfetto format - CPU pipeline only                                                                         |
| VN_SDK_DIAGNOSTICS_ASYNC   | ON      | -                                     | Enable asynchronous output of logging, tracing and metrics - CPU pipeline only                                                                |
| VN_SDK_MAXIMUM_LOG_LEVEL   | VERBOSE | -                                     | Allow log levels down to this level, increase to reduce library sizes significantly                                                           |
| VN_SDK_WARNINGS_FAIL       | OFF     | -                                     | Enable build flags such as `Werror`, use for development and CI to maintain clean builds                                                      |
| VN_SDK_BUILD_DETAILS       | OFF     | -                                     | Include build time and origin in libraries - non-reproducible build                                                                           |
| VN_SDK_SYSTEM_INSTALL      | ON      | -                                     | Gives correct install paths for a Linux system, disable for installing to a portable package                                                  |
| VN_SDK_FFMPEG_LIBS_PACKAGE | ''      | external ffmpeg package path          | Specify a local path to ffmpeg libraries if they cannot be found via a package manager                                                        |

All build options can be added to the cmake generation step eg. `-DVN_SDK_EXECUTABLES=ON`

## Core requirements

### Compiler

- **GCC** - use GGC 9 or later
- **Clang** - use Clang 14 or later
- **MSVC** - use Visual Studio 16 (2019) or later with MSVC 19

### Other requirements (all platforms)

- Git
- Python >= 3.9 - use of a venv is recommended
- CMake >= 3.19.0
- Ninja, recommended but not required - replace `-G Ninja` with `-G "Unix Makefiles"` on Linux and macOS or remove it on Windows from the following CMake commands if not using Ninja

### Windows requirements

[Visual Studio](https://visualstudio.microsoft.com/downloads/) with the 'Desktop development with C++' option is required. Ensure you use the 'x64 Native Tools Command Prompt' for all cmake operations.

## Building libraries only - native

To build the main libraries only for integration to other players and tools, use the following commands for Linux, Windows or Mac

```shell
cd LCEVCdec
mkdir build
cd build
cmake ..
cmake --build . --config Release
cmake --install .
```

Libraries, headers and licenses will be installed to the system unless `CMAKE_INSTALL_PREFIX` is specified. A .pc pkg-config file is also installed to `<install_prefix>/lib/pkgconfig` to use LCEVCdec as a dependency in downstream projects.

## Building with executables and unit tests - native

Building with executables allows you to decode LCEVC enhanced streams to YUV using the test harness or sample executable, this requires `libav*` libraries from the ffmpeg project to decode the base component of the stream as well as some logging, CLI and JSON parsing utilities. On Linux, these can be gathered with APT or similar package manager and discovered with pkg-config, for all platforms conan can be used to resolve the dependencies.

### Linux

To gather dependencies with APT use:
```shell
sudo apt-get install -y pkg-config nlohmann-json3-dev libfmt-dev libcli11-dev libavcodec-dev libavformat-dev libavutil-dev libavfilter-dev libgtest-dev libgmock-dev libxxhash-dev librange-v3-dev
```

OR gather the dependencies with conan:
```shell
cd LCEVCdec
pip install -r requirements.txt
./conan/ffmpeg/create.sh
conan install -r conancenter -u .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11
```

Finally build:
```shell
cd LCEVCdec
mkdir build
cd build
cmake -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON -DCMAKE_BUILD_TYPE=Release -G Ninja ..
cmake --build .
cmake --install .
```

### Windows & macOS

```shell
cd LCEVCdec
pip install -r requirements.txt
./conan/ffmpeg/create.[sh|bat]
mkdir build
cd build
conan install -r conan-center -u --build=missing --settings=build_type=Release ..
cmake -DVN_SDK_EXECUTABLES=ON -DVN_SDK_UNIT_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
cmake --install .
```

## Building with Vulkan

Vulkan is currently an experimental pipeline with limited bitstream support and unoptimised performance however it can easily be built by installing the following APT packages and adding the `-DVN_SDK_PIPELINE_VULKAN=ON` CMake flag to any of the previous instructions.

```shell
sudo apt-get install -y libvulkan-dev glslang-tools
```

Please note that the conan create step builds ffmpeg from source which is non-trivial, please see the [ffmpeg compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide) for tips. Alternatively you can download these libraries pre-built for your platform from [ffmpeg.org](https://ffmpeg.org/download.html) (ensure they are shared/dynamic libraries) and use `-o base_decoder=manual` in your conan install and `-DVN_SDK_FFMPEG_LIBS_PACKAGE=path/to/downloaded/libav*` in the cmake generation.

## Building for Android - cross compiled from Linux

Download the Android NDK version r25c from [here](https://github.com/android/ndk/wiki/Unsupported-Downloads), extract and follow the commands. Substitute the ABI (either `armeabi-v7a`, `arm64-v8a`, `x86` or `x86_64`) and PLATFORM ([API level](https://apilevels.com/)) as required for your platform.

```shell
export ANDROID_NDK=path/to/extracted/ndk/25.2.9519653
cmake -G Ninja -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=30 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/android_ndk.toolchain.cmake ..
cmake --build .
cmake --install .
```

It is possible to create FFmpeg libraries for Android and build the test harness, however that is not included in this guide.
