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
#include <LCEVC/enhancement/config_pool.h>
#include <LCEVC/utility/bin_reader.h>

#include <filesystem>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets = findAssetsDir("src/enhancement/test/assets");

class ConfigPoolTest : public testing::Test
{
public:
    void SetUp() override
    {
        ldcDiagnosticsInitialize(NULL);
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
        allocator = ldcMemoryAllocatorMalloc();

        ldeConfigPoolInitialize(allocator, &configPool, BitstreamVersionUnspecified);
        m_binReader = createBinReader((kTestAssets / "parse_gops.bin").string());
    }

    void TearDown() override { ldeConfigPoolRelease(&configPool); }

    std::vector<uint8_t> getFrame()
    {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;

        m_binReader->read(decodeIndex, presentationIndex, payload);
        return payload;
    }

    LdcMemoryAllocator* allocator;
    LdeConfigPool configPool = {};

private:
    std::unique_ptr<BinReader> m_binReader;
};

TEST_F(ConfigPoolTest, singleFrame)
{
    auto frame = getFrame();
    LdeGlobalConfig* globalConfigPtr = nullptr;
    LdeFrameConfig frameConfig = {};

    EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, 0, frame.data(), frame.size(),
                                         &globalConfigPtr, &frameConfig));

    EXPECT_EQ(frameConfig.globalConfigSet, true);
    EXPECT_EQ(frameConfig.frameConfigSet, true);
    EXPECT_EQ(frameConfig.chunkAllocation.size, 256);

    EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &frameConfig, globalConfigPtr));
}

TEST_F(ConfigPoolTest, individualFrames)
{
    EXPECT_EQ(configPool.quantMatrix.set, false);
    {
        auto frame = getFrame();
        LdeGlobalConfig* globalConfigPtr = nullptr;
        LdeFrameConfig frameConfig = {};

        EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, 0, frame.data(), frame.size(),
                                             &globalConfigPtr, &frameConfig));
        EXPECT_EQ(frameConfig.globalConfigSet, true);
        EXPECT_EQ(ldcVectorSize(&configPool.globalConfigs), 1);
        EXPECT_EQ(configPool.quantMatrix.set, true);
        EXPECT_EQ(frameConfig.quantMatrix.set, true);
        EXPECT_EQ(frameConfig.quantMatrix.values[LOQ1][0], 0);
        EXPECT_EQ(frameConfig.quantMatrix.values[LOQ1][1], 0);
        EXPECT_EQ(frameConfig.quantMatrix.values[LOQ1][2], 0);
        EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &frameConfig, globalConfigPtr));
    }

    {
        auto frame = getFrame();
        LdeGlobalConfig* globalConfigPtr = nullptr;
        LdeFrameConfig frameConfig = {};

        EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, 1, frame.data(), frame.size(),
                                             &globalConfigPtr, &frameConfig));
        EXPECT_EQ(frameConfig.globalConfigSet, false);
        EXPECT_EQ(ldcVectorSize(&configPool.globalConfigs), 1);
        EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &frameConfig, globalConfigPtr));
    }

    {
        auto frame = getFrame();
        LdeGlobalConfig* globalConfigPtr = nullptr;
        LdeFrameConfig frameConfig = {};

        EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, 2, frame.data(), frame.size(),
                                             &globalConfigPtr, &frameConfig));
        EXPECT_EQ(frameConfig.globalConfigSet, true);
        EXPECT_LE(ldcVectorSize(&configPool.globalConfigs), 2);
        EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &frameConfig, globalConfigPtr));
    }
}

TEST_F(ConfigPoolTest, eachFrameRelease)
{
    auto frame = getFrame();
    uint64_t timestamp = 0;
    while (!frame.empty()) {
        LdeGlobalConfig* globalConfigPtr = nullptr;
        LdeFrameConfig frameConfig = {};

        EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, timestamp, frame.data(), frame.size(),
                                             &globalConfigPtr, &frameConfig));

        EXPECT_LE(ldcVectorSize(&configPool.globalConfigs), 2);
        EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &frameConfig, globalConfigPtr));
        timestamp++;
        frame = getFrame();
    }
    EXPECT_EQ(timestamp, 5);
}

TEST_F(ConfigPoolTest, allFramesRelease)
{
    auto frame = getFrame();
    uint64_t timestamp = 0;
    struct ConfigFrame
    {
        LdeGlobalConfig* globalPtr = nullptr;
        LdeFrameConfig frame = {};
    };
    std::map<uint64_t, ConfigFrame> configFrames;

    while (!frame.empty()) {
        ConfigFrame configFrame{};

        EXPECT_TRUE(ldeConfigPoolFrameInsert(&configPool, timestamp, frame.data(), frame.size(),
                                             &configFrame.globalPtr, &configFrame.frame));
        configFrames[timestamp] = configFrame;
        timestamp++;
        frame = getFrame();
    }
    EXPECT_EQ(timestamp, 5);

    EXPECT_LE(ldcVectorSize(&configPool.globalConfigs), 3);

    for (auto& [k, v] : configFrames) {
        EXPECT_TRUE(ldeConfigPoolFrameRelease(&configPool, &v.frame, v.globalPtr));
    }

    EXPECT_LE(ldcVectorSize(&configPool.globalConfigs), 1);
}
