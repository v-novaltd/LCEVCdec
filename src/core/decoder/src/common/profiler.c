/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/profiler.h"

#include "common/log.h"
#include "common/memory.h"
#include "common/threading.h"
#include "context.h"

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if VN_OS(WINDOWS)
#include <Windows.h>
#else
#include <time.h>
#endif

/*------------------------------------------------------------------------------*/

typedef enum SampleDataType
{
    SDTBegin = 0,
    SDTEnd
} SampleDataType_t;

typedef struct SampleData
{
    SampleDataType_t type;
    ProfileID_t pid;
    uint64_t time;
} SampleData_t;

typedef struct ProfileData ProfileData_t;

typedef struct ProfileData
{
    const char* label;
    const char* file;
    uint32_t line;
    int32_t threadID;
    ProfileID_t id;
    ProfileData_t* retrieveNext;
} ProfileData_t;

typedef struct ProfilerState
{
    Memory_t memory;
    Logger_t log;

    volatile int32_t threadID;
    volatile int32_t unknownThreadID;
    volatile ProfileID_t profileID;
    volatile int32_t profileNextIndex;
    volatile int32_t sampleNextIndex;

    SampleData_t sampleDummy;
    SampleData_t* sampleData;
    ProfileData_t* profileData;
    Mutex_t* profileDataMutex;
    ProfileData_t* retrieveHead;

    bool flush;
    bool lastFlush;
    bool flushed;
    uint32_t flushCounter;

    FILE* logFile;

#if VN_OS(WINDOWS)
    LARGE_INTEGER timeFrequency;
#endif

} ProfilerState_t;

#if VN_CORE_FEATURE(PROFILER)

static const uint32_t kSampleDataSize = 20000000;
static const uint32_t kProfileDataSize = 80000;
static VN_THREADLOCAL() int32_t gCurrentThreadID = 0;
static VN_THREADLOCAL() uint32_t gCurrentThreadStackIndex = 0;
static VN_THREADLOCAL() ProfileID_t gCurrentThreadStack[64];
static VN_THREADLOCAL() char gLabelFormatBuffer[256];

/* Set this to a valid string to dump the label sample times for that string in
 * the order the samples are received. This is a useful tool for "dynamic"
 * profiles where the call site is inherently single threaded and monotonic. */
static const char* kDumpSamplesLabel = NULL; /* "apply_plane loq=0 plane=0"; */
static const uint32_t kDumpSampleCapacity = 32768;

typedef struct ProfileStats
{
    uint64_t minTime;
    uint64_t maxTime;
    uint64_t accumTime;
    uint64_t count;
    ProfileData_t* profile;
    uint64_t sampleStart;
} ProfileStats_t;

#endif

/*------------------------------------------------------------------------------*/

#if VN_CORE_FEATURE(PROFILER)

static inline char* duplicateStr(Memory_t memory, const char* src)
{
    const size_t length = strlen(src) + 1;
    char* dst = VN_MALLOC_T_ARR(memory, char, length + 1);

    memcpy(dst, src, length);

    return dst;
}

static inline int32_t atomicIncrement(volatile int32_t* value)
{
#if VN_OS(WINDOWS)
    return InterlockedIncrement((volatile uint32_t*)value);
#else
    return __sync_fetch_and_add(value, 1) + 1;
#endif
}

static inline uint64_t getHighFrequencyTime(ProfilerState_t* profiler)
{
#if VN_OS(WINDOWS)
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return (time.QuadPart * 1000000000) / profiler->timeFrequency.QuadPart;
#else
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    return ((uint64_t)(current_time.tv_sec) * 1000000000) + (uint64_t)current_time.tv_nsec;
#endif
}

static inline int32_t getUnknownThreadID(ProfilerState_t* profiler)
{
    return atomicIncrement(&profiler->unknownThreadID);
}

static inline int32_t getCurrentThreadID(ProfilerState_t* profiler)
{
    if (gCurrentThreadID == 0) {
        VN_WARNING(profiler->log, "Warning current thread not registered - registering now\n");

        int32_t unknownID = getUnknownThreadID(profiler);
        char buff[128];
        sprintf(buff, "Unknown Thread - %u\n", unknownID);
        profilerRegisterThread(profiler, buff);
    }

    return gCurrentThreadID;
}

static inline int32_t nextThreadID(ProfilerState_t* profiler)
{
    return atomicIncrement(&profiler->threadID);
}

static inline ProfileID_t nextProfileID(ProfilerState_t* profiler)
{
    return atomicIncrement(&profiler->profileID);
}

