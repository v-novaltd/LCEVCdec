/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This tests api/src/event_manager.h

#include "utils.h"

#include <LCEVC/lcevc_dec.h>
#include <event_manager.h>
#include <gtest/gtest.h>

#include <algorithm>

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
    EventManager m_manager;
    LCEVC_DecoderHandle m_arbitraryHandle{123};
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
    m_manager.triggerEvent(LCEVC_CanSendBase);
    bool wasTimeout = false;
    atomicWaitUntil(wasTimeout, equal, m_callbackCounts[LCEVC_CanSendBase], 1);
    EXPECT_FALSE(wasTimeout);

    m_manager.release();
    m_manager.triggerEvent(LCEVC_CanSendBase);
    atomicWaitUntil(wasTimeout, equal, m_callbackCounts[LCEVC_CanSendBase], 2);
    EXPECT_TRUE(wasTimeout);
}
