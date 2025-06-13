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

// This tests api/src/decoder.h

#include "test_decoder.h"

#include "data.h"
#include "LCEVC/build_config.h"
#include "utils.h"

#include <decoder.h>
#include <gtest/gtest.h>
#include <handle.h>
#include <interface.h>
#include <LCEVC/lcevc_dec.h>
#include <timestamps.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>

using namespace lcevc_dec::decoder;

// - Helper types ---------------------------------------------------------------------------------

struct LCEVC_Decoder
{
    std::mutex mutex;
    Decoder* decoder = nullptr;
};

enum class EnhancementOption
{
    Valid,
    Empty,
    None
};

// - Fixtures -------------------------------------------------------------------------------------

// DecoderFixtureUninitialised (and helpers)

struct CallbackData
{
    CallbackData(Handle<Decoder> decHandleIn = kInvalidHandle,
                 Handle<Picture> picHandleIn = kInvalidHandle) noexcept
        : decHandle(decHandleIn)
        , picHandle(picHandleIn)
    {}
    Handle<Decoder> decHandle = kInvalidHandle;
    Handle<Picture> picHandle = kInvalidHandle;

    bool operator==(const CallbackData& rhs) const
    {
        return (decHandle == rhs.decHandle) && (picHandle == rhs.picHandle);
    }
    bool operator!=(const CallbackData& rhs) const { return !(*this == rhs); }
};

class DecoderFixtureUninitialised : public testing::Test
{
public:
    DecoderFixtureUninitialised() noexcept
        : m_decoder(m_pretendAccelContextHdl, m_pretendDecoderHdl)
    {
        m_decoder.setConfig("loq_unprocessed_cap", kUnprocessedCap);
        m_decoder.setConfig("results_queue_cap", kResultsQueueCap);

        // Most of our tests include intentional "check that failure fails" cases. These are
        // super super verbose if you don't disable logging.
        m_decoder.setConfig("log_level", 0);
    }

    static void callback(Handle<Decoder> decHandle, int32_t event, Handle<Picture> picHandle,
                         const LCEVC_DecodeInformation* lcevcDecInfo, const uint8_t* data,
                         uint32_t dataSize, void* userData)
    {
        // Currently not testing the LCEVC_Log event, so these two params are unused. Not testing
        // that lcevcDecInfo matches, because we already do that outside the callback.
        VN_UNUSED(data);
        VN_UNUSED(dataSize);
        VN_UNUSED(lcevcDecInfo);

        auto* ptrToCaller = static_cast<DecoderFixtureUninitialised*>(userData);
        if (!ptrToCaller->m_expectedCallbackResults[event].empty()) {
            const CallbackData expectedResult = ptrToCaller->m_expectedCallbackResults[event].front();
            EXPECT_EQ(expectedResult, CallbackData(decHandle, picHandle))
                << "Callback data (event " << event << ", count "
                << ptrToCaller->m_callbackCounts[event] << ") was not as expected. Expected decoder "
                << expectedResult.decHandle.handle << " and picture "
                << expectedResult.picHandle.handle << ", but instead received decoder "
                << decHandle.handle << " and picture " << picHandle.handle;
            ptrToCaller->m_expectedCallbackResults[event].pop();
        }
        ptrToCaller->m_callbackCounts[event]++;
    }

protected:
    void TearDown() override { m_decoder.release(); }

    // Just use 'this' as an arbitrary value which differs in each test.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LCEVC_DecoderHandle m_pretendDecoderHdl{reinterpret_cast<uintptr_t>(this)};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LCEVC_AccelContextHandle m_pretendAccelContextHdl{reinterpret_cast<uintptr_t>(this + 1)};

    Decoder m_decoder;

    // This is an array of atomic lists (one for each event type), containing the data that was
    // fed to the callback. We can then check, on the other end of the wait, if they match.
    EventCountArr m_callbackCounts = {};

    // This is an array of queues of callback expectations. This allows us to check that the params
    // of the callback are correct. Note that we may send multiple base images before any are done,
    // so we have to queue them up and check that they come out in order.
    std::array<std::queue<CallbackData>, LCEVC_EventCount> m_expectedCallbackResults = {};
};

// DecoderFixture
class DecoderFixture : public DecoderFixtureUninitialised
{
protected:
    void SetUp() override
    {
        m_decoder.setEventCallback(DecoderFixtureUninitialised::callback, this);
        m_decoder.setConfig("events", kAllEvents);
        m_decoder.setConfig("core_threads", 1);
        m_decoder.initialize();
    }
};

// DecoderFixtureWithData (and helpers). Possible improvement: parameterize formats & resolutions

struct SmartPictureBufferDesc
{
    // Shared ptr is a bit silly here but, it allows us to do things like add entries to the end of
    // a queue and THEN pop them from the from (rather than delicately moving the unique ptr).
    std::shared_ptr<uint8_t> data = nullptr;
    uint32_t byteSize = 0;
    Handle<AccelBuffer> accelBuffer = kInvalidHandle;
    int32_t access = 0;
};

struct BaseWithData
{
    BaseWithData(Handle<Picture> handleIn, SmartPictureBufferDesc&& bufferMovedIn, uint64_t idIn)
        : handle(handleIn)
        , buffer(bufferMovedIn)
        /* NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)*/
        , id(reinterpret_cast<void*>(idIn))
    {}
    Handle<Picture> handle = kInvalidHandle;
    SmartPictureBufferDesc buffer = {};
    void* id = nullptr;
};

using OutputWithData = std::pair<LCEVC_PictureHandle, SmartPictureBufferDesc>;

class DecoderFixtureWithData : public DecoderFixture
{
    using BaseClass = DecoderFixture;

public:
    DecoderFixtureWithData()
    {
        LCEVC_DefaultPictureDesc(&m_inputDesc, LCEVC_I420_8, 960, 540);
        LCEVC_DefaultPictureDesc(&m_outputDesc, LCEVC_I420_8, 1920, 1080);
    }

protected:
    void TearDown() override
    {
        BaseClass::TearDown();
        while (!m_bases.empty()) {
            m_decoder.releasePicture(m_bases.front().handle);
            m_bases.pop_front();
        }
        // ExternalPicture memory and enhancements are released through the magic of smart ptrs.
    }

    void allocBaseManaged(uint64_t baseId)
    {
        LCEVC_PictureHandle managedPicture{kInvalidHandle};
        m_decoder.allocPictureManaged(m_inputDesc, managedPicture);
        m_bases.emplace_back(Handle<Picture>(managedPicture.hdl), SmartPictureBufferDesc{}, baseId);
    }