static inline int32_t nextProfileDataIndex(ProfilerState_t* profiler)
{
    return atomicIncrement(&profiler->profileNextIndex) - 1;
}

static inline int32_t nextSampleDataIndex(ProfilerState_t* profiler)
{
    return atomicIncrement(&profiler->sampleNextIndex) - 1;
}

static inline SampleData_t* nextSampleData(ProfilerState_t* profiler)
{
    uint32_t sampleIndex = nextSampleDataIndex(profiler);

    if (sampleIndex >= kSampleDataSize) {
        VN_WARNING(profiler->log, "Saturated sample buffer. Index = %u, Count = %u\n", sampleIndex,
                   kSampleDataSize);
        profiler->flush = true;
        return &profiler->sampleDummy;
    }

    return &profiler->sampleData[sampleIndex];
}

static inline void pushProfileID(Logger_t log, ProfileID_t id)
{
    if (gCurrentThreadStackIndex >= 64) {
        VN_ERROR(log, "Profile stack error, either the stack is not deep enough, or you have a "
                      "profile start without a stop\n");
        gCurrentThreadStackIndex = 0;
    }

    gCurrentThreadStack[gCurrentThreadStackIndex] = id;
    gCurrentThreadStackIndex += 1;
}

static inline ProfileID_t popProfileID(Logger_t log)
{
    if (gCurrentThreadStackIndex == 0) {
        VN_ERROR(log, "Profile stack error, you are attempting to pop from the stack when it is "
                      "empty, missing profile start\n");
        return 0;
    }

    gCurrentThreadStackIndex -= 1;
    return gCurrentThreadStack[gCurrentThreadStackIndex];
}

#endif

/*------------------------------------------------------------------------------*/

int32_t profilerInitialise(ProfilerState_t** profilerOut, Memory_t memory, Logger_t log)
{
#if VN_CORE_FEATURE(PROFILER)
    VN_DEBUG(log, "Opening profiler\n");

    ProfilerState_t* profiler = VN_CALLOC_T(memory, ProfilerState_t);

    if (!profiler) {
        return -1;
    }

    profiler->threadID = 0;
    profiler->profileID = 0;
    profiler->profileNextIndex = 0;
    profiler->sampleNextIndex = 0;
    profiler->sampleData = VN_CALLOC_T_ARR(memory, SampleData_t, kSampleDataSize);
    profiler->profileData = VN_CALLOC_T_ARR(memory, ProfileData_t, kProfileDataSize);
    profiler->flush = false;
    profiler->lastFlush = false;
    profiler->flushed = false;
    profiler->flushCounter = 0;

    if (!profiler->sampleData || !profiler->profileData) {
        goto error_exit;
    }

    if (mutexInitialise(memory, &profiler->profileDataMutex) != 0) {
        goto error_exit;
    }

    memset(&profiler->sampleDummy, 0, sizeof(SampleData_t));
    memset(profiler->sampleData, 0, sizeof(SampleData_t) * kSampleDataSize);
    memset(profiler->profileData, 0, sizeof(ProfileData_t) * kProfileDataSize);

#if VN_OS(WINDOWS)
    fopen_s(&profiler->logFile, "profile.txt", "w");
#elif VN_OS(ANDROID)
    /* @todo: This path needs to be configurable */
    profiler->logFile = fopen("/sdcard/profile.txt", "w");
#else
    profiler->logFile = fopen("profile.txt", "w");
#endif

    if (profiler->logFile) {
        VN_DEBUG(log, "Successfully opened profiler log file\n");
    } else {
        VN_ERROR(log, "Failed to open profiler log file\n");
    }

#if VN_OS(WINDOWS)
    QueryPerformanceFrequency(&profiler->timeFrequency);
#endif

    profiler->log = log;
    profiler->memory = memory;
    *profilerOut = profiler;

    profilerRegisterThread(profiler, "main_thread");

    return 0;

error_exit:
    if (profiler) {
        VN_FREE(memory, profiler->sampleData);
        VN_FREE(memory, profiler->profileData);
        VN_FREE(memory, profiler);
    }

    return -1;

#else
    VN_UNUSED(profilerOut);
    VN_UNUSED(memory);
    VN_UNUSED(log);
    return 0;
#endif
}

