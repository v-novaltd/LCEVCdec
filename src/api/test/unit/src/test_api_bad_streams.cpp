/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This tests api/include/LCEVC/lcevc_dec.h against corrupt streams.
//
// Note: These tests focus on MACROSCOPIC bitstream damage. They DO NOT test that INDIVIDUAL parts
// of the stream are correctly rejected when invalid (e.g. invalid payload type, wrong country
// code, incompatible dimensions). Those sorts of tests are best handled by unit tests of the Core
// deserialiser itself.

// Define this to use the interface of the API (which normally would be in a dll).
#define VNDisablePublicAPI

#include "data.h"
#include "utils.h"

#include <LCEVC/lcevc_dec.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <queue>
#include <random>
#include <vector>

static const int64_t kEndPTS = 100;

class EnhancementGetter
{
public:
    EnhancementWithData getEnhancement(int64_t pts) const
    {
        return ::getEnhancement(pts, m_srcEnhancements);
    }

    virtual ~EnhancementGetter() = default;
    virtual bool frameShouldBeEnhanced(int64_t) const = 0;

protected:
    std::vector<uint8_t> m_srcEnhancements[3] = {};
};

class ReverseEnhancementGetter : public EnhancementGetter
{
public:
    ReverseEnhancementGetter(const std::vector<uint8_t> srcEnhancements[3])
    {
        for (size_t i = 0; i < 3; i++) {
            for (auto revIt = srcEnhancements[i].rbegin(); revIt != srcEnhancements[i].rend(); revIt++) {
                m_srcEnhancements[i].push_back(*revIt);
            }
        }
    }
    bool frameShouldBeEnhanced(int64_t) const final { return false; }
};

class DroppedBytesEnhancementGetter : public EnhancementGetter
{
public:
    DroppedBytesEnhancementGetter(const std::vector<uint8_t> srcEnhancements[3])
    {
        // Use the same seed every time, so we don't get un-reproducible failures (in other words,
        // the dropped bytes are random like "surprising", but not like "different each time").
        std::mt19937 generator(2023);
        for (size_t i = 0; i < 3; i++) {
            for (uint8_t byte : srcEnhancements[i]) {
                if (generator() % 6 == 0) {
                    continue;
                }
                m_srcEnhancements[i].push_back(byte);
            }
        }
    }
    bool frameShouldBeEnhanced(int64_t) const final { return false; }
};

class NormalEnhancementGetter : public EnhancementGetter
{
public:
    NormalEnhancementGetter(const std::vector<uint8_t> srcEnhancements[3])
    {
        m_isValid = (srcEnhancements == kValidEnhancements);

        for (size_t i = 0; i < 3; i++) {
            m_srcEnhancements[i] = srcEnhancements[i];
        }
    }
    bool frameShouldBeEnhanced(int64_t) const override { return m_isValid; }

protected:
    bool m_isValid = false;
};

class DisorderedEnhancementGetter : public NormalEnhancementGetter
{
public:
    DisorderedEnhancementGetter(const std::vector<uint8_t> srcEnhancements[3])
        : NormalEnhancementGetter(srcEnhancements)
    {
        // Use the same seed every time, so we don't get un-reproducible failures (in other words,
        // this is a disordered list, but not a truly randomised one).
        std::mt19937 generator(2023);
        for (std::vector<uint8_t>& vec : m_srcEnhancements) {
            std::shuffle(vec.begin(), vec.end(), generator);
        }
    }
    bool frameShouldBeEnhanced(int64_t) const final { return false; }
};