    void allocOutputManaged()
    {
        LCEVC_PictureHandle managedPicture{kInvalidHandle};
        m_decoder.allocPictureManaged(m_outputDesc, managedPicture);
        m_outputs.emplace_back(managedPicture, SmartPictureBufferDesc{});
    }

    virtual bool allocFeedBase(int64_t pts, uint64_t baseId = 0)
    {
        allocBaseManaged(baseId);

        if (m_decoder.feedBase(pts, false, m_bases.back().handle, UINT32_MAX, m_bases.back().id) ==
            LCEVC_Success) {
            return true;
        }

        m_decoder.releasePicture(m_bases.back().handle);
        m_bases.pop_back();
        return false;
    }

    bool allocFeedOutput()
    {
        allocOutputManaged();
        if (m_decoder.feedOutputPicture(m_outputs.back().first.hdl) == LCEVC_Success) {
            return true;
        }

        m_decoder.releasePicture(m_outputs.back().first.hdl);
        m_outputs.pop_back();
        return false;
    }

    bool allocFeedEnhancement(int64_t pts)
    {
        const auto [enhancementData, enhancementSize] = getEnhancement(pts);
        return (m_decoder.feedEnhancementData(pts, false, enhancementData, enhancementSize) == LCEVC_Success);
    }

    // No memory allocated, just generates sized-pointers to existing data.
    static EnhancementWithData getEnhancement(int64_t pts)
    {
        const size_t idx = pts % (sizeof(kEnhancementSizes) / sizeof(kEnhancementSizes[0]));
        const auto size = static_cast<uint32_t>(kEnhancementSizes[idx]);
        return {kValidEnhancements[idx].data(), size};
    }

    // Sends an output, enhancement, and base, and sets expected callback results for "canReceive"
    // (i.e. can receive decoded output) and "basePictureDone".
    void sendOneOfEach(int64_t pts, LCEVC_ReturnCode& outputResult,
                       LCEVC_ReturnCode& enhancedResult, LCEVC_ReturnCode& baseResult,
                       EnhancementOption enhancementOption = EnhancementOption::Valid)
    {
        m_expectedCallbackResults[LCEVC_CanReceive].emplace(m_pretendDecoderHdl.hdl);

        if (enhancementOption == EnhancementOption::Valid) {
            const auto [enhancementData, enhancementSize] = getEnhancement(pts);
            enhancedResult = m_decoder.feedEnhancementData(pts, false, enhancementData, enhancementSize);
        } else if (enhancementOption == EnhancementOption::Empty) {
            enhancedResult = m_decoder.feedEnhancementData(pts, false, nullptr, 0);
        }

        allocBaseManaged(pts);
        m_expectedCallbackResults[LCEVC_BasePictureDone].emplace(m_pretendDecoderHdl.hdl,
                                                                 m_bases.back().handle.handle);
        baseResult = m_decoder.feedBase(pts, false, m_bases.back().handle, UINT32_MAX, nullptr);

        allocOutputManaged();
        outputResult = m_decoder.feedOutputPicture(m_outputs.back().first.hdl);
    }

    void sendOneOfEach(int64_t pts, EnhancementOption enhancementOption = EnhancementOption::Valid)
    {
        LCEVC_ReturnCode outputResult = LCEVC_Error;
        LCEVC_ReturnCode enhancedResult = LCEVC_Error;
        LCEVC_ReturnCode baseResult = LCEVC_Error;
        sendOneOfEach(pts, outputResult, enhancedResult, baseResult, enhancementOption);
        ASSERT_TRUE(outputResult == LCEVC_Success &&
                    ((enhancedResult == LCEVC_Success) || enhancementOption == EnhancementOption::None) &&
                    baseResult == LCEVC_Success);
    }

    void receiveOneOfEach(LCEVC_ReturnCode expectedCode, bool expectEnhanced)
    {
        LCEVC_PictureHandle outputHandle = {};
        LCEVC_DecodeInformation decodeInfo = {};
        EXPECT_EQ(m_decoder.produceOutputPicture(outputHandle, decodeInfo), expectedCode);
        EXPECT_EQ(decodeInfo.enhanced, expectEnhanced);
        const Picture* output = m_decoder.getPicture(outputHandle.hdl);
        const Picture* base = m_decoder.getPicture(m_bases.front().handle);
        m_bases.pop_front();

        // Double-check the "enhanced" flag using image dimensions (our enhancement data is 2D)
        LCEVC_PictureDesc outputDesc;
        output->getDesc(outputDesc);
        LCEVC_PictureDesc baseDesc;
        base->getDesc(baseDesc);
        if (expectEnhanced) {
            EXPECT_EQ(outputDesc.width, baseDesc.width * 2);
            EXPECT_EQ(outputDesc.height, baseDesc.height * 2);
        } else if (expectedCode == LCEVC_Success) {
            // Note: base should match output IF the decode was a "successful passthrough". For a
            // non-successful passthrough (i.e. passthrough is not allowed), the output desc can be
            // anything.
            EXPECT_EQ(memcmp(&outputDesc, &baseDesc, sizeof(outputDesc)), 0);
        }

        ASSERT_NE(output, nullptr);
        ASSERT_NE(base, nullptr);
    }

    std::deque<BaseWithData> m_bases;
    std::deque<OutputWithData> m_outputs;

    LCEVC_PictureDesc m_inputDesc = {};
    LCEVC_PictureDesc m_outputDesc = {};
};

// DecoderFixturePreFilled
class DecoderFixturePreFilled : public DecoderFixtureWithData
{
    using BaseClass = DecoderFixtureWithData;

public:

protected:
    bool allocFeedBase(int64_t pts, uint64_t baseId = 0) override
    {
        return BaseClass::allocFeedBase(pts, m_baseCount++);
    }

    void SetUp() override
    {
        BaseClass::SetUp();

        // This is the same as what we do in the sendAllUntilAllFull test, but without tests for
        // callback events (and with disposal of allocs that failed to send).

        while (allocFeedOutput()) {}

        bool anySucceeded = true;
        for (int64_t pts = 0; anySucceeded; pts++) {
            anySucceeded = false;

            if (allocFeedEnhancement(pts)) {
                anySucceeded = true;
            }
            if (allocFeedBase(pts)) {
                anySucceeded = true;
            }
            if (allocFeedOutput()) {
                anySucceeded = true;
            }
        }
    }

