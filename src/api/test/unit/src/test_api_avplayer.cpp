/* Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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

// Ported version of AVPlayer decoder unit tests
//
#include "asset_loader.h"

#include <gtest/gtest.h>
#include <gtest/internal/gtest-internal.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/lcevc_dec.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace lcevc_dec::utility;

// Various helper objects and functions
namespace {

const uintptr_t kInvalidHandle = UINTPTR_MAX;

LCEVC_ReturnCode lcevcGetNaluType(LCEVC_DecoderHandle /*decoder*/, const std::vector<uint8_t>& nalu,
                                  uint64_t /*timestamp*/, int32_t format, int32_t codec,
                                  int32_t* naluType, uint32_t* /*offset*/)
{
    std::vector<uint8_t> enhancementData(nalu.size());
    uint32_t actualSize = 0;
    int32_t r = LCEVC_extractEnhancementFromNAL(
        nalu.data(), static_cast<uint32_t>(nalu.size()), static_cast<LCEVC_NALFormat>(format),
        static_cast<LCEVC_CodecType>(codec), enhancementData.data(),
        static_cast<uint32_t>(enhancementData.size()), &actualSize);
    if (r == 0) {
        return LCEVC_NotFound;
    }

    *naluType = (enhancementData[3] >> 1) & 0x1F;

    return LCEVC_Success;
}

void lcevcPrepareForSeek(LCEVC_DecoderHandle decoder)
{
    EXPECT_EQ(LCEVC_Success, LCEVC_SynchronizeDecoder(decoder, true));
}

void lcevcPrepareForProfileChange(LCEVC_DecoderHandle decoder)
{
    EXPECT_EQ(LCEVC_Success, LCEVC_SynchronizeDecoder(decoder, true));
}

void lcevcFlush(LCEVC_DecoderHandle decoder)
{
    EXPECT_EQ(LCEVC_Success, LCEVC_SynchronizeDecoder(decoder, true));
}

#if 0 // Unused - See LCEVCEnhancerTest, BufferCapacityShouldReturnErrorAtMaxAndAllowFeedAgainAfterSkip
void lcevcDecodeSkip(LCEVC_DecoderHandle decoder, uint64_t timestamp)
{
    EXPECT_EQ(LCEVC_Success, LCEVC_SkipDecoder(decoder, timestamp));
    EXPECT_EQ(LCEVC_Success, LCEVC_SynchronizeDecoder(decoder, false));
}
#endif

std::unique_ptr<PixelBuffer> enhancePixelBufferInner(LCEVC_DecoderHandle decoder,
                                                     const PixelBuffer& base, uint64_t timestamp)
{
    // Make base picture that wraps incoming data
    // NB: Picture allocation does not distinguqish const/non-const - hence const_cast()
    LCEVC_PictureHandle basePicture = allocPictureForPixelBuffer(
        decoder, const_cast<PixelBuffer*>(&base)); // NOLINT(cppcoreguidelines-pro-type-const-cast)

    // Send to decoder
    if (LCEVC_SendDecoderBase(decoder, timestamp, basePicture, UINT32_MAX, nullptr) != LCEVC_Success) {
        return nullptr;
    }

    // Find out output size
    //
    // NB: This has _way_ too many assumptions about the state of the decoder
    uint32_t outputWidth{};
    uint32_t outputHeight{};
    if (LCEVC_PeekDecoder(decoder, timestamp, &outputWidth, &outputHeight) != LCEVC_Success) {
        return nullptr;
    }

    // Send output picture to decoder
    std::unique_ptr<PixelBuffer> output{allocPixelBuffer(decoder, outputWidth, outputHeight)};
    LCEVC_PictureHandle outputPicture = allocPictureForPixelBuffer(decoder, output.get());
    if (LCEVC_SendDecoderPicture(decoder, outputPicture) != LCEVC_Success) {
        return nullptr;
    }

    // Get decoded picture back from decoder
    LCEVC_PictureHandle decodedPicture{kInvalidHandle};
    LCEVC_DecodeInformation decodeInformation{};
    if (LCEVC_ReceiveDecoderPicture(decoder, &decodedPicture, &decodeInformation) != LCEVC_Success) {
        return nullptr;
    }

    EXPECT_EQ(decodedPicture.hdl, outputPicture.hdl);

    // Get base picture back from decoder
    LCEVC_PictureHandle doneBasePicture{kInvalidHandle};
    if (LCEVC_ReceiveDecoderBase(decoder, &doneBasePicture) != LCEVC_Success) {
        return nullptr;
    }
    EXPECT_EQ(doneBasePicture.hdl, basePicture.hdl);

    // Release base picture
    EXPECT_EQ(LCEVC_Success, LCEVC_FreePicture(decoder, doneBasePicture));

    // Release decoded picture (The pixels will still be in the associated PixelBuffer's data vector)
    EXPECT_EQ(LCEVC_Success, LCEVC_FreePicture(decoder, decodedPicture));

    return output;
}

