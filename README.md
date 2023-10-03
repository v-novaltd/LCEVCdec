# LCEVC Decoder SDK

## About

Low Complexity Enhancement Video Codec Decoder (LCEVC_DEC) is the primary MPEG-5 Part 2 decoder SDK repository maintained by V-Nova. To learn what the LCEVC codec is and how it works, please refer to the [V-Nova documentation portal](https://docs.v-nova.com/v-nova/lcevc/lcevc-sdk-overview).

## Features

 * Decode LCEVC compliant bitstreams
 * Support for a range of formats including YUV, NV12 and RGBA
 * Support for a range of colour formats including BT709 and BT2020
 * Support for HDR and 10-bit streams
 * Support for ABR ladders
 * CPU pixel processing stage
 * Extensive API

## Block Diagram

![block_diagram](./docs/img/block_diagram.svg)

## Requirements

See [Getting Started](docs/getting_started.md) to set up the appropiate build tools for the host OS.

## Building

See:

 * [Building - Linux](docs/building_linux.md) to build Linux hosted targets.
 * [Building - macOS](docs/building_macos.md) to build macOS hosted targets.
 * [Building - Windows](docs/building_windows.md) to build Windows hosted targets.

## Installation

To install a build of the SDK, see [Installation](docs/install.md)

## Conan Packaging

To install, build and upload a Conan package for **LCEVC_DEC** see [Conan Packaging](docs/conan_packaging.md)

## Notice

Copyright © V-Nova Limited 2023

This software is protected by copyrights and other intellectual property rights and no license is granted to any such rights. If you would like to obtain a license to compile, distribute, or make any other use of this software, please contact V-Nova Limited info@v-nova.com.
