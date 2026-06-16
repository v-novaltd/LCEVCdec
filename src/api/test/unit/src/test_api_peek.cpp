/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

// Define this to use the interface of the API (which normally would be in a dll).
#define VNDisablePublicAPI

#include "asset_loader.h"
#include "data.h"
#include "utils.h"

#include <gtest/gtest.h>
#include <LCEVC/pipeline/pipeline_config_base.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <queue>
#include <thread>

namespace {

constexpr uint64_t kEndPTS = 100;
class APIPeekBaseFixture : public testing::Test
{
protected:
    void SetUpDecoder(lcevc_dec::pipeline::PassthroughMode passthroughMode)
    {
        ASSERT_EQ(LCEVC_CreateDecoder(&m_decoder, {}), LCEVC_Success);
        ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_decoder, "threads", 1), LCEVC_Success);
        ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_decoder, "passthrough_mode",
                                            static_cast<int32_t>(passthroughMode)),
                  LCEVC_Success);
        ASSERT_EQ(LCEVC_InitializeDecoder(m_decoder), LCEVC_Success);

        LCEVC_DefaultPictureDesc(&m_inputDesc, LCEVC_I420_8, 960, 540);
        LCEVC_DefaultPictureDesc(&m_outputDesc, LCEVC_I420_8, 1920, 1080);
    }

    void allocZeroedPicture(const LCEVC_PictureDesc& pictureDesc, LCEVC_PictureHandle* picture)
    {
        ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &pictureDesc, picture), LCEVC_Success);

        LCEVC_PictureLockHandle lock = {};
        ASSERT_EQ(LCEVC_LockPicture(m_decoder, *picture, LCEVC_Access_Write, &lock), LCEVC_Success);

        LCEVC_PictureBufferDesc bufferDesc = {};
        ASSERT_EQ(LCEVC_GetPictureLockBufferDesc(m_decoder, lock, &bufferDesc), LCEVC_Success);
        ASSERT_NE(bufferDesc.data, nullptr);
        memset(bufferDesc.data, 0, bufferDesc.byteSize);
        ASSERT_EQ(LCEVC_UnlockPicture(m_decoder, lock), LCEVC_Success);
    }

    void TearDown() override { LCEVC_DestroyDecoder(m_decoder); }

    LCEVC_DecoderHandle m_decoder = {};
    LCEVC_PictureDesc m_inputDesc = {};
    LCEVC_PictureDesc m_outputDesc = {};
};

void expectPictureDescMatches(const LCEVC_PictureDesc& lhs, const LCEVC_PictureDesc& rhs)
{
    EXPECT_EQ(lhs.width, rhs.width);
    EXPECT_EQ(lhs.height, rhs.height);
    EXPECT_EQ(lhs.colorFormat, rhs.colorFormat);
    EXPECT_EQ(lhs.colorRange, rhs.colorRange);
    EXPECT_EQ(lhs.colorPrimaries, rhs.colorPrimaries);
    EXPECT_EQ(lhs.matrixCoefficients, rhs.matrixCoefficients);
    EXPECT_EQ(lhs.transferCharacteristics, rhs.transferCharacteristics);
    EXPECT_EQ(lhs.sampleAspectRatioNum, rhs.sampleAspectRatioNum);
    EXPECT_EQ(lhs.sampleAspectRatioDen, rhs.sampleAspectRatioDen);
    EXPECT_EQ(lhs.cropTop, rhs.cropTop);
    EXPECT_EQ(lhs.cropBottom, rhs.cropBottom);
    EXPECT_EQ(lhs.cropLeft, rhs.cropLeft);
    EXPECT_EQ(lhs.cropRight, rhs.cropRight);
}