    std::atomic<uint64_t> m_baseCount = 0;
};

// - Tests ----------------------------------------------------------------------------------------

// - Non-fixture tests --------------------------

TEST(decoderInitRelease, initialiseAndRelease)
{
    LCEVC_DecoderHandle throwawayDecoderHdl{kInvalidHandle};
    const LCEVC_AccelContextHandle throwawayAccelContextHdl{kInvalidHandle};
    Decoder dec(throwawayAccelContextHdl, throwawayDecoderHdl);
    EXPECT_FALSE(dec.isInitialized());
    EXPECT_TRUE(dec.initialize());
    EXPECT_TRUE(dec.isInitialized());
    dec.release();
    EXPECT_FALSE(dec.isInitialized());
}

TEST(decoderInitRelease, initAfterValidConfigs)
{
    LCEVC_DecoderHandle throwawayDecoderHdl{kInvalidHandle};
    const LCEVC_AccelContextHandle throwawayAccelContextHdl{kInvalidHandle};
    Decoder dec(throwawayAccelContextHdl, throwawayDecoderHdl);

    EXPECT_TRUE(dec.setConfig("loq_unprocessed_cap", kUnprocessedCap));
    EXPECT_TRUE(dec.setConfig("results_queue_cap", kResultsQueueCap));
    EXPECT_TRUE(dec.initialize());
}

TEST(decoderInitRelease, initAfterInvalidConfigs)
{
    LCEVC_DecoderHandle throwawayDecoderHdl{kInvalidHandle};
    const LCEVC_AccelContextHandle throwawayAccelContextHdl{kInvalidHandle};
    Decoder dec(throwawayAccelContextHdl, throwawayDecoderHdl);

    // Configs that exist will pass configuration, but if the values are unusable, then init fails.
    EXPECT_TRUE(dec.setConfig("loq_unprocessed_cap", -2));
    EXPECT_TRUE(dec.setConfig("results_queue_cap", -1000));
    EXPECT_FALSE(dec.initialize());
}

TEST(decoderInitRelease, initAfterNonexistentConfigs)
{
    LCEVC_DecoderHandle throwawayDecoderHdl{kInvalidHandle};
    const LCEVC_AccelContextHandle throwawayAccelContextHdl{kInvalidHandle};
    Decoder dec(throwawayAccelContextHdl, throwawayDecoderHdl);

    // Configs that don't exist will fail to set, but will not prevent init.
    EXPECT_FALSE(dec.setConfig("allConfigsShouldBeSnakeCase", true));
    EXPECT_FALSE(
        dec.setConfig("results_queue_cap", std::string("int configs should only be set to ints")));
    EXPECT_TRUE(dec.initialize());
}

// - DecoderFixtureUninitialised ----------------

TEST_F(DecoderFixtureUninitialised, initEvents)
{
    for (const std::atomic<uint32_t>& count : m_callbackCounts) {
        EXPECT_EQ(count.load(), 0);
    }
    m_decoder.setEventCallback(DecoderFixtureUninitialised::callback, this);
    for (const std::atomic<uint32_t>& count : m_callbackCounts) {
        EXPECT_EQ(count.load(), 0);
    }
    EXPECT_TRUE(m_decoder.setConfig("events", kAllEvents));

    // Set expected callback results:
    m_expectedCallbackResults[LCEVC_CanSendBase].emplace(m_pretendDecoderHdl.hdl);
    m_expectedCallbackResults[LCEVC_CanSendEnhancement].emplace(m_pretendDecoderHdl.hdl);
    m_expectedCallbackResults[LCEVC_CanSendPicture].emplace(m_pretendDecoderHdl.hdl);

    m_decoder.initialize();

    // All callbacks should be 0 except "can send":
    bool didTimeout = false;
    for (int32_t eventType = 0; eventType < LCEVC_EventCount; eventType++) {
        switch (eventType) {
            case LCEVC_CanSendBase:
            case LCEVC_CanSendEnhancement:
            case LCEVC_CanSendPicture: {
                EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[eventType], 1));
                ASSERT_FALSE(didTimeout);
                break;
            }

            default: {
                EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[eventType], 0));
                ASSERT_FALSE(didTimeout);
                break;
            }
        }
    }
}

// - DecoderFixture -----------------------------

// Death tests are dangerous in multithreaded environments, so if you label them DeathTest, Gtest
// will run them in a safer way (all at once, before other tests).
using DecoderFixtureDeathTest = DecoderFixture;

TEST_F(DecoderFixtureDeathTest, picturePoolInterfaceManaged)
{
    // This is a light test: it only confirms that decoder provides a valid interface to a pool of
    // pictures. Pool itself is tested elsewhere.

    LCEVC_PictureDesc desc;
    ASSERT_EQ(LCEVC_DefaultPictureDesc(&desc, LCEVC_I420_8, 1920, 1080), LCEVC_Success);

    LCEVC_PictureHandle managedPicture{kInvalidHandle};
    // getting a picture that hasn't been allocated should cause an assert, hence "death". However,
    // we don't care what the assert message says, hence "Assertion [anything] failed".
    VN_EXPECT_DEATH(m_decoder.getPicture(managedPicture.hdl), "Assertion .* failed", nullptr);

    EXPECT_TRUE(m_decoder.allocPictureManaged(desc, managedPicture));
    ASSERT_NE(m_decoder.getPicture(managedPicture.hdl), nullptr);
    LCEVC_PictureDesc descThatWeActuallyGot;
    m_decoder.getPicture(managedPicture.hdl)->getDesc(descThatWeActuallyGot);
    EXPECT_TRUE(equals(desc, descThatWeActuallyGot));
    EXPECT_TRUE(m_decoder.releasePicture(managedPicture.hdl));

    VN_EXPECT_DEATH(m_decoder.getPicture(managedPicture.hdl), "Assertion .* failed", nullptr);
}

