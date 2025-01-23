/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#include "common/time.h"

#include "common/memory.h"
#include "context.h"

#if VN_OS(WINDOWS)
#include <Windows.h>
#else
#include <time.h>
#endif

/*------------------------------------------------------------------------------*/

typedef struct Time
{
    Memory_t memory;

#if VN_OS(WINDOWS)
    LARGE_INTEGER frequency;
#endif
} * Time_t;

/*------------------------------------------------------------------------------*/

bool timeInitialize(Memory_t memory, Time_t* time)
{
    Time_t result = VN_CALLOC_T(memory, struct Time);

    if (!result) {
        return false;
    }

    result->memory = memory;

#if VN_OS(WINDOWS)
    QueryPerformanceFrequency(&result->frequency);
#endif

    *time = result;
    return true;
}

void timeRelease(Time_t time)
{
    if (time) {
        VN_FREE(time->memory, time);
    }
}

uint64_t timeNowNano(Time_t time)
{
    if (!time) {
        return 0;
    }

#if VN_OS(WINDOWS)
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (currentTime.QuadPart * 1000000000) / time->frequency.QuadPart;
#else
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    return ((uint64_t)(currentTime.tv_sec) * 1000000000) + (uint64_t)currentTime.tv_nsec;
#endif
}

/*------------------------------------------------------------------------------*/