void expectAssetDecodePeekMatchesExpected(LCEVC_DecoderHandle decoder, const std::filesystem::path& media,
                                          uint32_t limit, bool sendEnhancement,
                                          uint32_t expectedPeekWidth, uint32_t expectedPeekHeight,
                                          uint32_t expectedSarNum, uint32_t expectedSarDen)
{
    AssetLoader loader(media, limit);

    for (uint32_t index = 0; index < loader.size(); ++index) {
        const uint64_t pts = loader.timestamp(index);
        const PixelBuffer& base = loader.base(index);
        LCEVC_PictureDesc inputDesc = {};
        ASSERT_EQ(LCEVC_Success,
                  LCEVC_DefaultPictureDesc(&inputDesc, LCEVC_I420_8, base.width, base.height));
        inputDesc.sampleAspectRatioNum = base.sampleAspectRatioNum;
        inputDesc.sampleAspectRatioDen = base.sampleAspectRatioDen;

        if (sendEnhancement) {
            const std::vector<uint8_t>& enhancement = loader.enhancement(index);
            ASSERT_EQ(feedNalUnit(decoder, enhancement, pts, kFormatAnnexB, kCodecH264), LCEVC_Success);
        }

        LCEVC_PictureHandle basePicture = allocPictureForPixelBuffer(decoder, base, &inputDesc);
        ASSERT_EQ(LCEVC_SendDecoderBase(decoder, pts, basePicture, UINT32_MAX, nullptr), LCEVC_Success);

        uint32_t peekWidth = 0;
        uint32_t peekHeight = 0;
        ASSERT_EQ(LCEVC_PeekDecoder(decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);
        EXPECT_EQ(peekWidth, expectedPeekWidth);
        EXPECT_EQ(peekHeight, expectedPeekHeight);

        LCEVC_PictureDesc peekDesc = {};
        ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(decoder, pts, &peekDesc), LCEVC_Success);
        EXPECT_EQ(peekDesc.width, peekWidth);
        EXPECT_EQ(peekDesc.height, peekHeight);
        EXPECT_EQ(peekDesc.sampleAspectRatioNum, expectedSarNum);
        EXPECT_EQ(peekDesc.sampleAspectRatioDen, expectedSarDen);

        LCEVC_PictureDesc outputDesc = {};
        LCEVC_DefaultPictureDesc(&outputDesc, LCEVC_I420_8, peekWidth, peekHeight);

        LCEVC_PictureHandle output = {};
        ASSERT_EQ(LCEVC_AllocPicture(decoder, &outputDesc, &output), LCEVC_Success);
        ASSERT_EQ(LCEVC_SendDecoderPicture(decoder, output), LCEVC_Success);
        ASSERT_EQ(LCEVC_SynchronizeDecoder(decoder, false), LCEVC_Success);

        LCEVC_DecodeInformation info = {};
        LCEVC_PictureHandle receivedOutput = output;
        ASSERT_EQ(LCEVC_ReceiveDecoderPicture(decoder, &receivedOutput, &info), LCEVC_Success);

        LCEVC_PictureDesc returnedDesc = {};
        ASSERT_EQ(LCEVC_GetPictureDesc(decoder, receivedOutput, &returnedDesc), LCEVC_Success);
        expectPictureDescMatches(peekDesc, returnedDesc);

        LCEVC_PictureHandle doneBase = {};
        ASSERT_EQ(LCEVC_ReceiveDecoderBase(decoder, &doneBase), LCEVC_Success);
        EXPECT_EQ(doneBase.hdl, basePicture.hdl);

        ASSERT_EQ(LCEVC_FreePicture(decoder, doneBase), LCEVC_Success);
        ASSERT_EQ(LCEVC_FreePicture(decoder, receivedOutput), LCEVC_Success);
    }
}

class APIPeekFixture : public APIPeekBaseFixture
{
public:
    void SetUp() override { SetUpDecoder(lcevc_dec::pipeline::PassthroughMode::Allow); }
};

class APIPeekDisallowPassthroughFixture : public APIPeekBaseFixture
{
public:
    void SetUp() override { SetUpDecoder(lcevc_dec::pipeline::PassthroughMode::Disable); }
};