std::unique_ptr<PixelBuffer> enhancePixelBuffer(LCEVC_DecoderHandle decoder, const PixelBuffer& base,
                                                uint64_t timestamp, bool expectFail = false)
{
    std::unique_ptr<PixelBuffer> pixelBuffer = enhancePixelBufferInner(decoder, base, timestamp);
    if (expectFail) {
        EXPECT_EQ(pixelBuffer, nullptr);
    } else {
        EXPECT_NE(pixelBuffer, nullptr);
    }
    return pixelBuffer;
}

// Make a timestamp from seconds
uint64_t genPTS(double seconds) { return static_cast<uint64_t>(seconds * 1000.0); }

} // anonymous namespace

// Test fixture - holds decoder handle
//
struct LCEVCEnhancerTest : public ::testing::Test
{
    LCEVC_DecoderHandle decoder{kInvalidHandle};

    void TearDown() override
    {
        if (decoder.hdl != kInvalidHandle) {
            LCEVC_DestroyDecoder(decoder);
        }
    }

    // Mirror Swift’s enum aliases
    struct Codec
    {
        static constexpr int32_t h264 = LCEVC_CodecType_H264;
    };
    struct Format
    {
        static constexpr int32_t annexB = LCEVC_NALFormat_AnnexB;
        static constexpr int32_t mp4 = LCEVC_NALFormat_LengthPrefix;
    };

    enum class PassthroughMode : int8_t
    {
        Allowed = 0,
        Forced = 1,
        Disabled = -1,
        Scaled = 2
    };

    struct DecoderSettings
    {
        DecoderSettings(){};

        bool highlightResiduals = false;
        bool captureLogs = false;
        PassthroughMode passThrough = PassthroughMode::Allowed;
        uint32_t bufferCapacity = 20;
        bool useCPUMode = true;
    };

    void applySettings(const DecoderSettings& settings) const
    {
        EXPECT_EQ(LCEVC_Success,
                  LCEVC_ConfigureDecoderInt(decoder, "log_level", settings.captureLogs ? 5 : 2));

        EXPECT_EQ(LCEVC_Success, LCEVC_ConfigureDecoderInt(decoder, "threads", 32));

        EXPECT_EQ(LCEVC_Success, LCEVC_ConfigureDecoderBool(decoder, "highlight_residuals",
                                                            settings.highlightResiduals));
        EXPECT_EQ(LCEVC_Success, LCEVC_ConfigureDecoderInt(decoder, "passthrough_mode",
                                                           static_cast<int>(settings.passThrough)));
        EXPECT_EQ(LCEVC_Success,
                  LCEVC_ConfigureDecoderInt(decoder, "max_latency", settings.bufferCapacity));
    }

