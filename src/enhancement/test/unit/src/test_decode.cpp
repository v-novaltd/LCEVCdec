/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/enhancement/utilities.h>
#include <LCEVC/utility/bin_reader.h>
#include <LCEVC/utility/md5.h>

#include <filesystem>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets = findAssetsDir("src/enhancement/test/assets");

class Decode : public testing::Test
{
public:
    virtual void openBin() { assert(false); }
    void SetUp() override
    {
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        allocator = ldcMemoryAllocatorMalloc();
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);
        ldeFrameConfigInitialize(allocator, &frameConfig);
        frameConfig.chunkAllocation = chunkAllocation;

        openBin();
        EXPECT_EQ(getFrame(), true);
    }

    bool getFrame()
    {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> rawNalUnit;

        if (!m_binReader->read(decodeIndex, presentationIndex, rawNalUnit)) {
            return false;
        }

        if (bool globalConfigModified = false;
            !ldeConfigsParse(rawNalUnit.data(), rawNalUnit.size(), &globalConfig, &frameConfig,
                             &globalConfigModified)) {
            return false;
        }
        return true;
    }

    std::string hashCpuBuffer()
    {
        hash.reset();
        hash.update(cmdBufferCpu.data.start,
                    static_cast<uint32_t>(cmdBufferCpu.data.currentCommand - cmdBufferCpu.data.start));
        hash.update(cmdBufferCpu.data.currentResidual,
                    static_cast<uint32_t>(cmdBufferCpu.data.end - cmdBufferCpu.data.currentResidual));
        return hash.hexDigest();
    }
    std::string hashGpuBuffer()
    {
        hash.reset();
        hash.update((uint8_t*)cmdBufferGpu.commands,
                    cmdBufferGpu.commandCount * sizeof(LdeCmdBufferGpuCmd));
        hash.update((uint8_t*)cmdBufferGpu.residuals, cmdBufferGpu.residualCount * sizeof(int16_t));
        return hash.hexDigest();
    }

    LdcMemoryAllocator* allocator{};
    LdcMemoryAllocation chunkAllocation = {0};
    LdeGlobalConfig globalConfig = {0};
    LdeFrameConfig frameConfig = {0};

    LdeCmdBufferCpu cmdBufferCpu = {};
    LdeCmdBufferGpu cmdBufferGpu = {};
    LdeCmdBufferGpuBuilder cmdBufferGpuBuilder = {};

    MD5 hash;
    std::unique_ptr<BinReader> m_binReader;
};

class DecodeTemporalOn : public Decode
{
    void openBin() override
    {
        m_binReader = createBinReader((kTestAssets / "decode_temp_on.bin").string());
        if (!m_binReader) {
            FAIL() << "Failed to open BIN file";
        }
    }
};

class DecodeTemporalOff : public Decode
{
    void openBin() override
    {
        m_binReader = createBinReader((kTestAssets / "decode_temp_off.bin").string());
        if (!m_binReader) {
            FAIL() << "Failed to open BIN file";
        }
    }
};

TEST(ResidualChunks, ChecksAllTilesAndExcludesTemporal)
{
    LdeGlobalConfig global{};
    global.initialized = true;
    global.numPlanes = 1;
    global.numLayers = 4;
    global.numTiles[0][LOQ0] = 2;
    LdeFrameConfig frame{};
    frame.frameConfigSet = true;
    frame.entropyEnabled = true;
    frame.loqEnabled[LOQ0] = true;
    LdeChunk chunks[10]{};
    frame.chunks = chunks;
    frame.numChunks = 10;
    frame.tileChunkResidualIndex[0][LOQ0] = 1;
    chunks[0].entropyEnabled = true;
    chunks[9].entropyEnabled = true; // Non-residual chunks must not affect the result.
    EXPECT_FALSE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    chunks[8].entropyEnabled = true; // Last layer of the second tile.
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    frame.entropyEnabled = false;
    EXPECT_FALSE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    frame.entropyEnabled = true;
    frame.loqEnabled[LOQ0] = false;
    EXPECT_FALSE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    frame.loqEnabled[LOQ0] = true;
    frame.numChunks = 8;
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    frame.chunks = nullptr;
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
}

TEST(ResidualChunks, WholeTileZeroRun)
{
    LdeGlobalConfig global{};
    global.initialized = true;
    global.numPlanes = 1;
    global.numLayers = 1;
    global.numTiles[0][LOQ0] = 1;
    global.width = global.tileWidth[0] = 32;
    global.height = global.tileHeight[0] = 32;
    global.transform = TransformDDS;
    LdeFrameConfig frame{};
    frame.frameConfigSet = frame.entropyEnabled = frame.loqEnabled[LOQ0] = true;
    uint8_t data[] = {0xc0, 63}; // One zero coefficient and 63 following zero TUs.
    LdeChunk chunk{};
    chunk.entropyEnabled = true;
    chunk.rleOnly = true;
    chunk.data = data;
    chunk.size = sizeof(data);
    frame.chunks = &chunk;
    frame.numChunks = 1;
    EXPECT_FALSE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    data[1] = 62;
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    data[1] = 63;
    data[0] = 0xc2; // Nonzero first coefficient.
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    data[0] = 0xc0;
    chunk.size = 1; // Truncated run.
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
    chunk.size = sizeof(data);
    chunk.rleOnly = false;
    EXPECT_TRUE(ldeHasResiduals(&global, &frame, LOQ0, 0));
}

