Copyright (c) V-Nova International Limited 2023. All rights reserved.

# LCEVC SDK C++ Sample

This sample shows a basic usage of the LCEVCdec SDK Component.

Builds are expected to be 'out of source' — an empty build directory is created, and CMake is instructed to create build scripts there, with reference back to the source directory. All build files (objects, generated sources, and output artefacts) are kept within the build directory. This allows multiple build configurations to exist in parallel.

For these instructions, this 'out of source' build directory is specified as `build` in the example scripts below. The exact name or location is not important, and can be chosen as appropriate.

## Dependencies

This sample can be built using conan or pkg-config to retrieve external dependencies. These build docs specify how to build on Ubuntu with pkg-config (packages will vary on other distros) and Linux, macOS and Windows with conan.

The core requirements are the same as the LCEVCdec project and can be found in the [main build docs](docs/building.md).

When configuring the LCEVCdec build via cmake, the `-DVN_SDK_EXECUTABLES=ON` flag must be set to ensure the utilities and base decoder interface is built. Other LCEVCdec flags can be added including Vulkan as well as this.

## Building with conan

```shell
# First build & install LCEVCdec
cd LCEVCdec
pip install -r requirements.txt
./conan/ffmpeg/create.[sh|bat]
mkdir build && cd build
conan install -r conan-center -u --build=missing --settings=build_type=Release ..
cmake -DVN_SDK_EXECUTABLES=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
cmake --install .
# Now build the sample
cd ../src/sample_cpp/standalone
mkdir build && cd build
conan install -r conan-center -u --build=missing --settings=build_type=Release ..
cmake ..
cmake --build .
```

## Building with pkg-config

```shell
# First build & install LCEVCdec
sudo apt-get install -y pkg-config nlohmann-json3-dev libfmt-dev libcli11-dev libavcodec-dev libavformat-dev libavutil-dev libavfilter-dev libgtest-dev libgmock-dev libxxhash-dev librange-v3-dev
cd LCEVCdec
mkdir build && cd build
cmake -DVN_SDK_EXECUTABLES=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
cmake --install .
# Now build the sample
mkdir build && cd build
cmake ..
cmake --build .
```

## Execution

The sample will play a stream and dump the output to a file. The file extension, e.g. yuv, determines the color format of the output frames yuv - I420 10/8bit output, the bit depth is dependent on the input stream rgb - RGB 8bit output.

```shell
    lcevc_dec_sample <path to lcevc stream> dump.yuv
```