    void setupDecoder(const DecoderSettings& settings = {})
    {
        EXPECT_EQ(decoder.hdl, kInvalidHandle);

        LCEVC_AccelContextHandle accelContext{};
        EXPECT_EQ(LCEVC_Success, LCEVC_CreateDecoder(&decoder, accelContext));
        applySettings(settings);
        EXPECT_EQ(LCEVC_Success, LCEVC_InitializeDecoder(decoder));
    }
};

// ------------------- Tests -------------------

TEST_F(LCEVCEnhancerTest, NewAssetLoader)
{
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.ts",
                       kTestAssets / "elfuente_640x360_12f_ts.opl");

    EXPECT_EQ(loader.size(), 12);
}

TEST_F(LCEVCEnhancerTest, EnhancerInitializedWithoutAnyExceptionWhenCPUModeEnabled)
{
    EXPECT_NO_THROW({
        DecoderSettings settings;
        settings.useCPUMode = true;

        setupDecoder();
        ASSERT_TRUE(decoder.hdl != kInvalidHandle);
    });
}

TEST_F(LCEVCEnhancerTest, EnhancerInitializedWithoutAnyExceptionWhenGPUModeEnabled)
{
    EXPECT_NO_THROW({
        DecoderSettings settings;
        settings.useCPUMode = false;

        setupDecoder(settings);
        ASSERT_TRUE(decoder.hdl != kInvalidHandle);
    });
}

TEST_F(LCEVCEnhancerTest, FeedNalUnitShouldFeedNALuToDecSuccessfully)
{
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto& nalu = loader.enhancement(000);
    int rc = feedNalUnit(decoder, nalu, genPTS(0.0), Format::mp4, Codec::h264);
    EXPECT_EQ(rc, LCEVC_Success);
}

TEST_F(LCEVCEnhancerTest, FeedNalUnitShouldReturnNotFoundWhenLCEVCMissing)
{
    setupDecoder();
    std::vector<uint8_t> empty;
    int rc = feedNalUnit(decoder, empty, genPTS(0.0), Format::mp4, Codec::h264);
    EXPECT_EQ(rc, LCEVC_NotFound);
}

#if 0 /// Disable for the moment - unstable - DEC-1056
TEST_F(LCEVCEnhancerTest, BufferCapacityShouldReturnErrorAtMaxAndAllowFeedAgainAfterSkip)
{
    DecoderSettings settings;
    settings.bufferCapacity = 32;
    settings.captureLogs = true;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto& nalu = loader.enhancement(000);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * i), Format::mp4, Codec::h264), LCEVC_Success);
    }
    EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * 32), Format::mp4, Codec::h264), LCEVC_Again);
    lcevcDecodeSkip(decoder, 0);
    EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * 33), Format::mp4, Codec::h264), LCEVC_Success);
}
#endif

TEST_F(LCEVCEnhancerTest, GetLCEVCNaluTypeShouldReturnCorrectNALUType)
{
    DecoderSettings settings;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.ts",
                       kTestAssets / "elfuente_640x360_12f_ts.opl");

    uint32_t lcevcOffset = 0;
    int32_t naluType = 0;
    const int32_t idrType = 29;

    const auto& n1 = loader.enhancement(000);
    auto n1PTS = genPTS(0.100);
    lcevcGetNaluType(decoder, n1, n1PTS, Format::annexB, Codec::h264, &naluType, &lcevcOffset);
    EXPECT_EQ(naluType, idrType);

    const auto& n2 = loader.enhancement(001);
    auto n2PTS = genPTS(0.133);
    lcevcGetNaluType(decoder, n2, n2PTS, Format::annexB, Codec::h264, &naluType, &lcevcOffset);
    EXPECT_NE(naluType, idrType);
}

