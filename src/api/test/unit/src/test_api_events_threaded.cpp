/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

// This tests api/include/LCEVC/lcevc_dec.h against event-based operation. This is similar to
// `test_event_manager`, but with a focus on how the code might realistically get used. For
// example, the event_manager tests wouldn't catch deadlocks caused by accessing the API from
// within a callback. These tests also test asynchronous operation.

#include "decoder_asynchronous.h"
#include "decoder_synchronous.h"

#include <gtest/gtest.h>
#include <LCEVC/api_utility/chrono.h>
#include <LCEVC/lcevc_dec.h>

#include <type_traits>

using namespace lcevc_dec::utility;

// Tests ------------------------------------------------------------------------------------------

static const uint64_t kNumFrames = 25;

template <typename T>
using APIEventReporting = testing::Test;

using MyTypes = ::testing::Types<DecoderSynchronous, DecoderAsynchronous>;
TYPED_TEST_SUITE(APIEventReporting, MyTypes);

TYPED_TEST(APIEventReporting, test)
{
    if (std::is_same_v<TypeParam, DecoderAsynchronous>) {
        GTEST_SKIP() << "DEC-593";
    }
    TypeParam tester(kNumFrames);

    // This will be fully automatic:
    // Test program                    | Decoder fn    | Decoder event
    // --------------------------------------------------------------
    // setup ->                        | initialise -> | "send" events ->
    // send callbacks ->               | decode ->     | "output/base done" events ->
    // "reuse" callbacks, new sends -> | decode ->     | More "done" events, repeat <--
    tester.setup();

    // Wait until tester is done, with a 150 second timeout for the entire 25-frame decode loop
    // (locally, this took about 150ms, but valgrind+release is often 7 seconds, and valgrind+debug
    // is often 50 seconds, so this leaves plenty of time).
    bool wasTimeout = false;
    const MilliSecond timeout(150000);
    ASSERT_TRUE(
        atomicWaitUntilTimeout(wasTimeout, timeout, [&tester]() { return tester.atomicIsDone(); }));
    EXPECT_FALSE(wasTimeout);

    tester.teardown(wasTimeout);

    for (uint8_t eventUInt = 0; eventUInt < LCEVC_EventCount; eventUInt++) {
        const auto eventType = static_cast<LCEVC_Event>(eventUInt);
        const uint32_t count = tester.getCount(eventType);
        switch (eventType) {
            case LCEVC_BasePictureDone:
            case LCEVC_OutputPictureDone:
            case LCEVC_CanReceive: EXPECT_EQ(count, kNumFrames); break;
            case LCEVC_Exit: EXPECT_EQ(count, 1); break;
            case LCEVC_CanSendBase:
            case LCEVC_CanSendEnhancement:
            case LCEVC_CanSendPicture: EXPECT_GT(count, 0);

            // LCEVC_Log is currently unused. The other two are non-valid enum values.
            case LCEVC_Log:
            case LCEVC_EventCount:
            case LCEVC_Event_ForceUInt8: continue;
        }
    }
}
