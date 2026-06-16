/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include <LCEVC/common/clock.h>
#include <LCEVC/common/threads.h>
//
#include <gtest/gtest.h>
//
#include <cstdint>

TEST(ClockTest, MonotonicNanoseconds)
{
    const uint64_t before = ldcClockMonotonicNanoseconds();
    threadSleep(1);
    const uint64_t after = ldcClockMonotonicNanoseconds();

    EXPECT_LE(before, after);
}

TEST(ClockTest, WallSeconds) { EXPECT_GT(ldcClockWallSeconds(), 0); }

TEST(ClockTest, DeadlineMicroseconds)
{
    EXPECT_EQ(ldcClockDeadlineMicroseconds(static_cast<int32_t>(UINT32_MAX)), UINT64_MAX);

    const uint64_t before = ldcClockDeadlineMicroseconds(0);
    const uint64_t after = ldcClockDeadlineMicroseconds(1000);
    EXPECT_LT(before, after);
}