TEST_F(APIPeekFixture, DecodeLoop)
{
    std::queue<LCEVC_PictureHandle> availableBases = {};
    std::queue<LCEVC_PictureHandle> availableOutputs = {};
    std::queue<LCEVC_PictureHandle> queuedOutputs = {};
    std::queue<uint64_t> queuedPts = {};
    std::map<uint64_t, LCEVC_PictureDesc> peekedDescs = {};
    uint32_t outputsReceived = 0;

    for (uint64_t pts = 0; pts < kEndPTS; ++pts) {
        const EnhancementWithData enhancement =
            getEnhancement(static_cast<int64_t>(pts), kValidEnhancements);
        ASSERT_EQ(LCEVC_SendDecoderEnhancementData(m_decoder, pts, enhancement.first, enhancement.second),
                  LCEVC_Success);

        LCEVC_PictureHandle base = {};
        if (availableBases.empty()) {
            ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &m_inputDesc, &base), LCEVC_Success);
        } else {
            base = availableBases.front();
            availableBases.pop();
        }

        LCEVC_PictureLockHandle lock = {};
        ASSERT_EQ(LCEVC_LockPicture(m_decoder, base, LCEVC_Access_Write, &lock), LCEVC_Success);

        LCEVC_PictureBufferDesc bufferDesc = {};
        ASSERT_EQ(LCEVC_GetPictureLockBufferDesc(m_decoder, lock, &bufferDesc), LCEVC_Success);
        ASSERT_NE(bufferDesc.data, nullptr);
        memset(bufferDesc.data, 0, bufferDesc.byteSize);
        ASSERT_EQ(LCEVC_UnlockPicture(m_decoder, lock), LCEVC_Success);

        ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Success);

        uint32_t peekWidth = 0;
        uint32_t peekHeight = 0;
        const LCEVC_ReturnCode peekRes = LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight);
        ASSERT_EQ(peekRes, LCEVC_Success);
        EXPECT_EQ(peekWidth, m_outputDesc.width);
        EXPECT_EQ(peekHeight, m_outputDesc.height);

        LCEVC_PictureDesc peekPictureDesc = {};
        const LCEVC_ReturnCode peekDescRes =
            LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc);
        EXPECT_EQ(peekDescRes, peekRes);
        EXPECT_EQ(peekPictureDesc.width, peekWidth);
        EXPECT_EQ(peekPictureDesc.height, peekHeight);
        peekedDescs[pts] = peekPictureDesc;

        LCEVC_PictureDesc outputDesc = {};
        LCEVC_DefaultPictureDesc(&outputDesc, LCEVC_I420_8, peekWidth, peekHeight);

        LCEVC_PictureHandle output = {};
        if (availableOutputs.empty()) {
            ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &outputDesc, &output), LCEVC_Success);
        } else {
            output = availableOutputs.front();
            availableOutputs.pop();
        }
        ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, output), LCEVC_Success);

        queuedOutputs.push(output);
        queuedPts.push(pts);

        LCEVC_PictureHandle receivedOutput = queuedOutputs.front();
        LCEVC_DecodeInformation info = {};
        LCEVC_ReturnCode res = LCEVC_ReceiveDecoderPicture(m_decoder, &receivedOutput, &info);
        if (res == LCEVC_Success) {
            ASSERT_FALSE(queuedPts.empty());
            EXPECT_EQ(info.timestamp, queuedPts.front());
            EXPECT_TRUE(info.hasEnhancement);
            EXPECT_TRUE(info.enhanced);
            queuedPts.pop();

            LCEVC_PictureDesc returnedDesc = {};
            ASSERT_EQ(LCEVC_GetPictureDesc(m_decoder, receivedOutput, &returnedDesc), LCEVC_Success);
            EXPECT_EQ(returnedDesc.width, peekWidth);
            EXPECT_EQ(returnedDesc.height, peekHeight);
            ASSERT_TRUE(peekedDescs.find(info.timestamp) != peekedDescs.end());
            EXPECT_EQ(memcmp(&returnedDesc, &peekedDescs.at(info.timestamp), sizeof(LCEVC_PictureDesc)), 0);

            queuedOutputs.pop();
            availableOutputs.push(receivedOutput);
            outputsReceived++;
        } else {
            EXPECT_EQ(res, LCEVC_Again);
        }

        LCEVC_PictureHandle doneBase = {};
        while (LCEVC_ReceiveDecoderBase(m_decoder, &doneBase) == LCEVC_Success) {
            availableBases.push(doneBase);
        }
    }

    ASSERT_EQ(LCEVC_SynchronizeDecoder(m_decoder, false), LCEVC_Success);

    while (!queuedOutputs.empty()) {
        LCEVC_PictureHandle receivedOutput = queuedOutputs.front();
        LCEVC_DecodeInformation info = {};
        LCEVC_ReturnCode res = LCEVC_ReceiveDecoderPicture(m_decoder, &receivedOutput, &info);
        if (res == LCEVC_Success) {
            ASSERT_FALSE(queuedPts.empty());
            EXPECT_EQ(info.timestamp, queuedPts.front());
            EXPECT_TRUE(info.hasEnhancement);
            EXPECT_TRUE(info.enhanced);
            queuedPts.pop();

            LCEVC_PictureDesc returnedDesc = {};
            ASSERT_EQ(LCEVC_GetPictureDesc(m_decoder, receivedOutput, &returnedDesc), LCEVC_Success);
            EXPECT_EQ(returnedDesc.width, m_outputDesc.width);
            EXPECT_EQ(returnedDesc.height, m_outputDesc.height);
            ASSERT_TRUE(peekedDescs.find(info.timestamp) != peekedDescs.end());
            EXPECT_EQ(memcmp(&returnedDesc, &peekedDescs.at(info.timestamp), sizeof(LCEVC_PictureDesc)), 0);

            queuedOutputs.pop();
            availableOutputs.push(receivedOutput);
            outputsReceived++;
        } else {
            EXPECT_EQ(res, LCEVC_Again);
        }

        LCEVC_PictureHandle doneBase = {};
        while (LCEVC_ReceiveDecoderBase(m_decoder, &doneBase) == LCEVC_Success) {
            availableBases.push(doneBase);
        }
    }

    EXPECT_EQ(outputsReceived, kEndPTS);
}