void profilerRelease(ProfilerState_t** profilerOut, Memory_t memory)
{
#if VN_CORE_FEATURE(PROFILER)
    assert(profilerOut);

    ProfilerState_t* profiler = *profilerOut;
    if (!profiler) {
        return;
    }

    if (!profiler->flushed) {
        profilerFlush(profiler);
    }

    if (profiler->logFile) {
        fclose(profiler->logFile);
    }

    for (uint32_t i = 0; i < kProfileDataSize; ++i) {
        ProfileData_t* pd = &profiler->profileData[i];
        void* label = (void*)pd->label;
        VN_FREE(memory, label);
    }

    mutexRelease(profiler->profileDataMutex);

    VN_FREE(memory, profiler->sampleData);
    VN_FREE(memory, profiler->profileData);
    VN_FREE(memory, profiler);
    *profilerOut = NULL;
#else
    VN_UNUSED(profilerOut);
    VN_UNUSED(memory);
#endif
}

void profilerRegisterThread(ProfilerState_t* profiler, const char* label)
{
#if VN_CORE_FEATURE(PROFILER)
    if (!profiler) {
        return;
    }

    if (gCurrentThreadID != 0) {
        VN_WARNING(profiler->log, "Warning duplicate thread registration: %s\n", label);
    }

    gCurrentThreadID = nextThreadID(profiler);

    /* @todo: This is broken for thread-safety. */
    if (profiler->logFile) {
        fprintf(profiler->logFile, "T`%u`%s\n", gCurrentThreadID, label);
        fflush(profiler->logFile);
    }
#else
    VN_UNUSED(profiler);
    VN_UNUSED(label);
#endif
}

ProfileID_t profilerRegisterProfile(ProfilerState_t* profiler, const char* label, const char* file,
                                    uint32_t line)
{
#if VN_CORE_FEATURE(PROFILER)
    /* @todo: Can remove the atomic data index now that a mutex protects it. */
    mutexLock(profiler->profileDataMutex);

    const uint32_t profileIndex = nextProfileDataIndex(profiler);

    if (profileIndex > kProfileDataSize) {
        /* error case */
        mutexUnlock(profiler->profileDataMutex);
        return 0;
    }

    ProfileData_t* profileData = &profiler->profileData[profileIndex];
    profileData->id = nextProfileID(profiler);
    profileData->label = duplicateStr(profiler->memory, label);
    profileData->file = file;
    profileData->line = line;
    profileData->threadID = getCurrentThreadID(profiler);

    mutexUnlock(profiler->profileDataMutex);

    return profileData->id;
#else
    VN_UNUSED(profiler);
    VN_UNUSED(label);
    VN_UNUSED(file);
    VN_UNUSED(line);

    return 0;
#endif
}

ProfileID_t profilerRetrieveProfile(ProfilerState_t* profiler, const char* file, uint32_t line,
                                    const char* labelFmt, ...)
{
#if VN_CORE_FEATURE(PROFILER)

    /* Format label */
    va_list args;
    va_start(args, labelFmt);
    vsnprintf(gLabelFormatBuffer, 256, labelFmt, args);
    va_end(args);

    mutexLock(profiler->profileDataMutex);

    /* Walk list of retrievable profiles looking for this profile. */
    ProfileData_t* retrievePtr = profiler->retrieveHead;
    while (retrievePtr) {
        /* @todo: Speed this up with a string hash. */
        if (strcmp(retrievePtr->label, gLabelFormatBuffer) == 0) {
            mutexUnlock(profiler->profileDataMutex);
            return retrievePtr->id;
        }

        retrievePtr = retrievePtr->retrieveNext;
    }

    /* Failed to find profile, register new */
    const uint32_t profileIndex = nextProfileDataIndex(profiler);

    if (profileIndex > kProfileDataSize) {
        /* error case */
        mutexUnlock(profiler->profileDataMutex);
        return 0;
    }

    ProfileData_t* profile = &profiler->profileData[profileIndex];
    profile->id = nextProfileID(profiler);
    profile->label = duplicateStr(profiler->memory, gLabelFormatBuffer);
    profile->file = file;
    profile->line = line;
    profile->threadID = 0; /* Retrieved profiles cannot be unique across threads. */

    /* Stash profile in a retrievable list to help improve performance. */
    profile->retrieveNext = profiler->retrieveHead;
    profiler->retrieveHead = profile;

    mutexUnlock(profiler->profileDataMutex);

    return profile->id;
#else
    VN_UNUSED(profiler);
    VN_UNUSED(file);
    VN_UNUSED(line);
    VN_UNUSED(labelFmt);

    return 0;
#endif
}

void profilerTickStart(ProfilerState_t* profiler)
{
#if VN_CORE_FEATURE(PROFILER)
    if (!profiler) {
        return;
    }

    if (profiler->logFile) {
        fprintf(profiler->logFile, "TB`%" PRIu64 "\n", getHighFrequencyTime(profiler));
        fflush(profiler->logFile);
    }
#else
    VN_UNUSED(profiler);
#endif
}