TEST_F(LCEVCEnhancerTest, Decode3Frames)
{
    DecoderSettings settings;
    settings.bufferCapacity = 10;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.ts",
                       kTestAssets / "elfuente_640x360_12f_ts.opl");

    const auto pb1PTS = loader.timestamp(0);
    const auto pb2PTS = loader.timestamp(1);
    const auto pb3PTS = loader.timestamp(2);

    const auto& pb1 = loader.base(0);
    const auto& pb2 = loader.base(1);
    const auto& pb3 = loader.base(2);

    const auto& n1 = loader.enhancement(0);
    const auto& n2 = loader.enhancement(1);
    const auto& n3 = loader.enhancement(2);

    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::annexB, Codec::h264), LCEVC_Success);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::annexB, Codec::h264), LCEVC_Success);
    EXPECT_EQ(feedNalUnit(decoder, n3, pb3PTS, Format::annexB, Codec::h264), LCEVC_Success);

    auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    loader.checkDecoded(0, *enh1);

    auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    loader.checkDecoded(1, *enh2);

    auto enh3 = enhancePixelBuffer(decoder, pb3, pb3PTS);
    loader.checkDecoded(2, *enh3);
}

TEST_F(LCEVCEnhancerTest, UnprocessedLCEVCBlocksShouldReturnCorrectNumberAfterEnhancement)
{
    const int width = 320;
    const int height = 180;
    DecoderSettings settings{};
    settings.bufferCapacity = 2;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.ts",
                       kTestAssets / "elfuente_640x360_12f_ts.opl");

    const auto pb1PTS = genPTS(0.000);
    auto pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.033);
    auto pb2 = loader.base(001);

    auto n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::annexB, Codec::h264), LCEVC_Success);

    auto n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::annexB, Codec::h264), LCEVC_Success);

    auto n3 = loader.enhancement(002);
    EXPECT_EQ(feedNalUnit(decoder, n3, genPTS(0.167), Format::annexB, Codec::h264), LCEVC_Again);

    auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(enh1->width, 2 * width);
    EXPECT_EQ(enh1->height, 2 * height);

    EXPECT_EQ(feedNalUnit(decoder, n3, genPTS(0.200), Format::annexB, Codec::h264), LCEVC_Success);

    // Max buffer was 2 — feeding another should be "try again"
    EXPECT_EQ(feedNalUnit(decoder, n3, genPTS(0.233), Format::annexB, Codec::h264), LCEVC_Again);
    loader.checkDecoded(0, *enh1);

    auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    loader.checkDecoded(1, *enh2);

    EXPECT_EQ(feedNalUnit(decoder, n3, genPTS(0.267), Format::annexB, Codec::h264), LCEVC_Success);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnEnhancedImageForFMP4WhenCPUModeIsActive)
{
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    auto pb1PTS = genPTS(0.000);
    auto pb1 = loader.base(000);

    auto pb2PTS = genPTS(0.020);
    auto pb2 = loader.base(001);

    auto n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    auto n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::mp4, Codec::h264), LCEVC_Success);

    auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    loader.checkDecoded(0, *enh1);

    auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    loader.checkDecoded(1, *enh2);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnEnhancedImageForFMP4WhenGPUModeIsActive)
{
    const int width = 320;
    const int height = 180;
    DecoderSettings settings;
    settings.useCPUMode = false;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    auto pb1PTS = genPTS(0.000);
    auto pb1 = loader.base(000);

    auto pb2PTS = genPTS(0.020);
    auto pb2 = loader.base(001);

    auto n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    auto n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::mp4, Codec::h264), LCEVC_Success);

    auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(enh1->width, 2 * width);
    EXPECT_EQ(enh1->height, 2 * height);
    loader.checkDecoded(0, *enh1);

    auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(enh2->width, 2 * width);
    EXPECT_EQ(enh2->height, 2 * height);
    loader.checkDecoded(1, *enh2);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnBaseImageWhenPassThroughEnabled)
{
    DecoderSettings settings;
    settings.passThrough = PassthroughMode::Forced;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.020);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    const auto& n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::mp4, Codec::h264), LCEVC_Success);

    const auto pass1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(*pass1, pb1);

    const auto pass2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(*pass2, pb2);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnBaseImageWhenWrongFormatIsSet)
{
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.020);
    const auto& pb2 = loader.base(001);

    const int wrong = Format::annexB;
    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, wrong, Codec::h264), LCEVC_NotFound);

    const auto& n2 = loader.enhancement(003);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, wrong, Codec::h264), LCEVC_NotFound);

    const auto pass1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(*pass1, pb1);

    const auto pass2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(*pass2, pb2);

    const auto pb3PTS = genPTS(0.040);
    const auto& n3 = loader.enhancement(002);
    EXPECT_EQ(feedNalUnit(decoder, n3, pb3PTS, Format::mp4, Codec::h264), LCEVC_Success);
}

