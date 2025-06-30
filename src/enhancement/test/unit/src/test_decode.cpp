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

#include <find_assets_dir.h>
#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/utility/bin_reader.h>
#include <LCEVC/utility/md5.h>

#include <filesystem>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets = findAssetsDir("src/enhancement/test/assets");

class Decode : public testing::Test
{
public:
    void SetUp() override
    {
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        allocator = ldcMemoryAllocatorMalloc();
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);
        ldeFrameConfigInitialize(allocator, &frameConfig);
        frameConfig.chunkAllocation = chunkAllocation;

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

private:
    std::unique_ptr<BinReader> m_binReader = createBinReader((kTestAssets / "decode.bin").string());
};

TEST_F(Decode, DecodeToCpuCmdBuffer)
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
    EXPECT_EQ(hashCpuBuffer(), "6b5b6fdfa8d147d7e286b9b336d278eb");

    EXPECT_EQ(getFrame(), true);
    EXPECT_EQ(ldeCmdBufferCpuReset(&cmdBufferCpu, globalConfig.numLayers), true);
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, &cmdBufferCpu,
                                     nullptr, nullptr));
    EXPECT_EQ(cmdBufferCpu.count, 461);
    EXPECT_EQ(hashCpuBuffer(), "e7f1da13d3b99a8a470cd395e7e9c9ff");

    ldeCmdBufferCpuFree(&cmdBufferCpu);
}

TEST_F(Decode, DecodeToGpuCmdBuffer)
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
    EXPECT_EQ(hashGpuBuffer(), "dfd6949400d7156d2953af11ea640d28");

    EXPECT_TRUE(getFrame());
    EXPECT_TRUE(ldeCmdBufferGpuReset(&cmdBufferGpu, &cmdBufferGpuBuilder, globalConfig.numLayers));
    EXPECT_TRUE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr,
                                     &cmdBufferGpu, &cmdBufferGpuBuilder));
    EXPECT_EQ(cmdBufferGpu.commandCount, 18);
    EXPECT_EQ(cmdBufferGpuBuilder.residualCapacity, 1812);
    EXPECT_EQ(cmdBufferGpu.residualCount, 1812);
    EXPECT_EQ(hashGpuBuffer(), "7c9b27aeba8d489e0839dd545e7d9b5e");

    ldeCmdBufferGpuFree(&cmdBufferGpu, &cmdBufferGpuBuilder);
}

TEST_F(Decode, InvalidInputs)
{
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ2, 0, 0, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 4, 0, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 1, &cmdBufferCpu,
                                      nullptr, nullptr));
    EXPECT_FALSE(ldeDecodeEnhancement(&globalConfig, &frameConfig, LOQ0, 0, 0, nullptr, nullptr, nullptr));
}