void profilerTickStop(ProfilerState_t* profiler)
{
#if VN_CORE_FEATURE(PROFILER)
    if (!profiler) {
        return;
    }

    if (profiler->logFile) {
        fprintf(profiler->logFile, "TF`%" PRIu64 "\n", getHighFrequencyTime(profiler));
        fflush(profiler->logFile);
    }

#if VN_OS(ANDROID)

    if (profiler->flushCounter > 10) {
        profilerFlush(profiler);
        profiler->flushCounter = 0;
    }

    profiler->flushCounter += 1;

#else
    if (!profiler->flushed && (profiler->flush && !profiler->lastFlush)) {
        profilerFlush(profiler);
    }
#endif

    profiler->lastFlush = profiler->flush;
#else
    VN_UNUSED(profiler);
#endif
}

void profilerProfileStart(ProfilerState_t* profiler, ProfileID_t id)
{
#if VN_CORE_FEATURE(PROFILER)
    SampleData_t* sampleData = nextSampleData(profiler);
    sampleData->type = SDTBegin;
    sampleData->pid = id;
    sampleData->time = getHighFrequencyTime(profiler);
    pushProfileID(profiler->log, id);
#else
    VN_UNUSED(profiler);
    VN_UNUSED(id);
#endif
}

void profilerProfileStop(ProfilerState_t* profiler)
{
#if VN_CORE_FEATURE(PROFILER)
    SampleData_t* sampleData = nextSampleData(profiler);
    sampleData->type = SDTEnd;
    sampleData->pid = popProfileID(profiler->log);
    sampleData->time = getHighFrequencyTime(profiler);
#else
    VN_UNUSED(profiler);
#endif
}