// (Swift had a commented-out test for duplicate NALU; skipping here.)

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnErrorForFMP4WhenMatchingLCEVCIsNotFound)
{
    const int width = 320;
    const int height = 180;
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.020);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    // Intentionally not feeding the matching LCEVC for pb2

    const auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    loader.checkDecoded(0, *enh1);

    // Expect passthrough for pb2
    const auto out2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(out2->width, width);
    EXPECT_EQ(out2->height, height);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnErrorWhenMatchingLCEVCIsNotFoundAndPassthroughDisabled)
{
    DecoderSettings settings;
    settings.passThrough = PassthroughMode::Disabled;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.020);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    // Not feeding matching LCEVC for pb2 → expect error
    (void)enhancePixelBuffer(decoder, pb1, pb1PTS);
    // For pb2, we expect failure
    (void)enhancePixelBuffer(decoder, pb2, pb2PTS, true);
}

TEST_F(LCEVCEnhancerTest, PrepareForSeekShouldResetUnprocessedLCEVCBlocksCount)
{
    DecoderSettings settings;
    settings.bufferCapacity = 32;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto& nalu = loader.enhancement(000);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, /*genPTS(0.03 * i)*/ i, Format::mp4, Codec::h264),
                  LCEVC_Success);
    }
    EXPECT_EQ(feedNalUnit(decoder, nalu, /*genPTS(0.03 * 32)*/ 32, Format::mp4, Codec::h264), LCEVC_Again);

    lcevcPrepareForSeek(decoder);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, /*genPTS(0.03 * i)*/ i, Format::mp4, Codec::h264),
                  LCEVC_Success);
    }
}

TEST_F(LCEVCEnhancerTest, PrepareForProfileChangeShouldResetUnprocessedLCEVCBlocksCount)
{
    DecoderSettings settings;
    settings.bufferCapacity = 32;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto& nalu = loader.enhancement(000);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * i), Format::mp4, Codec::h264), LCEVC_Success);
    }
    EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * 32), Format::mp4, Codec::h264), LCEVC_Again);

    lcevcPrepareForProfileChange(decoder);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * i), Format::mp4, Codec::h264), LCEVC_Success);
    }
}

TEST_F(LCEVCEnhancerTest, FlushShouldResetUnprocessedLCEVCBlocksCount)
{
    DecoderSettings settings;
    settings.bufferCapacity = 32;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto& nalu = loader.enhancement(000);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * i), Format::mp4, Codec::h264), LCEVC_Success);
    }
    EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * 32), Format::mp4, Codec::h264), LCEVC_Again);

    lcevcFlush(decoder);

    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(feedNalUnit(decoder, nalu, genPTS(0.03 * i), Format::mp4, Codec::h264), LCEVC_Success);
    }
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnEnhancedImageWithHighlightedResidualsWhenEnabled)
{
    DecoderSettings settings;
    settings.highlightResiduals = true;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4_highlighted.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.020);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    const auto& n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::mp4, Codec::h264), LCEVC_Success);

    const auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    loader.checkDecoded(0, *enh1);

    const auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    loader.checkDecoded(1, *enh2);
}