TEST_F(APIPeekFixture, BaseOnlyFrameHasBaseDimensions)
{
    const uint64_t pts = 1000;

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);
    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Success);

    uint32_t peekWidth = 0;
    uint32_t peekHeight = 0;
    const LCEVC_ReturnCode peekRes = LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight);
    ASSERT_EQ(peekRes, LCEVC_Success);
    EXPECT_EQ(peekWidth, m_inputDesc.width);
    EXPECT_EQ(peekHeight, m_inputDesc.height);

    LCEVC_PictureDesc peekPictureDesc = {};
    const LCEVC_ReturnCode peekDescRes = LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc);
    EXPECT_EQ(peekDescRes, peekRes);
    EXPECT_EQ(peekPictureDesc.width, peekWidth);
    EXPECT_EQ(peekPictureDesc.height, peekHeight);

    LCEVC_PictureDesc outputDesc = {};
    LCEVC_DefaultPictureDesc(&outputDesc, LCEVC_I420_8, peekWidth, peekHeight);

    LCEVC_PictureHandle output = {};
    ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &outputDesc, &output), LCEVC_Success);
    ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, output), LCEVC_Success);

    ASSERT_EQ(LCEVC_SynchronizeDecoder(m_decoder, false), LCEVC_Success);

    LCEVC_DecodeInformation info = {};
    ASSERT_EQ(LCEVC_ReceiveDecoderPicture(m_decoder, &output, &info), LCEVC_Success);

    LCEVC_PictureDesc returnedDesc = {};
    ASSERT_EQ(LCEVC_GetPictureDesc(m_decoder, output, &returnedDesc), LCEVC_Success);
    EXPECT_EQ(returnedDesc.width, m_inputDesc.width);
    EXPECT_EQ(returnedDesc.height, m_inputDesc.height);
}

TEST_F(APIPeekFixture, FlushAfterSuccessfulPeekReturnsFlushed)
{
    const uint64_t pts = 2000;

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);
    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Success);

    uint32_t peekWidth = 0;
    uint32_t peekHeight = 0;
    ASSERT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);

    LCEVC_PictureDesc peekPictureDesc = {};
    ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_Success);

    LCEVC_PictureDesc outputDesc = {};
    LCEVC_DefaultPictureDesc(&outputDesc, LCEVC_I420_8, peekWidth, peekHeight);

    LCEVC_PictureHandle output = {};
    ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &outputDesc, &output), LCEVC_Success);
    ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, output), LCEVC_Success);

    ASSERT_EQ(LCEVC_FlushDecoder(m_decoder), LCEVC_Success);

    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_Flushed);
}

