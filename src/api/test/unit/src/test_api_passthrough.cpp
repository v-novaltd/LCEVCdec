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

#define VNDisablePublicAPI

#include "asset_loader.h"

#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/pipeline_config_base.h>

#include <cstdint>
#include <string>

namespace {

constexpr uint32_t kNumFrames = 200;

// Compare a decoded (I420 8-bit) output picture against the base it was passed through from,
// sample-for-sample, ignoring stride differences.
void expectPassthroughMatchesBase(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle output,
                                  const PixelBuffer& base, uint64_t pts)
{
    LCEVC_PictureLockHandle lock = {};
    ASSERT_EQ(LCEVC_LockPicture(decoder, output, LCEVC_Access_Read, &lock), LCEVC_Success);

    const uint32_t planeWidths[3] = {base.width, base.width / 2, base.width / 2};
    const uint32_t planeHeights[3] = {base.height, base.height / 2, base.height / 2};

    // The base buffer is packed contiguously plane after plane, each using its own row stride.
    const uint8_t* baseData = base.data.data();
    for (uint32_t plane = 0; plane < 3; ++plane) {
        LCEVC_PicturePlaneDesc outputPlane = {};
        ASSERT_EQ(LCEVC_GetPictureLockPlaneDesc(decoder, lock, plane, &outputPlane), LCEVC_Success);

        const uint8_t* outputRow = outputPlane.firstSample;
        const uint8_t* baseRow = baseData;
        for (uint32_t row = 0; row < planeHeights[plane]; ++row) {
            if (memcmp(outputRow, baseRow, planeWidths[plane]) != 0) {
                // Find the first differing sample and report it in full, then stop - comparing
                // every remaining byte of every frame would bury the useful detail in spam.
                uint32_t col = 0;
                while (col < planeWidths[plane] && outputRow[col] == baseRow[col]) {
                    ++col;
                }
                ADD_FAILURE() << "passthrough output differs from base: pts=" << pts
                              << " plane=" << plane << " row=" << row << " col=" << col
                              << " output=" << static_cast<int>(outputRow[col])
                              << " base=" << static_cast<int>(baseRow[col])
                              << " (outputStride=" << outputPlane.rowByteStride
                              << " baseStride=" << base.strides[plane] << ")";
                ASSERT_EQ(LCEVC_UnlockPicture(decoder, lock), LCEVC_Success);
                return;
            }
            outputRow += outputPlane.rowByteStride;
            baseRow += base.strides[plane];
        }
        baseData += base.strides[plane] * planeHeights[plane];
    }

    ASSERT_EQ(LCEVC_UnlockPicture(decoder, lock), LCEVC_Success);
}

// Copy the samples of a base PixelBuffer into a decoder-allocated (I420 8-bit) picture, honouring
// the picture's real per-plane layout.
void fillPictureFromBase(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, const PixelBuffer& base)
{
    LCEVC_PictureLockHandle lock = {};
    ASSERT_EQ(LCEVC_LockPicture(decoder, picture, LCEVC_Access_Write, &lock), LCEVC_Success);

    const uint32_t planeWidths[3] = {base.width, base.width / 2, base.width / 2};
    const uint32_t planeHeights[3] = {base.height, base.height / 2, base.height / 2};

    const uint8_t* baseData = base.data.data();
    for (uint32_t plane = 0; plane < 3; ++plane) {
        LCEVC_PicturePlaneDesc dst = {};
        ASSERT_EQ(LCEVC_GetPictureLockPlaneDesc(decoder, lock, plane, &dst), LCEVC_Success);

        uint8_t* dstRow = dst.firstSample;
        const uint8_t* srcRow = baseData;
        for (uint32_t row = 0; row < planeHeights[plane]; ++row) {
            memcpy(dstRow, srcRow, planeWidths[plane]);
            dstRow += dst.rowByteStride;
            srcRow += base.strides[plane];
        }
        baseData += base.strides[plane] * planeHeights[plane];
    }

    ASSERT_EQ(LCEVC_UnlockPicture(decoder, lock), LCEVC_Success);
}

class APIPassthroughFixture : public testing::TestWithParam<const char*>
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(LCEVC_CreateDecoder(&m_decoder, {}), LCEVC_Success);
        if (LCEVC_ConfigureDecoderString(m_decoder, "pipeline", GetParam()) != LCEVC_Success) {
            GTEST_SKIP() << "Pipeline '" << GetParam() << "' is not available in this build";
        }
        ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_decoder, "threads", 1), LCEVC_Success);
        ASSERT_EQ(LCEVC_ConfigureDecoderInt(m_decoder, "passthrough_mode",
                                            static_cast<int32_t>(lcevc_dec::pipeline::PassthroughMode::Allow)),
                  LCEVC_Success);
        if (LCEVC_InitializeDecoder(m_decoder) != LCEVC_Success) {
            GTEST_SKIP() << "Pipeline '" << GetParam()
                         << "' is missing the required runtime hardware to initialize";
        }
    }

    void TearDown() override { LCEVC_DestroyDecoder(m_decoder); }

    LCEVC_DecoderHandle m_decoder = {};
};