TEST_F(DecoderFixtureDeathTest, picturePoolInterfaceExternal)
{
    // Copy of the above, but with some externally allocated memory
    LCEVC_PictureDesc desc;
    ASSERT_EQ(LCEVC_DefaultPictureDesc(&desc, LCEVC_I420_8, 1920, 1080), LCEVC_Success);

    LCEVC_PictureHandle externalPicture{kInvalidHandle};
    VN_EXPECT_DEATH(m_decoder.getPicture(externalPicture.hdl), "Assertion .* failed", nullptr);

    const std::unique_ptr<uint8_t[]> overallBuffer = std::make_unique<uint8_t[]>(1920 * 1080 * 3 / 2);
    const LCEVC_PictureBufferDesc bufferDesc = {overallBuffer.get(),
                                                1920 * 1080 * 3 / 2,
                                                {kInvalidHandle},
                                                LCEVC_Access_Modify};
    const LCEVC_PicturePlaneDesc i420In3Planes[kI420NumPlanes] = {
        {overallBuffer.get(), 1920},
        {overallBuffer.get() + 1920 * 1080, 960},
        {overallBuffer.get() + 1920 * 1080 + 960 * 540, 960},
    };
    EXPECT_TRUE(m_decoder.allocPictureExternal(desc, externalPicture, i420In3Planes, &bufferDesc));
    EXPECT_NE(m_decoder.getPicture(externalPicture.hdl), nullptr);
    EXPECT_TRUE(m_decoder.releasePicture(externalPicture.hdl));

    VN_EXPECT_DEATH(m_decoder.getPicture(externalPicture.hdl), "Assertion .* failed", nullptr);
}

// - DecoderFixtureWithData ---------------------

using DecoderFixtureWithDataDeathTest = DecoderFixtureWithData;

TEST_F(DecoderFixtureWithDataDeathTest, pictureLockPoolInterface)
{
    // Like picture pool tests, but even lighter.

    VN_EXPECT_DEATH(m_decoder.getPictureLock(kInvalidHandle), "Assertion .* failed", nullptr);
    EXPECT_FALSE(m_decoder.unlockPicture(kInvalidHandle));

    allocBaseManaged(0); // ID literally does not matter here.
    LCEVC_PictureLockHandle lockHandle;
    Picture& picToLock = *m_decoder.getPicture(m_bases.back().handle);
    EXPECT_TRUE(m_decoder.lockPicture(picToLock, Access::Read, lockHandle));
    EXPECT_NE(lockHandle.hdl, kInvalidHandle);
    // can't re-lock at any access level, whether it's stricter or looser.
    EXPECT_FALSE(m_decoder.lockPicture(picToLock, Access::Read, lockHandle));
    EXPECT_FALSE(m_decoder.lockPicture(picToLock, Access::Write, lockHandle));
    EXPECT_FALSE(m_decoder.lockPicture(picToLock, Access::Modify, lockHandle));
    EXPECT_NE(m_decoder.getPictureLock(lockHandle.hdl), nullptr);

    // Don't worry about the lock actually working (that'll be tested in picture.h unit tests).

    EXPECT_TRUE(m_decoder.unlockPicture(lockHandle.hdl));
    VN_EXPECT_DEATH(m_decoder.getPictureLock(lockHandle.hdl), "Assertion .* failed", nullptr);
    EXPECT_FALSE(m_decoder.unlockPicture(lockHandle.hdl));
}

TEST_F(DecoderFixtureWithData, sendEnhancement)
{
    int64_t pts = 0;

    // Unlike sendBase/sendOutputPicture, this is non-fatal (because you can operate without
    // enhancement, but not without input/output).
    EXPECT_EQ(m_decoder.feedEnhancementData(0, false, nullptr, 10), LCEVC_Error);

    EnhancementWithData enhancement = getEnhancement(pts);
    EXPECT_EQ(m_decoder.feedEnhancementData(pts++, false, enhancement.first, enhancement.second),
              LCEVC_Success);

    // Alloc until too many:
    do {
        enhancement = getEnhancement(pts);
    } while (m_decoder.feedEnhancementData(pts++, false, enhancement.first, enhancement.second) ==
             LCEVC_Success);

    // Further enhancement data should return "again"
    enhancement = getEnhancement(pts);
    EXPECT_EQ(m_decoder.feedEnhancementData(pts++, false, enhancement.first, enhancement.second),
              LCEVC_Again);
}

TEST_F(DecoderFixtureWithDataDeathTest, sendBase)
{
    // Whenever doing an allocate-and-feed loop, you feed from the back (because that's where the
    // newly allocated data has gone).
    int64_t pts = 0;

    VN_EXPECT_DEATH(m_decoder.feedBase(0, false, kInvalidHandle, UINT32_MAX, nullptr),
                    "Assertion .* failed", LCEVC_Error);
    allocBaseManaged(pts);
    EXPECT_EQ(m_decoder.feedBase(pts++, false, m_bases.back().handle, UINT32_MAX, nullptr), LCEVC_Success);

    // Alloc until too many:
    do {
        allocBaseManaged(pts);
    } while (m_decoder.feedBase(pts++, false, m_bases.back().handle, UINT32_MAX, nullptr) == LCEVC_Success);

    // Further bases should return "again"
    allocBaseManaged(pts);
    EXPECT_EQ(m_decoder.feedBase(pts++, false, m_bases.back().handle, UINT32_MAX, nullptr), LCEVC_Again);
}

TEST_F(DecoderFixtureWithDataDeathTest, sendOutputPicture)
{
    VN_EXPECT_DEATH(m_decoder.feedOutputPicture(kInvalidHandle), "Assertion .* failed", LCEVC_Error);
    allocOutputManaged();
    EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Success);

    // Alloc until too many:
    do {
        allocOutputManaged();
    } while (m_decoder.feedOutputPicture(m_outputs.back().first.hdl) == LCEVC_Success);

    // Further outputs should return "again"
    allocOutputManaged();
    EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Again);
}

TEST_F(DecoderFixtureWithData, sendAllOnce)
{
    // Purpose of test:
    // When you send everything needed for a decode, you get 2 callbacks: LCEVC_CanReceive and
    //    LCEVC_BasePictureDone. These indicate that a decode occurred.

    EXPECT_EQ(m_callbackCounts[LCEVC_CanReceive], 0);
    EXPECT_EQ(m_callbackCounts[LCEVC_BasePictureDone], 0);

    LCEVC_ReturnCode outputResult = LCEVC_Error;
    LCEVC_ReturnCode enhancedResult = LCEVC_Error;
    LCEVC_ReturnCode baseResult = LCEVC_Error;
    sendOneOfEach(0, outputResult, enhancedResult, baseResult);
    EXPECT_EQ(outputResult, LCEVC_Success);
    EXPECT_EQ(enhancedResult, LCEVC_Success);
    EXPECT_EQ(baseResult, LCEVC_Success);

    bool wasTimeout = false;
    EXPECT_TRUE(atomicWaitUntil(wasTimeout, equal, m_callbackCounts[LCEVC_CanReceive], 1));
    EXPECT_TRUE(atomicWaitUntil(wasTimeout, equal, m_callbackCounts[LCEVC_BasePictureDone], 1));
    EXPECT_FALSE(wasTimeout);
}

