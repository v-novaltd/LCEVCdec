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

See [Getting Started](docs/getting_started.md) to set up the appropriate build tools for the host OS.

## Building

See:

 * [Building - Linux](docs/building_linux.md) to build Linux hosted targets.
 * [Building - macOS](docs/building_macos.md) to build macOS hosted targets.
 * [Building - Windows](docs/building_windows.md) to build Windows hosted targets.
 * [(Optional) Building with PGO](docs/building_with_pgo.md) to build with Profile-Guided Optimization.

## Installation

To install a build of the SDK, see [Installation](docs/install.md)

## Conan Packaging

To install, build and upload a Conan package for **LCEVC_DEC** see [Conan Packaging](docs/conan_packaging.md)

## Notice

Copyright © V-Nova Limited 2014-2024

Additional Information and Restrictions
* The LCEVCdec software is licensed under the BSD-3-CLAUSE-CLEAR License
* The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
* If the software is incorporated into another project, THE TERMS OF THE BSD 3-Clause Clear License and the additional licensing information contained in this folder MUST BE MAINTAINED, AND THE SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT.
* ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE EXCLUSION OF PATENT LICENSES PROVISION OF THIS BSD-3-CLAUSE-CLEAR LICENSE. For enquiries about patent licenses, please contact legal@v-nova.com.