TEST_F(APIPeekFixture, PeekReturnsNotFoundForUnsentOrReturnedTimestamp)
{
    const uint64_t unsentPts = 3000;
    const uint64_t sentPts = 3001;

    uint32_t width = 0;
    uint32_t height = 0;
    LCEVC_PictureDesc peekDesc = {};

    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, unsentPts, &width, &height), LCEVC_NotFound);
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, unsentPts, &peekDesc), LCEVC_NotFound);

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);
    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, sentPts, base, UINT32_MAX, nullptr), LCEVC_Success);

    ASSERT_EQ(LCEVC_PeekDecoder(m_decoder, sentPts, &width, &height), LCEVC_Success);
    ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, sentPts, &peekDesc), LCEVC_Success);

    LCEVC_PictureDesc outputDesc = {};
    LCEVC_DefaultPictureDesc(&outputDesc, LCEVC_I420_8, width, height);

    LCEVC_PictureHandle output = {};
    ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &outputDesc, &output), LCEVC_Success);
    ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, output), LCEVC_Success);
    ASSERT_EQ(LCEVC_SynchronizeDecoder(m_decoder, false), LCEVC_Success);

    LCEVC_DecodeInformation info = {};
    ASSERT_EQ(LCEVC_ReceiveDecoderPicture(m_decoder, &output, &info), LCEVC_Success);
    EXPECT_EQ(info.timestamp, sentPts);

    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, sentPts, &width, &height), LCEVC_NotFound);
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, sentPts, &peekDesc), LCEVC_NotFound);
}

TEST_F(APIPeekFixture, PeekResolutionReturnsSuccessAfterEnhancementBeforeBase)
{
    const uint64_t pts = 4000;
    const EnhancementWithData enhancement = getEnhancement(static_cast<int64_t>(pts), kValidEnhancements);
    ASSERT_EQ(LCEVC_SendDecoderEnhancementData(m_decoder, pts, enhancement.first, enhancement.second),
              LCEVC_Success);

    uint32_t peekWidth = 0;
    uint32_t peekHeight = 0;
    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);
    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Success);

    ASSERT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);
}

TEST_F(APIPeekFixture, PeekPictureReturnsAgainAfterEnhancementBeforeBase)
{
    const uint64_t pts = 4001;
    const EnhancementWithData enhancement = getEnhancement(static_cast<int64_t>(pts), kValidEnhancements);
    ASSERT_EQ(LCEVC_SendDecoderEnhancementData(m_decoder, pts, enhancement.first, enhancement.second),
              LCEVC_Success);

    uint32_t peekWidth = 0;
    uint32_t peekHeight = 0;
    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);

    LCEVC_PictureDesc peekPictureDesc = {};
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_Again);

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);
    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Success);

    ASSERT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &peekWidth, &peekHeight), LCEVC_Success);
    ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_Success);
    EXPECT_EQ(peekPictureDesc.width, peekWidth);
    EXPECT_EQ(peekPictureDesc.height, peekHeight);
}

TEST_F(APIPeekDisallowPassthroughFixture, PeekPictureDescReturnsErrorWhenEnhancementMissingAndPassthroughDisabled)
{
    const uint64_t pts = 5000;

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);

    // Per API header contract, missing enhancement with passthrough disabled should
    // yield LCEVC_NotFound from PeekDecoderPictureDesc for this timestamp.
    EXPECT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, UINT32_MAX, nullptr), LCEVC_Again);

    LCEVC_PictureDesc peekPictureDesc = {};
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_NotFound);

    LCEVC_PictureHandle doneBase = {};
    while (LCEVC_ReceiveDecoderBase(m_decoder, &doneBase) == LCEVC_Success) {
        EXPECT_EQ(LCEVC_FreePicture(m_decoder, doneBase), LCEVC_Success);
    }
}

TEST_F(APIPeekFixture, PeekReturnsTimeoutAfterBaseTimeoutExpires)
{
    const uint64_t pts = 6000;
    const EnhancementWithData enhancement = getEnhancement(static_cast<int64_t>(pts), kValidEnhancements);
    ASSERT_EQ(LCEVC_SendDecoderEnhancementData(m_decoder, pts, enhancement.first, enhancement.second),
              LCEVC_Success);

    LCEVC_PictureHandle base = {};
    allocZeroedPicture(m_inputDesc, &base);

    ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, base, 1, nullptr), LCEVC_Success);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    uint32_t width = 0;
    uint32_t height = 0;
    EXPECT_EQ(LCEVC_PeekDecoder(m_decoder, pts, &width, &height), LCEVC_Success);

    LCEVC_PictureDesc peekPictureDesc = {};
    EXPECT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekPictureDesc), LCEVC_Timeout);

    EXPECT_EQ(LCEVC_FlushDecoder(m_decoder), LCEVC_Success);
    LCEVC_PictureHandle doneBase = {};
    while (LCEVC_ReceiveDecoderBase(m_decoder, &doneBase) == LCEVC_Success) {
        EXPECT_EQ(LCEVC_FreePicture(m_decoder, doneBase), LCEVC_Success);
    }
}

