/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <find_assets_dir.h>
#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/utility/bin_reader.h>

#include <filesystem>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets = findAssetsDir("src/enhancement/test/assets");

class ConfigParserTest : public testing::Test
{
public:
    void SetUp() override
    {
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        allocator = ldcMemoryAllocatorMalloc();
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);
        ldeFrameConfigInitialize(ldcMemoryAllocatorMalloc(), &frameConfig);
        frameConfig.chunkAllocation = chunkAllocation;
    }

    void openAsset(std::string path) { binReader = createBinReader((kTestAssets / path).string()); }

    std::vector<uint8_t> getFrame()
    {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> rawNalUnit;
        binReader->read(decodeIndex, presentationIndex, rawNalUnit);

        return rawNalUnit;
    }

    LdcMemoryAllocator* allocator{};
    LdcMemoryAllocation chunkAllocation = {0};
    LdeGlobalConfig globalConfig = {};
    LdeFrameConfig frameConfig = {};

private:
    std::unique_ptr<BinReader> binReader;
};

TEST_F(ConfigParserTest, checkParams)
{
    openAsset("parse_std.bin");
    auto frame = getFrame();
    bool globalConfigModified = false;
    EXPECT_TRUE(ldeConfigsParse(frame.data(), frame.size(), &globalConfig, &frameConfig,
                                &globalConfigModified));

    EXPECT_EQ(globalConfigModified, true);
    EXPECT_EQ(globalConfig.bitstreamVersionSet, true);
    EXPECT_EQ(globalConfig.bitstreamVersion, 1);
    EXPECT_EQ(frameConfig.nalType, NTIDR);
    EXPECT_EQ(globalConfig.width, 180);
    EXPECT_EQ(globalConfig.height, 100);
    EXPECT_EQ(globalConfig.transform, TransformDD);
    EXPECT_EQ(globalConfig.upscale, USCubic);
    EXPECT_EQ(globalConfig.scalingModes[LOQ0], Scale1D);
    EXPECT_EQ(globalConfig.initialized, true);
    EXPECT_EQ(globalConfig.predictedAverageEnabled, true);
    EXPECT_EQ(globalConfig.temporalReducedSignallingEnabled, true);
    EXPECT_EQ(globalConfig.temporalEnabled, true);
    EXPECT_EQ(frameConfig.frameConfigSet, true);
    EXPECT_EQ(frameConfig.entropyEnabled, true);
    EXPECT_EQ(frameConfig.temporalSignallingPresent, 0);
    EXPECT_EQ(frameConfig.temporalRefresh, true);

    EXPECT_EQ(globalConfig.crop.left, 2);
    EXPECT_EQ(globalConfig.crop.right, 4);
    EXPECT_EQ(globalConfig.crop.top, 6);
    EXPECT_EQ(globalConfig.crop.bottom, 8);

    EXPECT_EQ(frameConfig.numChunks, 24);
    EXPECT_EQ(frameConfig.chunkAllocation.size, 768);
    EXPECT_EQ(frameConfig.chunks[0].size, 3);
    EXPECT_EQ(frameConfig.loqEnabled[LOQ0], true);
    EXPECT_EQ(frameConfig.loqEnabled[LOQ1], false);

    EXPECT_LE(frameConfig.sharpenStrength, 0.04);
    EXPECT_EQ(frameConfig.ditherEnabled, true);
    EXPECT_EQ(frameConfig.ditherType, DTUniform);
    EXPECT_EQ(frameConfig.ditherStrength, 2);
    EXPECT_EQ(frameConfig.deblockEnabled, false); // Encoder bug, this should be true
    EXPECT_EQ(globalConfig.deblock.corner, 2);
    EXPECT_EQ(globalConfig.deblock.side, 1);
    EXPECT_EQ(frameConfig.dequantOffsetMode, DQMConstOffset);
    EXPECT_EQ(frameConfig.dequantOffset, 1);

    EXPECT_EQ(globalConfig.tileWidth[0], 90);
    EXPECT_EQ(globalConfig.tileWidth[2], 45);
    EXPECT_EQ(globalConfig.tileHeight[0], 50);
    EXPECT_EQ(globalConfig.tileHeight[2], 25);
    EXPECT_EQ(globalConfig.numTiles[0][LOQ0], 4);
    EXPECT_EQ(globalConfig.numTiles[0][LOQ1], 2);

    ldeConfigsReleaseFrame(&frameConfig);
    EXPECT_FALSE(frameConfig.chunks);
}

TEST_F(ConfigParserTest, multiFrame)
{
    openAsset("parse_std.bin");
    auto frame = getFrame();
    EXPECT_EQ(frame.size(), 65);
    bool globalConfigModified = false;
    EXPECT_TRUE(ldeConfigsParse(frame.data(), frame.size(), &globalConfig, &frameConfig,
                                &globalConfigModified));
    EXPECT_EQ(frameConfig.nalType, NTIDR);

    frame = getFrame();
    EXPECT_EQ(frame.size(), 8);
    EXPECT_TRUE(ldeConfigsParse(frame.data(), frame.size(), &globalConfig, &frameConfig,
                                &globalConfigModified));
    EXPECT_EQ(frameConfig.nalType, NTNonIDR);

    ldeConfigsReleaseFrame(&frameConfig);
}

TEST_F(ConfigParserTest, checkHDRParams)
{
    openAsset("parse_hdr.bin");
    auto frame = getFrame();
    bool globalConfigModified = false;
    EXPECT_TRUE(ldeConfigsParse(frame.data(), frame.size(), &globalConfig, &frameConfig,
                                &globalConfigModified));
    EXPECT_EQ(globalConfig.vuiInfo.flags, 5377);
    EXPECT_EQ(globalConfig.vuiInfo.videoFormat, VUIVFNTSC);
    EXPECT_EQ(globalConfig.vuiInfo.aspectRatioIdc, 255);
    EXPECT_EQ(globalConfig.vuiInfo.sarWidth, 20);
    EXPECT_EQ(globalConfig.vuiInfo.sarHeight, 10);
    EXPECT_EQ(globalConfig.vuiInfo.colourPrimaries, 11);
    EXPECT_EQ(globalConfig.vuiInfo.transferCharacteristics, 9);
    EXPECT_EQ(globalConfig.vuiInfo.matrixCoefficients, 8);
    EXPECT_EQ(globalConfig.vuiInfo.chromaSampleLocTypeTopField, 4);
    EXPECT_EQ(globalConfig.vuiInfo.chromaSampleLocTypeBottomField, 5);

    EXPECT_EQ(globalConfig.hdrInfo.contentLightLevel.maxContentLightLevel, 100);
    EXPECT_EQ(globalConfig.hdrInfo.contentLightLevel.maxPicAverageLightLevel, 80);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.whitePointX, 10000);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.whitePointY, 20000);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.maxDisplayMasteringLuminance, 110);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.minDisplayMasteringLuminance, 10);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesX[0], 1);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesX[1], 2);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesX[2], 3);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesY[0], 4);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesY[1], 5);
    EXPECT_EQ(globalConfig.hdrInfo.masteringDisplay.displayPrimariesY[2], 6);

    ldeConfigsReleaseFrame(&frameConfig);
}
