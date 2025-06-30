/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/utility/bin_reader.h>
#include <stdint.h>

#include <string>

using namespace lcevc_dec::utility;

int main(int argc, char** argv)
{
    std::string inputFile;

    CLI::App app{"Enhancement bin decoder sample"};
    app.add_option("input", inputFile, "Input stream")->required();
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Setup common logger and memory utilities
    ldcDiagnosticsInitialize(NULL);
    ldcDiagnosticsLogLevel(LdcLogLevelInfo);
    ldcDiagnosticsHandlerPush(ldcDiagHandlerStdio, stdout);
    LdcMemoryAllocator* allocator = ldcMemoryAllocatorMalloc();

    // Setup bin reader and associated vars
    uint32_t frameIndex = 0;
    int64_t decodeIndex = 0;
    int64_t presentationIndex = 0;
    std::vector<uint8_t> rawNalUnit;
    std::unique_ptr<BinReader> binReader = createBinReader(inputFile);

    // Setup LCEVC configs and cmdbuffer output
    LdeGlobalConfig globalConfig = {};
    LdeFrameConfig frameConfig = {};
    LdeCmdBufferCpu cmdBufferCpu = {};

    // Initialize memory for the configs, chunks and cmdbuffer
    ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);
    ldeFrameConfigInitialize(allocator, &frameConfig);
    ldeCmdBufferCpuInitialize(allocator, &cmdBufferCpu, 0);

    // Loop until we have no frames
    while (binReader->read(decodeIndex, presentationIndex, rawNalUnit)) {
        if (bool globalConfigModified = false;
            !ldeConfigsParse(rawNalUnit.data(), rawNalUnit.size(), &globalConfig, &frameConfig,
                             &globalConfigModified)) {
            return EXIT_FAILURE;
        }

        fmt::print("Decoding frame {}\n", frameIndex);
        uint8_t transformSize = globalConfig.transform == TransformDDS ? 16 : 4;
        for (int8_t loqIdx = 1; loqIdx >= 0; loqIdx--) {
            for (uint8_t planeIdx = 0; planeIdx < globalConfig.numPlanes; planeIdx++) {
                for (uint32_t tileIdx = 0; tileIdx < globalConfig.numTiles[planeIdx][loqIdx]; tileIdx++) {
                    // Reset the cmdBuffer for this loq-plane-tile
                    ldeCmdBufferCpuReset(&cmdBufferCpu, transformSize);
                    if (!ldeDecodeEnhancement(&globalConfig, &frameConfig, static_cast<LdeLOQIndex>(loqIdx),
                                              planeIdx, tileIdx, &cmdBufferCpu, nullptr, nullptr)) {
                        return EXIT_FAILURE;
                    }
                    fmt::print("Frame {} LOQ{} plane {} tile {} has {} residuals\n", frameIndex,
                               loqIdx, planeIdx, tileIdx, cmdBufferCpu.count);
                }
            }
        }

        frameIndex++;
    }
    return EXIT_SUCCESS;
}