class BreakNthFrameGetter : public NormalEnhancementGetter
{
public:
    BreakNthFrameGetter(const std::vector<uint8_t> srcEnhancements[3], const std::set<uint8_t>& framesToBreak)
        : NormalEnhancementGetter(srcEnhancements)
    {
        for (uint8_t idx : framesToBreak) {
            assert(idx < 3);

            // Break it by providing an invalid start code.
            m_srcEnhancements[idx][0] = 255;
            m_srcEnhancements[idx][1] = 255;
            m_srcEnhancements[idx][2] = 255;
        }

        if (framesToBreak.count(0) != 0) {
            // If the first frame was broken, then they'll ALL be broken, because that contains
            // configuration information.
            m_expectedBrokenFrames = {0, 1, 2};
        } else {
            // Otherwise, only the explicitly broken ones will be broken.
            m_expectedBrokenFrames = framesToBreak;
        }
    }
    bool frameShouldBeEnhanced(int64_t pts) const final
    {
        const auto srcFrame = static_cast<uint8_t>(pts % 3);
        return m_expectedBrokenFrames.count(srcFrame) == 0;
    }

private:
    std::set<uint8_t> m_expectedBrokenFrames = {};
};

// Can't use the EnhancementGetter directly (gtest will try to create instances of it, but it's
// pure virtual). So, use a shared_ptr: it's cleaner than a raw pointer, and the gtest library
// isn't compatible with references or unique_ptrs as test params.
class APIBadStreamsFixture : public testing::TestWithParam<std::shared_ptr<EnhancementGetter>>
{
public:
    void SetUp() override
    {
        // Create with all default configs, no events.
        ASSERT_EQ(LCEVC_CreateDecoder(&m_decHdl, {}), LCEVC_Success);
        ASSERT_EQ(LCEVC_InitializeDecoder(m_decHdl), LCEVC_Success);

        LCEVC_DefaultPictureDesc(&m_inputDesc, LCEVC_I420_8, 960, 540);
        LCEVC_DefaultPictureDesc(&m_outputDesc, LCEVC_I420_8, 1920, 1080);
    }

    LCEVC_DecoderHandle getHdl() const { return m_decHdl; }
    const LCEVC_PictureDesc& getInputDesc() const { return m_inputDesc; }
    const LCEVC_PictureDesc& getOutputDesc() const { return m_outputDesc; }

private:
    LCEVC_DecoderHandle m_decHdl = {};
    LCEVC_PictureDesc m_inputDesc = {};
    LCEVC_PictureDesc m_outputDesc = {};
};

// Note that we have 1 fully valid test.
INSTANTIATE_TEST_SUITE_P(
    VariousEnhancements, APIBadStreamsFixture,
    testing::Values(std::make_shared<NormalEnhancementGetter>(kValidEnhancements),
                    std::make_shared<NormalEnhancementGetter>(kEnhancementsBadStartCodes),
                    std::make_shared<NormalEnhancementGetter>(kEnhancementsMessedUp),
                    std::make_shared<ReverseEnhancementGetter>(kValidEnhancements),
                    std::make_shared<DisorderedEnhancementGetter>(kValidEnhancements),
                    std::make_shared<DroppedBytesEnhancementGetter>(kValidEnhancements),
                    std::make_shared<BreakNthFrameGetter>(kValidEnhancements, std::set<uint8_t>{0}),
                    std::make_shared<BreakNthFrameGetter>(kValidEnhancements, std::set<uint8_t>{1}),
                    std::make_shared<BreakNthFrameGetter>(kValidEnhancements, std::set<uint8_t>{2}),
                    std::make_shared<BreakNthFrameGetter>(kValidEnhancements, std::set<uint8_t>{1, 2})));

