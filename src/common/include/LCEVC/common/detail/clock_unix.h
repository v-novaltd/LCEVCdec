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

/*
 * Unix implementation for common clock utilities.
 */
#ifndef VN_LCEVC_COMMON_DETAIL_CLOCK_UNIX_H
#define VN_LCEVC_COMMON_DETAIL_CLOCK_UNIX_H

#include <time.h>

static inline uint64_t ldcClockMonotonicNanoseconds(void)
{
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    return ((uint64_t)(currentTime.tv_sec) * 1000000000) + (uint64_t)currentTime.tv_nsec;
}

static inline uint64_t ldcClockWallSeconds(void)
{
    const time_t currentTime = time(NULL);
    return (currentTime >= 0) ? (uint64_t)currentTime : 0;
}

static inline uint64_t ldcClockDeadlineMicroseconds(int32_t offset)
{
    if ((uint32_t)offset == UINT32_MAX) {
        return UINT64_MAX;
    }

    struct timespec ts;

    // NB: Use CLOCK_REALTIME as pthread_cond_timedwait() uses that by default.
    clock_gettime(CLOCK_REALTIME, &ts);
    const uint64_t uSecs = (ts.tv_sec * 1000000LL) + (ts.tv_nsec / 1000L);

    return uSecs + offset;
}

#endif // VN_LCEVC_COMMON_DETAIL_CLOCK_UNIX_H