INSTANTIATE_TEST_SUITE_P(Pipelines, APIPassthroughFixture, testing::Values("cpu", "vulkan"),
                         [](const testing::TestParamInfo<const char*>& info) {
                             return std::string(info.param);
                         });

TEST_P(APIPassthroughFixture, NullEnhancementPassesBaseThrough)
{
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.ts", 1);
    ASSERT_GT(loader.size(), 0u);

    const PixelBuffer& base = loader.base(0);

    LCEVC_PictureDesc inputDesc = {};
    ASSERT_EQ(LCEVC_Success, LCEVC_DefaultPictureDesc(&inputDesc, LCEVC_I420_8, base.width, base.height));
    inputDesc.sampleAspectRatioNum = base.sampleAspectRatioNum;
    inputDesc.sampleAspectRatioDen = base.sampleAspectRatioDen;

    // Allocate the base picture through the decoder and fill it once - it is handed back after each
    // frame via ReceiveDecoderBase, so we can reuse it for every timestamp.
    LCEVC_PictureHandle basePicture = {};
    ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &inputDesc, &basePicture), LCEVC_Success);
    fillPictureFromBase(m_decoder, basePicture, base);
    ASSERT_FALSE(HasFatalFailure());

    for (uint64_t pts = 0; pts < kNumFrames; ++pts) {
        // No enhancement data for this timestamp -> expect passthrough of the base.
        ASSERT_EQ(LCEVC_SendDecoderEnhancementData(m_decoder, pts, nullptr, 0), LCEVC_Success);

        ASSERT_EQ(LCEVC_SendDecoderBase(m_decoder, pts, basePicture, UINT32_MAX, nullptr), LCEVC_Success);

        // Passthrough keeps the base resolution.
        LCEVC_PictureDesc outputDesc = {};
        ASSERT_EQ(LCEVC_PeekDecoderPictureDesc(m_decoder, pts, &outputDesc), LCEVC_Success);
        EXPECT_EQ(outputDesc.width, base.width);
        EXPECT_EQ(outputDesc.height, base.height);

        // Decoder-allocated output picture, so the GPU pipelines can write to it directly.
        LCEVC_PictureHandle outputPicture = {};
        ASSERT_EQ(LCEVC_AllocPicture(m_decoder, &outputDesc, &outputPicture), LCEVC_Success);
        ASSERT_EQ(LCEVC_SendDecoderPicture(m_decoder, outputPicture), LCEVC_Success);

        ASSERT_EQ(LCEVC_SynchronizeDecoder(m_decoder, false), LCEVC_Success);

        LCEVC_DecodeInformation info = {};
        LCEVC_PictureHandle receivedOutput = outputPicture;
        ASSERT_EQ(LCEVC_ReceiveDecoderPicture(m_decoder, &receivedOutput, &info), LCEVC_Success);
        EXPECT_EQ(receivedOutput.hdl, outputPicture.hdl);
        EXPECT_EQ(info.timestamp, pts);
        EXPECT_FALSE(info.hasEnhancement);

        // The output must be a sample-exact copy of the base picture we fed in (padding aside).
        expectPassthroughMatchesBase(m_decoder, receivedOutput, base, pts);

        LCEVC_PictureHandle doneBase = {};
        ASSERT_EQ(LCEVC_ReceiveDecoderBase(m_decoder, &doneBase), LCEVC_Success);
        EXPECT_EQ(doneBase.hdl, basePicture.hdl);

        // The base is reused for the next frame; only the output is freed here.
        ASSERT_EQ(LCEVC_FreePicture(m_decoder, receivedOutput), LCEVC_Success);

        if (HasFatalFailure() || HasNonfatalFailure()) {
            break; // Stop at the first bad frame
        }
    }

    ASSERT_EQ(LCEVC_FreePicture(m_decoder, basePicture), LCEVC_Success);
}

} // namespace
