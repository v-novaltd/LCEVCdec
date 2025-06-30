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

// This tests api/src/event_dispatcher.h

#include <LCEVC/lcevc_dec.h>
//
#include "event.h"
#include "event_dispatcher.h"
#include "pool.h"
#include "utils.h"
//
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using namespace lcevc_dec::decoder;

// - Constants ------------------------------------------------------------------------------------

static const std::vector<int32_t> kArbitraryEvents = {LCEVC_CanSendBase, LCEVC_Exit, LCEVC_OutputPictureDone};

// - Fixtures -------------------------------------------------------------------------------------

class EventDispatcherFixture : public testing::Test
{
public:
    EventDispatcherFixture() {}

    void SetUp() override
    {
        m_dispatcher->enableEvents(kArbitraryEvents);
        m_dispatcher->setEventCallback(EventDispatcherFixture::callback, this);
    }

    void TearDown() override { m_dispatcher.release(); }

    static void callback(LCEVC_DecoderHandle, LCEVC_Event event, LCEVC_PictureHandle,
                         const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void* userData)
    {
        // We are emphatically NOT testing how this or that particular event works (that's in
        // test_decoder.cpp). This is simply a test that the callback happens at all.
        auto* ptrToCaller = static_cast<EventDispatcherFixture*>(userData);
        ptrToCaller->m_callbackCounts[event]++;
    }

protected:
    EventDispatcher& GetManager() { return *m_dispatcher; }
    EventCountArr& GetEventCounts() { return m_callbackCounts; }

private:
    std::unique_ptr<EventDispatcher> m_dispatcher{createEventDispatcher(nullptr)};
    EventCountArr m_callbackCounts = {};
    Pool<LdpPicture> m_picturePool{16};
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

// - EventDispatcher -------------------------------

// Meta tests

TEST(eventManagerInit, init)
{
    std::unique_ptr<EventDispatcher> dispatcher(createEventDispatcher(nullptr));

    dispatcher->enableEvents(kArbitraryEvents);
    for (int32_t event = 0; event < LCEVC_EventCount; event++) {
        const bool wasEnabled = (std::count(kArbitraryEvents.begin(), kArbitraryEvents.end(), event) > 0);
        EXPECT_EQ(dispatcher->isEventEnabled(event), wasEnabled);
    }
}

TEST(eventManagerInit, noCallbackUntilInit)
{
    EventCountArr callbackCounts = {};

    std::unique_ptr<EventDispatcher> dispatcher(createEventDispatcher(nullptr));

    // Technically this should be an "atomic wait" check but, we don't want to hard-code too many
    // waits into our tests, and it should generally be overkill since there's a wait at the end of
    // this test.
    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);
    dispatcher->generate(LCEVC_Exit);
    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);

    dispatcher->enableEvents(kArbitraryEvents);

    auto callback = [](LCEVC_DecoderHandle, LCEVC_Event event, LCEVC_PictureHandle,
                       const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void* userData) {
        auto* counts = static_cast<EventCountArr*>(userData);
        (*counts)[event]++;
    };

    dispatcher->setEventCallback(callback, &callbackCounts);

    EXPECT_EQ(callbackCounts[LCEVC_Exit], 0);
    dispatcher->generate(LCEVC_Exit);
    bool wasTimeout = false;
    atomicWaitUntil(wasTimeout, equal, callbackCounts[LCEVC_Exit], 1);
    EXPECT_FALSE(wasTimeout);
}