TEST_F(DecodeTemporalOn, DecodeToCpuCmdBuffer)
{
    EXPECT_EQ(ldeCmdBufferCpuInitialize(allocator, &cmdBufferCpu, 0), true);
    EXPECT_EQ(ldeCmdBufferCpuReset(&cmdBufferCpu, globalConfig.numLayers), true);

    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ1, 0, 0, &cmdBufferCpu,
                                     nullptr, nullptr));
    EXPECT_EQ(cmdBufferCpu.count, 19);
    EXPECT_EQ(hashCpuBuffer(), "c0367ce3a91ed34af5040e43d598d8c2");

    EXPECT_EQ(ldeCmdBufferCpuReset(&cmdBufferCpu, globalConfig.numLayers), true);
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, &cmdBufferCpu,
                                     nullptr, nullptr));
    EXPECT_EQ(cmdBufferCpu.count, 344);
    EXPECT_EQ(hashCpuBuffer(), "aff322c6e7b6b99923c89a1a1febd328");

    EXPECT_EQ(getFrame(), true);
    EXPECT_EQ(ldeCmdBufferCpuReset(&cmdBufferCpu, globalConfig.numLayers), true);
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, &cmdBufferCpu,
                                     nullptr, nullptr));
    EXPECT_EQ(cmdBufferCpu.count, 461);
    EXPECT_EQ(hashCpuBuffer(), "e7f1da13d3b99a8a470cd395e7e9c9ff");

    ldeCmdBufferCpuFree(&cmdBufferCpu);
}

TEST_F(DecodeTemporalOn, DecodeToGpuCmdBuffer)
{
    EXPECT_TRUE(ldeCmdBufferGpuInitialize(allocator, &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(globalConfig.numLayers, 4);
    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ1, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 10);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 76);
    EXPECT_EQ(cmdBufferGpu.residualCount, 76);
    EXPECT_EQ(hashGpuBuffer(), "09061005ca2065efc64caeefa3473f34");

    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 16);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 1376);
    EXPECT_EQ(cmdBufferGpu.residualCount, 1376);
    EXPECT_EQ(hashGpuBuffer(), "9baf24d0841659cacdf0e5204d272be6");

    EXPECT_TRUE(getFrame());
    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 18);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 1812);
    EXPECT_EQ(cmdBufferGpu.residualCount, 1812);
    EXPECT_EQ(hashGpuBuffer(), "5f5c3906c5827aca6e27ed3bcfcba054");

    ldeCmdBufferGpuFree(&cmdBufferGpu, &cmdBufferGpuBuilder);
}

TEST_F(DecodeTemporalOff, DecodeToCpuCmdBuffer)
{
    EXPECT_EQ(ldeCmdBufferCpuInitialize(allocator, &cmdBufferCpu, 0), true);
    EXPECT_EQ(ldeCmdBufferCpuReset(&cmdBufferCpu, globalConfig.numLayers), true);

    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ1, 0, 0, &cmdBufferCpu,
                                     nullptr, nullptr));
    EXPECT_EQ(cmdBufferCpu.count, 110);
    EXPECT_EQ(hashCpuBuffer(), "2c3e03aca3071a7fc1602aa0183d8e5d");
}

TEST_F(DecodeTemporalOff, DecodeToGpuCmdBuffer)
{
    EXPECT_TRUE(ldeCmdBufferGpuInitialize(allocator, &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(globalConfig.numLayers, 4);
    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ1, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 9);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 440);
    EXPECT_EQ(cmdBufferGpu.residualCount, 440);
    EXPECT_EQ(hashGpuBuffer(), "6756dfa75d9d89120fa6f22e0d2be6c3");

    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 14);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 804);
    EXPECT_EQ(cmdBufferGpuBuilder.residualSetCount, 0);
    EXPECT_EQ(cmdBufferGpuBuilder.residualClearAndSetCount, 0);
    EXPECT_EQ(cmdBufferGpu.residualCount, 804);
    EXPECT_EQ(hashGpuBuffer(), "45914d5584eca6510c622c352393836c");

    EXPECT_TRUE(getFrame());
    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 14);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 804);
    EXPECT_EQ(cmdBufferGpu.residualCount, 776);
    EXPECT_EQ(hashGpuBuffer(), "d6ab7c0bdb99077d155825d19cf9e98c");

    ldeCmdBufferGpuFree(&cmdBufferGpu, &cmdBufferGpuBuilder);
}

TEST_F(DecodeTemporalOn, InvalidInputs)
{
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ2, 0, 0, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 4, 0, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 1, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr, nullptr, nullptr));
}
