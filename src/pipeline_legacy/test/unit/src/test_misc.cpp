/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

// This tests decoder_config.h, picture_lock.h and buffer_manager.h

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).

#include "utils.h"

#include <buffer_manager.h>
#include <decoder_config.h>
#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>
#include <picture.h>
#include <picture_lock.h>

#include <thread>

using namespace lcevc_dec::decoder;

class ConfigFixture : public testing::Test
{
public:
    void SetUp() override
    {
        // Set all config items to a non-default valid value
        config.set("highlight_residuals", true);
        config.set("log_stdout", true);
        config.set("use_loq0", false);
        config.set("use_loq1", false);
        config.set("s_filter_strength", -2.0f);
        config.set("dither_strength", 1);
        config.set("dpi_pipeline_mode", 1);
        config.set("dpi_threads", 1);
        config.set("log_level", static_cast<int32_t>(LdcLogLevel::LdcLogLevelVerbose));
        config.set("results_queue_cap", 1);
        config.set("loq_unprocessed_cap", 1);
        config.set("passthrough_mode", static_cast<int32_t>(PassthroughPolicy::Disable));
        config.set("predicted_average_method", static_cast<int32_t>(PredictedAverageMethod::None));
        config.set("pss_surface_fp_setting", 1);
        config.set("events", events);
    }

    DecoderConfig config;
    const std::vector<int32_t> events = {LCEVC_Log, LCEVC_Exit};
};

TEST_F(ConfigFixture, NonDefaultValid)
{
    EXPECT_TRUE(config.validate());
    EXPECT_TRUE(config.getEvents() == events);
}

TEST_F(ConfigFixture, LoqUnprocessedCapInvalid)
{
    config.set("loq_unprocessed_cap", -2);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigFixture, ResultsQueueCapInvalid)
{
    config.set("results_queue_cap", -2);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigFixture, UnderPAMethodInvalid)
{
    config.set("predicted_average_method", -1);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigFixture, OverPAMethodInvalid)
{
    config.set("predicted_average_method", static_cast<int32_t>(PredictedAverageMethod::COUNT) + 1);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigFixture, EventsInvalid)
{
    std::vector<int32_t> invalidEvents = {-1, LCEVC_EventCount + 1};
    config.set("events", invalidEvents);
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigFixture, SetParamInvalid)
{
    EXPECT_FALSE(config.set("garbage_parameter", 0));
    EXPECT_FALSE(config.set("highlight_residuals", 0)); // Incorrect type
}

TEST(PictureLockTest, PictureLockValid)
{
    const uint32_t res[2] = {1920, 1080};
    std::unique_ptr<uint8_t[]> dataBuffer = std::make_unique<uint8_t[]>(res[0] * res[1] * 3 / 2);
    LdpPictureBufferDesc inputBufferDesc = {dataBuffer.get(), res[0] * res[1] * 3 / 2, nullptr,
                                            LdpAccessModify};
    LdpPicturePlaneDesc planeDescArr[kI420NumPlanes] = {
        {dataBuffer.get(), res[0]},
        {dataBuffer.get() + res[0] * res[1], res[0] / 2},
        {dataBuffer.get() + res[0] * res[1] * 5 / 4, res[0] / 2}};
    LdpPictureDesc inputPictureDesc;
    LCEVC_DefaultPictureDesc((LCEVC_PictureDesc*)&inputPictureDesc, LCEVC_I420_8, res[0], res[1]);

    PictureExternal picture;
    picture.setDescExternal(inputPictureDesc, planeDescArr, &inputBufferDesc);

    LdpAccess access = {};
    PictureLock pictureLock(&picture, access);
    LdpPictureBufferDesc outputBufferDesc = {};
    EXPECT_TRUE(pictureLock.getBufferDesc(&outputBufferDesc));
    EXPECT_EQ(inputBufferDesc.data, outputBufferDesc.data);
    EXPECT_EQ(inputBufferDesc.byteSize, outputBufferDesc.byteSize);
    EXPECT_EQ(inputBufferDesc.accelBuffer, outputBufferDesc.accelBuffer);
    EXPECT_EQ(inputBufferDesc.access, outputBufferDesc.access);

    for (uint32_t planeIdx = 0; planeIdx < kI420NumPlanes; planeIdx++) {
        LdpPicturePlaneDesc planeDesc = {};
        EXPECT_TRUE(pictureLock.getPlaneDesc(planeIdx, &planeDesc));
        EXPECT_EQ(planeDesc.firstSample, planeDescArr[planeIdx].firstSample);
        EXPECT_EQ(planeDesc.rowByteStride, planeDescArr[planeIdx].rowByteStride);
    }
}

TEST(BufferManagerTest, BufferManagerValid)
{
    const size_t kBufferSize = 1920 * 1080;
    BufferManager buffer;
    PictureBuffer* pictureBuffer = buffer.getBuffer(kBufferSize);

    EXPECT_EQ(kBufferSize, pictureBuffer->size());
    EXPECT_TRUE(buffer.releaseBuffer(pictureBuffer));
    EXPECT_FALSE(buffer.releaseBuffer(pictureBuffer));
}
