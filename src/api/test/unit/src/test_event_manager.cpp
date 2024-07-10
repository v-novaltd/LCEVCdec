/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// This tests api/src/event_manager.h

#include "utils.h"

#include <event_manager.h>
#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>

#include <array>

using namespace lcevc_dec::decoder;

// - Constants ------------------------------------------------------------------------------------

static const std::vector<int32_t> kArbitraryEvents = {LCEVC_CanSendBase, LCEVC_Exit, LCEVC_OutputPictureDone};

// - Fixtures -------------------------------------------------------------------------------------

class EventManagerFixture : public testing::Test
{
public:
    EventManagerFixture()
        : m_manager(m_arbitraryHandle)
    {}

    void SetUp() override
    {
        m_manager.initialise(kArbitraryEvents);
        m_manager.setEventCallback(EventManagerFixture::callback, this);
    }

    void TearDown() override { m_manager.release(); }

    static void callback(Handle<Decoder>, int32_t event, Handle<Picture>,
                         const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void* userData)
    {
        // We are emphatically NOT testing how this or that particular event works (that's in
        // test_decoder.cpp). This is simply a test that the callback happens at all.
        auto* ptrToCaller = static_cast<EventManagerFixture*>(userData);
        ptrToCaller->m_callbackCounts[event]++;
    }

protected:
    EventManager& GetManager() { return m_manager; }
    EventCountArr& GetEventCounts() { return m_callbackCounts; }

private:
    LCEVC_DecoderHandle m_arbitraryHandle{123};
    EventManager m_manager;
    EventCountArr m_callbackCounts = {};
};

// - Tests ----------------------------------------------------------------------------------------

// - Event --------------------------------------

TEST(eventTests, validEvent)
{
    Event validEvent(LCEVC_Exit);
    EXPECT_EQ(validEvent.eventType, LCEVC_Exit);
    EXPECT_TRUE(validEvent.isValid());
    EXPECT_FALSE(validEvent.isFlush());
}

TEST(eventTests, invalidEvent)
{
    Event invalidEvent(LCEVC_EventCount);
    EXPECT_EQ(invalidEvent.eventType, LCEVC_EventCount);
    EXPECT_FALSE(invalidEvent.isValid());
    EXPECT_FALSE(invalidEvent.isFlush());
}

// - EventManager -------------------------------

// Meta tests

TEST(eventManagerInit, init)
{
    LCEVC_DecoderHandle junkHandle{123};
    EventManager manager(junkHandle);
    manager.initialise(kArbitraryEvents);
    for (int32_t event = 0; event < LCEVC_EventCount; event++) {
        const bool wasEnabled = (std::count(kArbitraryEvents.begin(), kArbitraryEvents.end(), event) > 0);
        EXPECT_EQ(manager.isEventEnabled(event), wasEnabled);
    }
}

TEST(eventManagerInit, noCallbackUntilInit)
{
    EventCountArr callbackCounts = {};
    LCEVC_DecoderHandle junkHandle{123};
    EventManager manager(junkHandle);

    // Technically this should be an "atomic wait" check but, we don't want to hard-code too many
    // waits into our tests, and it should generally be overkill since there's a wait at the end of
    // this test.
    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);
    manager.triggerEvent(LCEVC_Exit);
    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);

    manager.initialise(kArbitraryEvents);

    auto callback = [](Handle<Decoder>, int32_t event, Handle<Picture>,
                       const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void* userData) {
        auto* callbackCounts = static_cast<EventCountArr*>(userData);
        (*callbackCounts)[event]++;
    };

    manager.setEventCallback(callback, &callbackCounts);

    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);
    manager.triggerEvent(LCEVC_Exit);
    bool wasTimeout = false;
    atomicWaitUntil(wasTimeout, equal, callbackCounts[LCEVC_Exit], 1);
    EXPECT_FALSE(wasTimeout);
}

// Fixture tests

TEST_F(EventManagerFixture, release)
{
    // This test involves a hard-coded wait of 50ms... Sorry!
    GetManager().triggerEvent(LCEVC_CanSendBase);
    bool wasTimeout = false;
    atomicWaitUntil(wasTimeout, equal, GetEventCounts()[LCEVC_CanSendBase], 1);
    EXPECT_FALSE(wasTimeout);

    GetManager().release();
    GetManager().triggerEvent(LCEVC_CanSendBase);
    atomicWaitUntil(wasTimeout, equal, GetEventCounts()[LCEVC_CanSendBase], 2);
    EXPECT_TRUE(wasTimeout);
}