TEST_F(DecoderFixtureWithData, receiveBase)
{
    // Test that you can receive the base that you've sent.

    LCEVC_PictureHandle baseHandle{kInvalidHandle};
    EXPECT_EQ(m_decoder.produceFinishedBase(baseHandle), LCEVC_Again);
    EXPECT_EQ(baseHandle.hdl, kInvalidHandle);

    LCEVC_ReturnCode dummy = LCEVC_Error;
    sendOneOfEach(0, dummy, dummy, dummy);

    // should now be able to receive the base that we sent.
    EXPECT_EQ(m_decoder.produceFinishedBase(baseHandle), LCEVC_Success);
    EXPECT_EQ(baseHandle.hdl, m_bases.back().handle.handle);
}

TEST_F(DecoderFixtureWithData, sendAllUntilAllFull)
{
    // Purpose of test:
    // a) We've shown that we receive LCEVC_Again when we max out one send, now check that we get
    //    them when we max out all 3 sends, together. AND,
    // b) Check that the sends, when done together, trigger BOTH the "decode done" callbacks (which
    //    are LCEVC_CanReceive and LCEVC_BasePictureDone), AND the 3 LCEVC_CanSend... callbacks

    std::map<LCEVC_Event, uint32_t> successes;

    // Wait for the first events to come through, to isolate the following tests (assert, because
    // we already confirmed this behaviour in TEST_F(DecoderFixtureUninitialised, initEvents).
    bool didTimeout = false;
    ASSERT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendEnhancement], 1));
    ASSERT_FALSE(didTimeout);
    ASSERT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendBase], 1));
    ASSERT_FALSE(didTimeout);
    ASSERT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendPicture], 1));
    ASSERT_FALSE(didTimeout);

    // Gather the initial counts
    std::array<uint32_t, LCEVC_EventCount> oldCounts = {};
    for (size_t event = 0; event < m_callbackCounts.size(); event++) {
        oldCounts[event] = m_callbackCounts[event];
    }

    // Fill up the outputs (to avoid premature failures of the send functions).
    while (true) {
        allocOutputManaged();
        if (m_decoder.feedOutputPicture(m_outputs.back().first.hdl) == LCEVC_Success) {
            successes[LCEVC_CanSendPicture]++;
        } else {
            break;
        }
    }

    // Make sure that sending the outputs DIDN'T trigger any events, i.e. they're STILL 1.
    EXPECT_EQ(oldCounts[LCEVC_CanSendEnhancement], 1);
    EXPECT_EQ(oldCounts[LCEVC_CanSendBase], 1);
    EXPECT_EQ(oldCounts[LCEVC_CanSendPicture], 1);

    // Now, cycle through enhancements, bases, and outputs, until they ALL fail.
    bool lastSendSucceeded = true;
    static const LCEVC_Event sendTypes[3] = {LCEVC_CanSendPicture, LCEVC_CanSendEnhancement,
                                             LCEVC_CanSendBase};
    std::map<LCEVC_Event, LCEVC_ReturnCode> lastSendResults;
    std::map<LCEVC_Event, LCEVC_ReturnCode> newSendResults;
    int64_t pts = 0;
    for (; lastSendSucceeded; pts++) {
        // Do this right before sending, to make sure we've finished waiting for all callbacks from
        // the last loop.
        for (const LCEVC_Event sendType : sendTypes) {
            oldCounts[sendType] = m_callbackCounts[sendType];
        }

        sendOneOfEach(pts, newSendResults[LCEVC_CanSendPicture],
                      newSendResults[LCEVC_CanSendEnhancement], newSendResults[LCEVC_CanSendBase]);

        for (const LCEVC_Event sendType : sendTypes) {
            switch (newSendResults[sendType]) {
                case LCEVC_Again: break;
                case LCEVC_Success:
                    successes[sendType]++;
                    // fallthrough for all non-"again" results
                default: {
                    if (lastSendResults[sendType] == LCEVC_Again) {
                        // We can't know exactly how many decodes occurred, so we can't know how
                        // many "canSend" callbacks we got. Just expect that it increased.
                        EXPECT_TRUE(atomicWaitUntil(didTimeout, greaterThan,
                                                    m_callbackCounts[sendType], oldCounts[sendType]));
                        ASSERT_FALSE(didTimeout);
                    }
                    break;
                }
            }

            lastSendResults[sendType] = newSendResults[sendType];

            lastSendSucceeded = false;
            if (lastSendResults[sendType] == LCEVC_Success) {
                lastSendSucceeded = true;
            }
        }
    }

    // Expect that we got a "canReceive" and a "basePictureDone" for each decode that we think
    // happened. This will be whichever is less: the number of sent-bases that aren't on the base
    // queue, or the number of sent-outputs that aren't still on the output queue.
    const uint64_t usedBases = successes[LCEVC_CanSendBase] - kUnprocessedCap;
    const uint64_t usedOutputs = successes[LCEVC_CanSendPicture] - kUnprocessedCap;
    const uint64_t expectedDecodes = std::min(usedBases, usedOutputs);
    EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanReceive],
                                static_cast<uint32_t>(expectedDecodes)));
    EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_BasePictureDone],
                                static_cast<uint32_t>(expectedDecodes)));
    ASSERT_FALSE(didTimeout);

    // This is overkill but, it's conceivable that, though we've tested the sends individually,
    // they don't work all at the same time (we want them ALL to return LCEVC_Again now)
    const auto [enhancementData, enhancementSize] = getEnhancement(pts);
    EXPECT_EQ(m_decoder.feedEnhancementData(pts++, false, enhancementData, enhancementSize), LCEVC_Again);
    allocOutputManaged();
    EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Again);
    allocBaseManaged(pts);
    EXPECT_EQ(m_decoder.feedBase(pts, false, m_bases.back().handle, UINT32_MAX, this + pts), LCEVC_Again);
}

// - DecoderFixturePreFilled --------------------

// Flush

