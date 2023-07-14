/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

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
}* Time_t;

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