TEST_F(APIPeekFixture, AssetDecodePeekMatchesExpectedFor2DStreamEnhanced)
{
    expectAssetDecodePeekMatchesExpected(m_decoder, kTestAssets / "elfuente_640x360_12f.ts", 6,
                                         true, 640u, 360u, 1u, 1u);
}

TEST_F(APIPeekFixture, AssetDecodePeekMatchesExpectedFor1DStreamEnhanced)
{
    expectAssetDecodePeekMatchesExpected(m_decoder, kTestAssets / "elfuente_1D_640x360_12f.ts", 6,
                                         true, 640u, 360u, 1u, 1u);
}

TEST_F(APIPeekFixture, AssetDecodePeekMatchesExpectedFor2DStreamBaseOnly)
{
    expectAssetDecodePeekMatchesExpected(m_decoder, kTestAssets / "elfuente_640x360_12f.ts", 6,
                                         false, 320u, 180u, 1u, 1u);
}

TEST_F(APIPeekFixture, AssetDecodePeekMatchesExpectedFor1DStreamBaseOnly)
{
    expectAssetDecodePeekMatchesExpected(m_decoder, kTestAssets / "elfuente_1D_640x360_12f.ts", 6,
                                         false, 320u, 360u, 2u, 1u);
}

TEST_F(APIPeekFixture, AssetDecodePeekReportsEnhancedBitdepthForReducedBitdepthStream)
{
    // The base of this stream is 8-bit while the enhanced output is 10-bit, so the peeked
    // picture description must report the enhanced (10-bit) color format.
    AssetLoader loader(kTestAssets / "elfuente_360x200_3f_rbd.ts", 3);

    ASSERT_GT(loader.size(), 0u);

    for (uint32_t index = 0; index < loader.size(); ++index) {
        const uint64_t pts = loader.timestamp(index);
        const PixelBuffer& base = loader.base(index);

        // The base picture is fed as 8-bit.
        LCEVC_PictureDesc inputDesc = {};
        ASSERT_EQ(LCEVC_Success,
                  LCEVC_DefaultPictureDesc(&inputDesc, LCEVC_I420_8, base.width, base.height));
        EXPECT_EQ(inputDesc.colorFormat, LCEVC_I420_8);

        const std::vector<uint8_t>& enhancement = loader.enhancement(index);
        ASSERT_EQ(feedNalUnit(m_decoder, enhancement, pts, kFormatAnnexB, kCodecH264), LCEVC_Success);

        LCEVC_PictureHandle basePicture = allocPictureForPixelBuffer(m_decoder, base, &inputDesc);
        ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, basePicture, UINT32_MAX, nullptr), LCEVC_Success);

        // The peeked output description must report the 10-bit enhanced format.
        LCEVC_PictureDesc peekDesc = {};
        ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &peekDesc), LCEVC_Success);
        EXPECT_EQ(peekDesc.colorFormat, LCEVC_I420_10_LE);

        LCEVC_PictureHandle output = {};
        ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &peekDesc, &output), LCEVC_Success);
        ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, output), LCEVC_Success);
        ASSERT_EQ(LCEVC_SynchronizeDecoder(m_decoder, false), LCEVC_Success);

        LCEVC_DecodeInformation info = {};
        LCEVC_PictureHandle receivedOutput = output;
        ASSERT_EQ(LCEVC_ReceiveDecoderPicture(m_decoder, &receivedOutput, &info), LCEVC_Success);

        LCEVC_PictureDesc returnedDesc = {};
        ASSERT_EQ(LCEVC_GetPictureDesc(m_decoder, receivedOutput, &returnedDesc), LCEVC_Success);
        EXPECT_EQ(returnedDesc.colorFormat, LCEVC_I420_10_LE);
        expectPictureDescMatches(peekDesc, returnedDesc);

        LCEVC_PictureHandle doneBase = {};
        ASSERT_EQ(LCEVC_ReceiveDecoderBase(m_decoder, &doneBase), LCEVC_Success);
        EXPECT_EQ(doneBase.hdl, basePicture.hdl);

        ASSERT_EQ(LCEVC_FreePicture(m_decoder, doneBase), LCEVC_Success);
        ASSERT_EQ(LCEVC_FreePicture(m_decoder, receivedOutput), LCEVC_Success);
    }
}

} // namespace