TEST_P(APIBadStreamsFixture, Test)
{
    const EnhancementGetter& getter = *GetParam();

    std::queue<LCEVC_PictureHandle> outputs = {};
    std::queue<int64_t> baseTimehandles = {};
    uint32_t numReceived = 0;

    // Send data for each timestamp and try to receive outputs as you go. At the end, we'll flush.
    for (int64_t pts = 0; pts < kEndPTS; pts++) {
        LCEVC_PictureHandle baseHdl = {};
        ASSERT_EQ(LCEVC_AllocPicture(getHdl(), &getInputDesc(), &baseHdl), LCEVC_Success);
        ASSERT_EQ(LCEVC_SendDecoderBase(getHdl(), pts, false, baseHdl, UINT32_MAX, nullptr), LCEVC_Success);
        baseTimehandles.push(pts);

        // Note that insertion should be fine. It's just that the output should be un-enhanced
        const EnhancementWithData enhancement = getter.getEnhancement(pts);
        ASSERT_EQ(LCEVC_SendDecoderEnhancementData(getHdl(), pts, false, enhancement.first,
                                                   enhancement.second),
                  LCEVC_Success);

        LCEVC_PictureHandle outputHdl = {};
        ASSERT_EQ(LCEVC_AllocPicture(getHdl(), &getOutputDesc(), &outputHdl), LCEVC_Success);
        ASSERT_EQ(LCEVC_SendDecoderPicture(getHdl(), outputHdl), LCEVC_Success);
        outputs.push(outputHdl);

        LCEVC_DecodeInformation info = {};
        LCEVC_ReturnCode res = LCEVC_ReceiveDecoderPicture(getHdl(), &outputs.front(), &info);
        if (res != LCEVC_Again) {
            // Check for normal success:
            EXPECT_EQ(info.skipped, false);
            EXPECT_EQ(info.timestamp, baseTimehandles.front());
            ASSERT_FALSE(baseTimehandles.empty());
            baseTimehandles.pop();

            // Check that bad enhancement was handled correctly: not enhanced, and size equals base
            LCEVC_PictureDesc returnedDesc = {};
            LCEVC_GetPictureDesc(getHdl(), outputs.front(), &returnedDesc);
            EXPECT_EQ(info.enhanced, getter.frameShouldBeEnhanced(info.timestamp));
            EXPECT_EQ(info.hasEnhancement, getter.frameShouldBeEnhanced(info.timestamp));
            if (getter.frameShouldBeEnhanced(info.timestamp)) {
                EXPECT_EQ(returnedDesc.width, getOutputDesc().width);
                EXPECT_EQ(returnedDesc.height, getOutputDesc().height);
            } else {
                EXPECT_EQ(returnedDesc.width, getInputDesc().width);
                EXPECT_EQ(returnedDesc.height, getInputDesc().height);
            }

            ASSERT_FALSE(outputs.empty());
            outputs.pop();
            numReceived++;
        }
    }

    // Now just loop until all done
    while (!outputs.empty()) {
        LCEVC_DecodeInformation info = {};
        LCEVC_ReturnCode res = LCEVC_ReceiveDecoderPicture(getHdl(), &outputs.front(), &info);
        if (res != LCEVC_Again) {
            // Check for normal success:
            EXPECT_EQ(info.skipped, false);
            EXPECT_EQ(info.timestamp, baseTimehandles.front());
            ASSERT_FALSE(baseTimehandles.empty());
            baseTimehandles.pop();

            // Check that bad enhancement was handled correctly: not enhanced, and size equals base
            LCEVC_PictureDesc descThatWeActuallyGot = {};
            LCEVC_GetPictureDesc(getHdl(), outputs.front(), &descThatWeActuallyGot);
            EXPECT_EQ(info.enhanced, getter.frameShouldBeEnhanced(info.timestamp));
            EXPECT_EQ(info.hasEnhancement, getter.frameShouldBeEnhanced(info.timestamp));
            if (getter.frameShouldBeEnhanced(info.timestamp)) {
                EXPECT_EQ(descThatWeActuallyGot.width, getOutputDesc().width);
                EXPECT_EQ(descThatWeActuallyGot.height, getOutputDesc().height);
            } else {
                EXPECT_EQ(descThatWeActuallyGot.width, getInputDesc().width);
                EXPECT_EQ(descThatWeActuallyGot.height, getInputDesc().height);
            }

            outputs.pop();
            numReceived++;
        }
    }

    EXPECT_EQ(numReceived, kEndPTS);
}
