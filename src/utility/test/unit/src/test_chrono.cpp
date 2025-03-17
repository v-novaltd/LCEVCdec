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

#include <gtest/gtest.h>
#include <LCEVC/api_utility/chrono.h>

#include <cstdint>
#include <thread>

using namespace lcevc_dec::utility;
using Clock = ScopedTimer<MicroSecond>;

TEST(ScopedTimerTest, IncrementValid)
{
    // ScopedTimer should measure its creation and deletion times.
    const TimePoint start = getTimePoint();
    Clock::Type clockLifespan = 0;
    {
        const Clock clock(&clockLifespan);
        std::this_thread::sleep_for(MicroSecond(10000));
    }
    const int64_t duration = getTimeSincePoint<MicroSecond>(start);
    EXPECT_GE(duration + 100000, clockLifespan);
    EXPECT_LE(duration - 100000, clockLifespan);
}