TEST_F(DecoderFixturePreFilled, flushClearsBases)
{
    const int64_t pts = 100;
    allocBaseManaged(pts);
    EXPECT_EQ(m_decoder.feedBase(pts, false, m_bases.back().handle, UINT32_MAX, m_bases.back().id),
              LCEVC_Again);
    m_decoder.flush();
    EXPECT_EQ(m_decoder.feedBase(pts, false, m_bases.back().handle, UINT32_MAX, m_bases.back().id),
              LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, flushClearsEnhancements)
{
    const int64_t pts = 100;
    const auto [enhancementData, enhancementSize] = getEnhancement(pts);
    EXPECT_EQ(m_decoder.feedEnhancementData(pts, false, enhancementData, enhancementSize), LCEVC_Again);
    m_decoder.flush();
    EXPECT_EQ(m_decoder.feedEnhancementData(pts, false, enhancementData, enhancementSize), LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, flushClearsOutputPictures)
{
    allocOutputManaged();
    EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Again);
    m_decoder.flush();
    EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, flushCausesReceiveToReturnFlushed)
{
    LCEVC_PictureHandle picOut;
    LCEVC_DecodeInformation infoOut;
    EXPECT_EQ(m_decoder.produceOutputPicture(picOut, infoOut), LCEVC_Success);
    m_decoder.flush();
    EXPECT_EQ(m_decoder.produceOutputPicture(picOut, infoOut), LCEVC_Flushed);
}

// Skip

TEST_F(DecoderFixtureWithData, skipOneBase)
{
    EXPECT_TRUE(allocFeedBase(0));

    // You can tell that a base was skipped, because you can "receive" it.
    LCEVC_PictureHandle hdl;
    EXPECT_EQ(m_decoder.produceFinishedBase(hdl), LCEVC_Again);
    EXPECT_EQ(m_decoder.skip(0), LCEVC_Success);
    EXPECT_EQ(m_decoder.produceFinishedBase(hdl), LCEVC_Success);
    EXPECT_EQ(hdl.hdl, m_bases.front().handle.handle);
}

TEST_F(DecoderFixtureWithData, skipOneEnhancement)
{
    const auto [enhancementData, enhancementSize] = getEnhancement(static_cast<int64_t>(0));
    EXPECT_EQ(m_decoder.feedEnhancementData(0, false, enhancementData, enhancementSize), LCEVC_Success);

    // You can't actually "receive" any skipped-result from a skipped enhancement, since there's
    // no output picture that it corresponds to. However, we DO know that you can't send two
    // enhancements with the same timestamp. So, use that as a proxy to determine if the
    // enhancement was skipped.

    EXPECT_EQ(m_decoder.feedEnhancementData(0, false, enhancementData, enhancementSize), LCEVC_Error);
    EXPECT_EQ(m_decoder.skip(0), LCEVC_Success);
    EXPECT_EQ(m_decoder.feedEnhancementData(0, false, enhancementData, enhancementSize), LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, skipClearsSomeBases)
{
    // Like "skipOneBase", but for macro-level changes to an already-filled decoder

    // full before
    const Picture* backPic = m_decoder.getPicture(m_bases.back().handle);
    const uint64_t nextTH = backPic->getTimehandle() + 1;
    allocBaseManaged(nextTH);
    EXPECT_EQ(m_decoder.feedBase(nextTH, false, m_bases.back().handle, UINT32_MAX, m_bases.back().id),
              LCEVC_Again);

    // non-full after
    const Picture* frontPic = m_decoder.getPicture(m_bases.front().handle);
    const uint64_t middleTH = (frontPic->getTimehandle() + backPic->getTimehandle() / 2);
    m_decoder.skip(static_cast<int64_t>(middleTH));
    EXPECT_EQ(m_decoder.feedBase(nextTH, false, m_bases.back().handle, UINT32_MAX, m_bases.back().id),
              LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, skipClearsSomeEnhancements)
{
    // Like "skipOneEnhancement", but for macro-level changes to an already-filled decoder

    // full before
    const Picture* backPic = m_decoder.getPicture(m_bases.back().handle);
    const uint64_t nextTH = backPic->getTimehandle() + 1;
    const auto [enhancementData, enhancementSize] = getEnhancement(static_cast<int64_t>(nextTH));
    EXPECT_EQ(m_decoder.feedEnhancementData(nextTH, false, enhancementData, enhancementSize), LCEVC_Again);

    // non-full after
    const Picture* frontPic = m_decoder.getPicture(m_bases.front().handle);
    const uint64_t middleTH = (frontPic->getTimehandle() + backPic->getTimehandle() / 2);
    m_decoder.skip(static_cast<int64_t>(middleTH));
    EXPECT_EQ(m_decoder.feedEnhancementData(nextTH, false, enhancementData, enhancementSize), LCEVC_Success);
}

TEST_F(DecoderFixturePreFilled, skipDecodesAfterSkippedFrames)
{
    // Should get handles which match those we sent (m_outputs), but filled with data that match
    // the bases which generated them (m_bases).

    // Skip the middle.
    const Picture* frontPic = m_decoder.getPicture(m_bases.front().handle);
    const Picture* backPic = m_decoder.getPicture(m_bases.back().handle);
    const auto skippedTimestamp =
        static_cast<int64_t>(frontPic->getTimehandle() + backPic->getTimehandle() / 2);
    m_decoder.skip(skippedTimestamp);

    // Now, expect them ALL to be skipped, UNTIL you exceed the skipped timehandle
    LCEVC_PictureHandle picHandleOut;
    LCEVC_DecodeInformation infoOut;
    const Picture* latestDecodedPic = nullptr;
    const Picture* nextExpectedBase = nullptr;
    m_decoder.produceOutputPicture(picHandleOut, infoOut);
    while (infoOut.timestamp <= skippedTimestamp) {
        EXPECT_TRUE(infoOut.skipped);
        EXPECT_EQ(picHandleOut.hdl, m_outputs.front().first.hdl);
        m_outputs.pop_front();

        nextExpectedBase = m_decoder.getPicture(m_bases.front().handle);
        EXPECT_EQ(infoOut.timestamp, timehandleGetTimestamp(nextExpectedBase->getTimehandle()));

        m_bases.pop_front();
        m_decoder.produceOutputPicture(picHandleOut, infoOut);
    }

    // This should now be the first NON skipped frame.
    EXPECT_FALSE(infoOut.skipped);

    // Several bases will have never even reached the decode step, so skip ahead to the first base
    // that SHOULD be non-skipped.
    while (static_cast<int64_t>(nextExpectedBase->getTimehandle()) <= skippedTimestamp) {
        m_bases.pop_front();
        nextExpectedBase = m_decoder.getPicture(m_bases.front().handle);
    }
    latestDecodedPic = m_decoder.getPicture(picHandleOut.hdl);
    EXPECT_EQ(picHandleOut.hdl, m_outputs.front().first.hdl);
    EXPECT_EQ(latestDecodedPic->getTimehandle(), nextExpectedBase->getTimehandle());
}

// ProduceOutputPicture and callbacks

TEST_F(DecoderFixturePreFilled, receiveOneOutputFromFullDecoder)
{
    // The purpose of this test is: Now that we know that we can fill up the decoder's queues,
    // check that we can produce one output. This means:
    // a) When we go from full to partly-full, we get one "canSend" for all send types. (note that
    //    we would NOT expect this for ALL send types if they had different queue capacities).
    // b) The decoded picture has the LCEVC_DecodeInformation that we expect.
    // c) Every "produceOutput" gives us a correct "outputPictureDone" callback.

    std::array<uint32_t, LCEVC_EventCount> oldCounts = {};
    for (size_t event = 0; event < m_callbackCounts.size(); event++) {
        oldCounts[event] = m_callbackCounts[event];
    }

    // Check decode info
    {
        m_expectedCallbackResults[LCEVC_OutputPictureDone].emplace(m_pretendDecoderHdl.hdl,
                                                                   m_outputs.front().first.hdl);

        LCEVC_DecodeInformation infoOut;
        LCEVC_PictureHandle outputHdl;
        EXPECT_EQ(m_decoder.produceOutputPicture(outputHdl, infoOut), LCEVC_Success);
        EXPECT_EQ(m_outputs.front().first.hdl, outputHdl.hdl);

        const BaseWithData& expectedBaseData = m_bases.front();
        Picture* expectedBase = m_decoder.getPicture(expectedBaseData.handle);

        EXPECT_EQ(infoOut.baseBitdepth, expectedBase->getBitdepth());
        EXPECT_EQ(infoOut.baseHeight, expectedBase->getHeight());
        EXPECT_EQ(infoOut.baseUserData, m_bases.front().id);
        EXPECT_EQ(infoOut.baseWidth, expectedBase->getWidth());
        EXPECT_EQ(infoOut.enhanced, true);
        EXPECT_EQ(infoOut.hasBase, true);
        EXPECT_EQ(infoOut.hasEnhancement, true);
        EXPECT_EQ(infoOut.skipped, false);
        EXPECT_EQ(infoOut.timestamp, expectedBase->getTimehandle());
    }

    // Check callback stuff: one output should be done, and since this cleared a space at the end
    // of the assembly line, we expect one more decode to have occurred, and therefore one more
    // space to have opened up in each input queue.
    {
        bool didTimeout = false;

        // Output used:
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_OutputPictureDone], 1));
        // Decode occurred:
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanReceive],
                                    oldCounts[LCEVC_CanReceive] + 1));
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_BasePictureDone],
                                    oldCounts[LCEVC_BasePictureDone] + 1));
        // Spaces have opened up at the input side:
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendBase],
                                    oldCounts[LCEVC_CanSendBase] + 1));
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendEnhancement],
                                    oldCounts[LCEVC_CanSendEnhancement] + 1));
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_CanSendPicture],
                                    oldCounts[LCEVC_CanSendPicture] + 1));
        ASSERT_FALSE(didTimeout);
    }
}