void profilerFlush(ProfilerState_t* profiler)
{
#if VN_CORE_FEATURE(PROFILER)
    uint32_t profileDataSize = 0;
    uint32_t sampleDataSize = 0;
    uint64_t* dumpSampleTimes = NULL;
    uint32_t dumpSampleCount = 0;

    if (!profiler) {
        return;
    }

    if (kDumpSamplesLabel) {
        dumpSampleTimes = VN_CALLOC_T_ARR(profiler->memory, uint64_t, kDumpSampleCapacity);
    }

    VN_DEBUG(profiler->log, "Starting profiler flush\n");

    if (profiler->logFile) {
        for (uint32_t i = 0; i < kProfileDataSize; ++i) {
            const ProfileData_t* profileData = &profiler->profileData[i];

            if (profileData->id == 0) {
                break;
            }

            fprintf(profiler->logFile, "P`%u`%u`%s`%s`%u\n", profileData->id, profileData->threadID,
                    profileData->label, profileData->file, profileData->line);
            fflush(profiler->logFile);

            profileDataSize += 1;
        }

        for (uint32_t i = 0; i < kSampleDataSize; ++i) {
            const SampleData_t* sampleData = &profiler->sampleData[i];

            if (sampleData->pid == 0) {
                break;
            }

            switch (sampleData->type) {
                case SDTBegin:
                    fprintf(profiler->logFile, "B`%u`%" PRIu64 "\n", sampleData->pid, sampleData->time);
                    break;
                case SDTEnd:
                    fprintf(profiler->logFile, "F`%u`%" PRIu64 "\n", sampleData->pid, sampleData->time);
                    break;
                default: break;
            }
            sampleDataSize += 1;
        }

        fflush(profiler->logFile);
    } else {
        for (uint32_t i = 0; i < kProfileDataSize; ++i) {
            const ProfileData_t* profileData = &profiler->profileData[i];

            if (profileData->id == 0) {
                break;
            }

            profileDataSize += 1;
        }

        for (uint32_t i = 0; i < kSampleDataSize; ++i) {
            const SampleData_t* sampleData = &profiler->sampleData[i];

            if (sampleData->pid == 0) {
                break;
            }

            sampleDataSize += 1;
        }
    }

    /* Process profiles to log out */
    ProfileStats_t* stats = VN_CALLOC_T_ARR(profiler->memory, ProfileStats_t, profileDataSize);

    /* Note that each thread has a unique entry per profile point. So no need to
     * worry about 2 profile points working on different threads at the same time.
     * These are merged later on. */
    for (uint32_t i = 0; i < sampleDataSize; ++i) {
        const SampleData_t* sample = &profiler->sampleData[i];
        const ProfileID_t pid = sample->pid;
        ProfileData_t* profile = NULL;
        uint32_t profile_idx = 0;

        for (uint32_t j = 0; j < profileDataSize; ++j) {
            if (profiler->profileData[j].id == pid) {
                profile = &profiler->profileData[j];
                profile_idx = j;
                break;
            }
        }

        if (profile) {
            ProfileStats_t* profile_stat = &stats[profile_idx];

            /* Stash profile on the stat */
            if (profile_stat->profile == NULL) {
                profile_stat->profile = profile;
                profile_stat->minTime = 0xFFFFFFFFFFFFFFFF;
            }

            if (sample->type == SDTBegin) {
                /* This assumes no sample corruption. */
                profile_stat->sampleStart = sample->time;
            } else {
                uint64_t sample_time = sample->time - profile_stat->sampleStart;

                profile_stat->minTime = minU64(sample_time, profile_stat->minTime);
                profile_stat->maxTime = maxU64(sample_time, profile_stat->maxTime);
                profile_stat->accumTime += sample_time;
                profile_stat->count++;

                /* Accumulate dump samples data */
                if (dumpSampleTimes && dumpSampleCount < kDumpSampleCapacity &&
                    (strcmp(profile->label, kDumpSamplesLabel) == 0)) {
                    dumpSampleTimes[dumpSampleCount++] = sample_time;
                }
            }
        }
    }

    /* Merge cross-thread profiles */
    for (uint32_t i = 0; i < profileDataSize; ++i) {
        ProfileStats_t* srcStats = &stats[i];
        const ProfileData_t* srcProfile = srcStats->profile;

        if (!srcProfile) {
            continue;
        }

        for (uint32_t j = i + 1; j < profileDataSize; ++j) {
            ProfileStats_t* chkStats = &stats[j];
            const ProfileData_t* chkProfile = chkStats->profile;

            if (!chkProfile) {
                continue;
            }

            /* Does this chkProfile overlap srcProfile. Merge check is based upon:
             * label, file and line. Technically file and line could work, but multiple
             * profile samples in a macro may fail this check erroneously. */
            if ((srcProfile->line == chkProfile->line) &&
                (strcmp(srcProfile->label, chkProfile->label) == 0) &&
                (strcmp(srcProfile->file, chkProfile->file) == 0)) {
                srcStats->minTime = minU64(srcStats->minTime, chkStats->minTime);
                srcStats->maxTime = maxU64(srcStats->maxTime, chkStats->maxTime);
                srcStats->accumTime += chkStats->accumTime;
                srcStats->count += chkStats->count;

                /* Clear profile to prevent these stats being worked on any further. */
                chkStats->profile = NULL;
            }
        }
    }

    /* Log amalgamated stats
     * @todo: Track inclusive and exclusive sample time and log a hierarchical representation. */
    VN_DEBUG(profiler->log, "Profiler Stats\n");

    for (uint32_t i = 0; i < profileDataSize; ++i) {
        const ProfileStats_t* profileStat = &stats[i];
        const ProfileData_t* profile = stats[i].profile;

        if (profile) {
            const double minTimeMS = profileStat->minTime / 1000000.0;
            const double maxTimeMS = profileStat->maxTime / 1000000.0;
            const double avgTimeMS = profileStat->accumTime / (profileStat->count * 1000000.0);

            VN_DEBUG(profiler->log, "  %-40s - min: %fms, max: %fms, avg: %fms, count: %" PRIu64 "\n",
                     profile->label, minTimeMS, maxTimeMS, avgTimeMS, profileStat->count);
        }
    }

    /* Log dump samples */
    if (dumpSampleTimes) {
        VN_DEBUG(profiler->log, "Profiler Dump Sample Data: %s\n", kDumpSamplesLabel);

        if (dumpSampleCount) {
            /* @todo: This may interfer on platforms that interleave logging with other
             *        applications */
            VN_DEBUG(profiler->log, "%f", dumpSampleTimes[0] / 1000000.0);
            for (uint32_t i = 1; i < dumpSampleCount; ++i) {
                VN_DEBUG(profiler->log, ", %f", dumpSampleTimes[i] / 1000000.0);
            }
            VN_DEBUG(profiler->log, "\n");
        } else {
            VN_DEBUG(profiler->log, "  No sample data\n");
        }
    }

    /* Clean up after ourselves. */
    VN_FREE(profiler->memory, stats);
    VN_FREE(profiler->memory, dumpSampleTimes);

    profiler->profileNextIndex = 0;
    profiler->sampleNextIndex = 0;
    profiler->flushed = true;

    VN_DEBUG(profiler->log, "Finished flushing profiler - Profiles: %u, Samples: %u\n",
             profileDataSize, sampleDataSize);
#else
    VN_UNUSED(profiler);
#endif
}