TEST_F(LCEVCEnhancerTest, PropagateAttachmentsFromSourceShouldSetAspectRatioCorrectly)
{
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_640x360_12f.mp4",
                       kTestAssets / "elfuente_640x360_12f_mp4.opl");

    const auto pb1PTS = genPTS(0.000);
    auto pb1 = loader.base(000);
    pb1.sampleAspectRatioNum = 1;
    pb1.sampleAspectRatioDen = 1; // set aspect ratio (1:1) like the Swift test

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::mp4, Codec::h264), LCEVC_Success);

    const auto enh = enhancePixelBuffer(decoder, pb1, pb1PTS);
    // Verify attachments propagated
    EXPECT_EQ(enh->sampleAspectRatioNum, 1);
    EXPECT_EQ(enh->sampleAspectRatioDen, 1);
}

// ------------------- 1D Tests -------------------

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnEnhancedImageFor1DTSStream)
{
    const int width = 320;
    const int height = 360;
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_1D_640x360_12f.ts",
                       kTestAssets / "elfuente_1D_640x360_12f_ts.opl");

    const auto pb1PTS = genPTS(0.000);
    auto pb1 = loader.base(000);
    pb1.sampleAspectRatioNum = 2;
    pb1.sampleAspectRatioDen = 1;

    auto pb2PTS = genPTS(0.033);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::annexB, Codec::h264), LCEVC_Success);

    const auto& n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::annexB, Codec::h264), LCEVC_Success);

    const auto enh1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(enh1->width, 2 * width);
    EXPECT_EQ(enh1->height, height);

    const auto enh2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(enh2->width, 2 * width);
    EXPECT_EQ(enh2->height, height);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnBaseImageWhenPassThroughEnabledAndStreamIs1D)
{
    DecoderSettings settings;
    settings.passThrough = PassthroughMode::Forced;
    setupDecoder(settings);
    AssetLoader loader(kTestAssets / "elfuente_1D_640x360_12f.ts",
                       kTestAssets / "elfuente_1D_640x360_12f_ts.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.033);
    const auto& pb2 = loader.base(001);

    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, Format::annexB, Codec::h264), LCEVC_Success);

    const auto& n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, Format::annexB, Codec::h264), LCEVC_Success);

    const auto pass1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(*pass1, pb1);

    const auto pass2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(*pass2, pb2);
}

TEST_F(LCEVCEnhancerTest, DecodeShouldReturnBaseImageWhenWrongFormatIsSetAndStreamIs1D)
{
    setupDecoder();
    AssetLoader loader(kTestAssets / "elfuente_1D_640x360_12f.ts",
                       kTestAssets / "elfuente_1D_640x360_12f_ts.opl");

    const auto pb1PTS = genPTS(0.000);
    const auto& pb1 = loader.base(000);

    const auto pb2PTS = genPTS(0.033);
    const auto& pb2 = loader.base(001);

    int wrong = Format::mp4;
    const auto& n1 = loader.enhancement(000);
    EXPECT_EQ(feedNalUnit(decoder, n1, pb1PTS, wrong, Codec::h264), LCEVC_NotFound);

    const auto& n2 = loader.enhancement(001);
    EXPECT_EQ(feedNalUnit(decoder, n2, pb2PTS, wrong, Codec::h264), LCEVC_NotFound);

    const auto pass1 = enhancePixelBuffer(decoder, pb1, pb1PTS);
    EXPECT_EQ(*pass1, pb1);

    const auto pass2 = enhancePixelBuffer(decoder, pb2, pb2PTS);
    EXPECT_EQ(*pass2, pb2);

    const auto pb3PTS = genPTS(0.040);
    const auto& n3 = loader.enhancement(002);
    EXPECT_EQ(feedNalUnit(decoder, n3, pb3PTS, Format::annexB, Codec::h264), LCEVC_Success);
}