TEST_F(DecoderFixturePreFilled, receiveAllOutputFromFullDecoder)
{
    // The purpose of this test is: Now that we know that we can receive one decode, check that we
    // can receive all of them, in the right order. So:
    // a) Every "produceOutput" gives us a correct "outputPictureDone" callback, i.e. for THAT
    //    frame's picture.
    // b) After we empty the queues, we get "LCEVC_Again".

    std::array<uint32_t, LCEVC_EventCount> oldCounts = {};
    for (size_t event = 0; event < m_callbackCounts.size(); event++) {
        oldCounts[event] = m_callbackCounts[event];
    }

    const BaseWithData& firstBaseData = m_bases.front();
    const Picture* const firstBase = m_decoder.getPicture(firstBaseData.handle);

    bool didTimeout = false;
    LCEVC_PictureHandle outputHdlOut = {};
    LCEVC_DecodeInformation infoOut;
    uint32_t numOutputsProduced = 0;
    m_expectedCallbackResults[LCEVC_OutputPictureDone].emplace(m_pretendDecoderHdl.hdl,
                                                               m_outputs.front().first.hdl);

    for (uint64_t pts = firstBase->getTimehandle();
         m_decoder.produceOutputPicture(outputHdlOut, infoOut) == LCEVC_Success; pts++) {
        numOutputsProduced++;

        // erase "used" entries.
        ASSERT_FALSE(m_outputs.empty());
        m_outputs.pop_front();
        m_bases.pop_front();

        // Don't check most of the decode info, just the timestamp (to make sure they're in order).
        EXPECT_EQ(infoOut.timestamp, pts);

        // Check that the callback got sent, then set up expectation for the next one. Note that
        // there might not be a next one if this was the last iteration (i.e. if m_outputs was
        // emptied when we called pop_front).
        EXPECT_TRUE(atomicWaitUntil(didTimeout, equal, m_callbackCounts[LCEVC_OutputPictureDone], numOutputsProduced))
            << "Count was " << m_callbackCounts[LCEVC_OutputPictureDone].load()
            << " but we expected " << numOutputsProduced << "\n";
        ASSERT_FALSE(didTimeout);
        m_expectedCallbackResults[LCEVC_OutputPictureDone].emplace(
            m_pretendDecoderHdl.hdl, (m_outputs.empty() ? kInvalidHandle : m_outputs.front().first.hdl));
    }

    // Confirm that, once you empty out the decoded outputs, you get "LCEVC_Again" (as in "try
    // again later, once you've sent a base and output).
    if (m_outputs.empty()) {
        allocOutputManaged();
    }
    EXPECT_EQ(m_decoder.produceOutputPicture(m_outputs.front().first, infoOut), LCEVC_Again);
}

