/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "find_assets_dir.h"
#include "LCEVC/utility/bin_reader.h"
#include "LCEVC/utility/filesystem.h"

#include <gtest/gtest.h>
#include <LCEVC/PerseusDecoder.h>

using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/core/test/assets")};

typedef struct cmdBufferSplitInput
{
    uint8_t threads;
    int32_t values[16];
} cmdBufferSplitInput_t;

class APITestCommon
{
protected:
    void SetUp()
    {
        std::unique_ptr<BinReader> reader =
            createBinReader((kTestAssets / "Tunnel_360x200_DDS_8bit_3f.bin").string());
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        reader->read(decodeIndex, presentationIndex, mPayload);
    }
    void TearDown() {}
    std::vector<uint8_t> mPayload;

public:
};

class APITest
    : protected APITestCommon
    , public testing::Test
{
    void SetUp() override { return APITestCommon::SetUp(); }
    void TearDown() override { return APITestCommon::TearDown(); }
};

class APITestCmdBuffersSplit
    : public APITestCommon
    , public testing::TestWithParam<cmdBufferSplitInput_t>
{
    void SetUp() override { return APITestCommon::SetUp(); }
    void TearDown() override { return APITestCommon::TearDown(); }
};

// -----------------------------------------------------------------------------

TEST_F(APITest, GetCmdBuffers)
{
    const uint8_t threads = 2;
    perseus_decoder_config perseusCfg;
    EXPECT_EQ(perseus_decoder_config_init(&perseusCfg), 0);
    perseusCfg.generate_cmdbuffers = true;
    perseusCfg.apply_cmdbuffers_threads = threads;
    perseusCfg.num_worker_threads = 1;

    perseus_decoder decoder = nullptr;
    EXPECT_EQ(perseus_decoder_open(&decoder, &perseusCfg), 0);
    perseus_decoder_stream streamCfg;
    perseus_decoder_parse(decoder, mPayload.data(), mPayload.size(), &streamCfg);
    perseus_image image = {};
    EXPECT_EQ(perseus_decoder_decode_high(decoder, &image), 0);

    perseus_cmdbuffer buffer = {};
    perseus_cmdbuffer_entrypoint entryPoints[threads] = {};
    EXPECT_EQ(perseus_decoder_get_cmd_buffer(decoder, PSS_LOQ_0, 0, 0, &buffer, entryPoints, threads), 0);

    EXPECT_EQ(buffer.type, PSS_CBT_4x4);
    EXPECT_EQ(buffer.count, 730);
    EXPECT_EQ(buffer.data_size, 23360);
    EXPECT_EQ(buffer.command_size, 756);
    EXPECT_NE(buffer.data, nullptr);
    EXPECT_NE(buffer.commands, nullptr);
    EXPECT_EQ(entryPoints[0].count, 368);
    EXPECT_EQ(entryPoints[0].command_offset, 0);
    EXPECT_EQ(entryPoints[0].data_offset, 0);
    EXPECT_EQ(entryPoints[0].initial_jump, 0);
    EXPECT_EQ(entryPoints[1].count, 362);
    EXPECT_EQ(entryPoints[1].command_offset, 380);
    EXPECT_EQ(entryPoints[1].data_offset, 11776);
    EXPECT_EQ(entryPoints[1].initial_jump, 2495);
    EXPECT_EQ(entryPoints[0].count + entryPoints[1].count, buffer.count);

    EXPECT_EQ(perseus_decoder_close(decoder), 0);
}

TEST_P(APITestCmdBuffersSplit, CmdBuffersSplit)
{
    cmdBufferSplitInput_t params = GetParam();
    perseus_decoder_config perseusCfg;
    EXPECT_EQ(perseus_decoder_config_init(&perseusCfg), 0);
    perseusCfg.generate_cmdbuffers = true;
    perseusCfg.apply_cmdbuffers_threads = params.threads;

    perseus_decoder decoder = nullptr;
    EXPECT_EQ(perseus_decoder_open(&decoder, &perseusCfg), 0);
    perseus_decoder_stream streamCfg;
    perseus_decoder_parse(decoder, mPayload.data(), mPayload.size(), &streamCfg);
    perseus_image image = {};
    EXPECT_EQ(perseus_decoder_decode_high(decoder, &image), 0);

    perseus_cmdbuffer buffer = {};
    std::vector<perseus_cmdbuffer_entrypoint> entryPoints(params.threads);
    EXPECT_EQ(perseus_decoder_get_cmd_buffer(decoder, PSS_LOQ_0, 0, 0, &buffer, entryPoints.data(),
                                             params.threads),
              0);

    uint32_t countSum = 0;
    for (uint8_t i = 0; i < params.threads; i++) {
        EXPECT_EQ(entryPoints[i].count, params.values[i]);
        countSum += entryPoints[i].count;
    }
    EXPECT_EQ(countSum, buffer.count);
}

INSTANTIATE_TEST_SUITE_P(
    APITestCmdBuffersSplitValues, APITestCmdBuffersSplit,
    testing::Values(
        cmdBufferSplitInput_t{2, {368, 362}}, cmdBufferSplitInput_t{4, {183, 185, 183, 179}},
        cmdBufferSplitInput_t{6, {129, 116, 123, 119, 119, 124}},
        cmdBufferSplitInput_t{8, {97, 86, 106, 79, 88, 95, 88, 91}},
        cmdBufferSplitInput_t{12, {62, 67, 54, 62, 65, 53, 58, 66, 64, 55, 57, 67}},
        cmdBufferSplitInput_t{16, {62, 35, 49, 37, 62, 28, 45, 45, 50, 38, 72, 28, 39, 49, 49, 42}}));

// -----------------------------------------------------------------------------
