/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This tests clock.h, decoder_config.h, picture_lock.h and buffer_manager.h

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).
#define VNDisablePublicAPI

#include <LCEVC/lcevc_dec.h>
#include <buffer_manager.h>
#include <clock.h>
#include <decoder_config.h>
#include <gtest/gtest.h>
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
        config.set("log_level", LogType_Verbose);
        config.set("results_queue_cap", 1);
        config.set("loq_unprocessed_cap", 1);
        config.set("passthrough_mode", static_cast<int32_t>(lcevc_dec::utility::DILPassthroughPolicy::Disable));
        config.set("predicted_average_method",
                   static_cast<int32_t>(lcevc_dec::utility::PredictedAverageMethod::None));
        config.set("pss_surface_fp_setting", 1);
        config.set("events", events);
    }

    DecoderConfig config;
    const std::vector<int32_t> events = {LCEVC_Log, LCEVC_Exit};
};

TEST(ClockTest, IncrementValid)
{
    // Clock is accurate to 1% over 10ms when started and stopped at the 'same time' as chrono
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    Clock clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::high_resolution_clock::now() - start)
                            .count();
    uint64_t clockDuration = clock.getTimeSinceStart();
    EXPECT_GE(duration + 100, clockDuration);
    EXPECT_LE(duration - 100, clockDuration);
}

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
    config.set("predicted_average_method",
               static_cast<int32_t>(lcevc_dec::utility::PredictedAverageMethod::COUNT) + 1);
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
    const size_t kNumPlanes = 3;
    const uint32_t res[2] = {1920, 1080};
    std::unique_ptr<uint8_t[]> dataBuffers[kNumPlanes] = {
        std::make_unique<uint8_t[]>(res[0] * res[1]), std::make_unique<uint8_t[]>(res[0] * res[1] / 4),
        std::make_unique<uint8_t[]>(res[0] * res[1] / 4)};
    LCEVC_PictureBufferDesc inputBufferDesc[kNumPlanes] = {
        {dataBuffers[0].get(), res[0] * res[1], {kInvalidHandle}, LCEVC_Access_Modify},
        {dataBuffers[1].get(), res[0] * res[1] / 4, {kInvalidHandle}, LCEVC_Access_Modify},
        {dataBuffers[2].get(), res[0] * res[1] / 4, {kInvalidHandle}, LCEVC_Access_Modify}};

    PictureExternal picture;
    picture.bindMemoryBuffers(kNumPlanes, inputBufferDesc);
    LCEVC_PictureDesc inputPictureDesc;
    LCEVC_DefaultPictureDesc(&inputPictureDesc, LCEVC_I420_8, res[0], res[1]);
    picture.setDesc(inputPictureDesc);

    for (uint8_t plane = 0; plane < kNumPlanes; plane++) {
        const uint32_t kWidth = (plane == 0) ? res[0] : res[0] / 2;

        Access access = {};
        PictureLock pictureLock(picture, access);
        PictureBufferDesc outputBufferDesc = pictureLock.getBufferDesc(plane);
        EXPECT_EQ(inputBufferDesc[plane].byteSize, outputBufferDesc.byteSize);
        EXPECT_EQ(inputBufferDesc[plane].access, outputBufferDesc.access);
        EXPECT_EQ(inputBufferDesc[plane].data, outputBufferDesc.data);

        PicturePlaneDesc outputPlaneDesc = pictureLock.getPlaneDesc(plane);
        EXPECT_EQ(outputPlaneDesc.rowByteStride, kWidth);
        EXPECT_EQ(outputPlaneDesc.sampleByteSize, 1);
        EXPECT_EQ(outputPlaneDesc.sampleByteStride, 1);
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
