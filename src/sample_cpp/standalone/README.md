Copyright (c) V-Nova International Limited 2023. All rights reserved.

# LCEVC SDK Samplw

This sample shows a basic usage of the LCEVCdec SDK Component.

## Dependancies

The software requires an AVCodec SDK to build. Select the download suitable for your
platform from here https://ffmpeg.org/download.html

## Required Binaries

Please ensure you have the following binaries available for the sample to
compile and execute correctly:

   - lcevc_dec_core.dll / lcevc_dec_core.so
   - lcevc_dec_api.dll / lcevc_dec_api.so

## Compilation

A CMake build script is provided for convenience. This sample depends on the 
LCEVCdec SDK Component.

This assumes the user has an appropriate toolchain installed.

1. Copy the include and lib folders from the LCEVCdec SDK Component into the root
   folder of this sample containing the CMakeLists.txt

2. Make a sub-directory `mkdir cmake && cd cmake` to execute CMake from.

3. Run cmake depending on your platform either:
   3a. If using a custom ffmpeg directory add `-DAVCODEC_ROOT=<path to ffmpeg directory>` to the cmake command below
   3b. For Windows: `cmake -G "Visual Studio 16 2019" ..`
   3c. For Linux: `cmake ..`

4. Compile using cmake: cmake --build . --config Release

5. Binaries will be placed in `bin`.

6. Copy the Required Binaries to the `bin` folder, if using a custom ffmpeg directory you 
   will need to copy the shared libraries from the ffmpeg bin directory.

## Execution

The sample will play a stream and dump the output to a file.
The file extension, e.g. yuv, determines the color format of the output frames
yuv - I420 10/8bit output, the bit depth is dependent on the input stream
rgb - RGB 8bit output

>    `lcevc_dec_api_sample_cpp <path to lcevc stream> dump.yuv`