TEST_F(DecoderFixturePreFilled, receiveUntilAllHandlesHaveBeenReused)
{
    // The purpose of this test is: check that, if we start reusing old handles, our decodes still
    // succeed. That makes this a fairly light test.

    // limit is more than the max capacity of ANY list, so all lists get fully swapped out.
    const uint32_t limit = 2 * std::max(kUnprocessedCap, kResultsQueueCap);

    // Variables that are updated over the course of the loop
    LCEVC_DecodeInformation infoOut;

    // iteration variables
    const BaseWithData& lastBaseData = m_bases.back();
    const Picture* const lastBase = m_decoder.getPicture(lastBaseData.handle);
    int64_t newPTS = static_cast<int64_t>(lastBase->getTimehandle()) + 1;
    const int64_t endPTS = newPTS + limit;
    for (; newPTS < endPTS; newPTS++) {
        // extract
        LCEVC_PictureHandle outputHdl;
        EXPECT_EQ(m_decoder.produceOutputPicture(outputHdl, infoOut), LCEVC_Success);

        // Erase used base
        m_bases.pop_front();

        // RECYCLE the output holder
        m_outputs.push_back(m_outputs.front()); // re-queue at the back
        m_outputs.pop_front();

        // Queue more (only alloc new base and enhancement, output is already alloc'd)
        EXPECT_TRUE(allocFeedEnhancement(newPTS));
        EXPECT_TRUE(allocFeedBase(newPTS));
        EXPECT_EQ(m_decoder.feedOutputPicture(m_outputs.back().first.hdl), LCEVC_Success);
    }
}

// - Peek ---------------------------------------

// A decoder, parameterized on passthrough mdode
class DecoderFixturePassthrough
    : public DecoderFixtureWithData
    , public testing::WithParamInterface<int32_t>
{
    using BaseClass = DecoderFixtureWithData;

public:
    void SetUp() override
    {
        m_decoder.setConfig("passthrough_mode", GetParam());
        BaseClass::SetUp();
    }

protected:
    static bool forcePassthrough() { return GetParam() == 1; }
    static bool allowPassthrough() { return GetParam() != -1; }
};

TEST_P(DecoderFixturePassthrough, peek)
{
    // Standard case: send a base and an enhancement, peek, and expect double-width and double-
    // height, plus "success" return code.
    allocFeedBase(0);
    allocFeedEnhancement(0);
    uint32_t width = 0;
    uint32_t height = 0;
    const uint32_t outputMultiplier = (forcePassthrough() ? 1 : 2);
    const LCEVC_ReturnCode outputResult = LCEVC_Success;
    EXPECT_EQ(m_decoder.peek(0, width, height), outputResult);
    EXPECT_EQ(width, m_inputDesc.width * outputMultiplier);
    EXPECT_EQ(height, m_inputDesc.height * outputMultiplier);
}

TEST_P(DecoderFixturePassthrough, peekBaseNoEnhancement)
{
    // Passthrough-allowed case: with no enhancement, you should get the original width and height
    allocFeedBase(0);
    uint32_t width = 0;
    uint32_t height = 0;
    const uint32_t outputMultiplier = (allowPassthrough() ? 1 : 0);
    const LCEVC_ReturnCode outputResult = (allowPassthrough() ? LCEVC_Success : LCEVC_NotFound);
    EXPECT_EQ(m_decoder.peek(0, width, height), outputResult);
    EXPECT_EQ(width, m_inputDesc.width * outputMultiplier);
    EXPECT_EQ(height, m_inputDesc.height * outputMultiplier);
}

TEST_P(DecoderFixturePassthrough, peekEnhancementNoBase)
{
    // Passthrough-allowed case: with no enhancement, you should get the original width and height
    allocFeedEnhancement(0);
    uint32_t width = 0;
    uint32_t height = 0;
    const uint32_t outputMultiplier = (forcePassthrough() ? 0 : 2);
    const LCEVC_ReturnCode outputResult = (forcePassthrough() ? LCEVC_NotFound : LCEVC_Success);
    EXPECT_EQ(m_decoder.peek(0, width, height), outputResult);
    EXPECT_EQ(width, m_inputDesc.width * outputMultiplier);
    EXPECT_EQ(height, m_inputDesc.height * outputMultiplier);
}

TEST_P(DecoderFixturePassthrough, peekInvalidCases)
{
    uint32_t width = 0;
    uint32_t height = 0;

    // Various invalid cases.

    // Invalid case 1: peeking a timestamp with no inputs whatsoever.
    EXPECT_EQ(m_decoder.peek(0, width, height), LCEVC_NotFound);

    // Case 2: Peeking the wrong timestamp
    allocFeedBase(100);
    EXPECT_EQ(m_decoder.peek(101, width, height), LCEVC_NotFound);

    // Case 3: Peeking into an otherwise valid input, but the base has timed out (the timeout is 0,
    // so it should timeout instantly). Also, if passthrough is allowed, then we expect timeouts to
    // turn into passthroughs, so check that the dimensions match up too.
    width = 0;
    height = 0;
    const uint32_t outputMultiplier = (allowPassthrough() ? 1 : 0);
    allocBaseManaged(0);
    allocFeedEnhancement(0);
    ASSERT_EQ(m_decoder.feedBase(0, false, m_bases.back().handle, 0, m_bases.back().id), LCEVC_Success);
    EXPECT_EQ(m_decoder.peek(0, width, height), LCEVC_Timeout);
    EXPECT_EQ(width, m_inputDesc.width * outputMultiplier);
    EXPECT_EQ(height, m_inputDesc.height * outputMultiplier);
}

// - Passthrough --------------------------------

TEST_P(DecoderFixturePassthrough, passthrough)
{
    // Send a full set, and expect success, plus enhanced iff not forcing passthrough. Likewise,
    // update m_outputDesc before alloc'ing an output, to ensure it matches expectations
    if (forcePassthrough()) {
        m_outputDesc = m_inputDesc;
    }
    sendOneOfEach(0);
    receiveOneOfEach(LCEVC_Success, !forcePassthrough());

    // Send a base and output but no enhancement: success if passthrough is allowed, but never
    // enhanced.
    m_outputDesc = m_inputDesc;
    sendOneOfEach(1, EnhancementOption::None);
    receiveOneOfEach(allowPassthrough() ? LCEVC_Success : LCEVC_Error, false);

    // Send a base, an output, and an EMPTY enhancement: success if passthrough is allowed, but
    // never enhanced (in other words, same as no enhancement).
    m_outputDesc = m_inputDesc;
    sendOneOfEach(2, EnhancementOption::Empty);
    receiveOneOfEach(allowPassthrough() ? LCEVC_Success : LCEVC_Error, false);
}

INSTANTIATE_TEST_SUITE_P(Peek, DecoderFixturePassthrough, testing::Values(-1, 0, 1));
