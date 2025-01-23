/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

#include "common/threading.h"

#include "common/log.h"
#include "common/memory.h"
#include "lcevc_config.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>

/*------------------------------------------------------------------------------*/

#if VN_CORE_FEATURE(PTHREADS)

#if VN_CORE_FEATURE(EMSCRIPTEN_PTHREADS)
#include <emscripten/threading.h>
#endif /* VN_CORE_FEATURE(EMSCRIPTEN) */

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define VN_THREADLOOP_SIGNATURE() static void* threadLoop(void* data)

typedef struct ThreadPlatform
{
    pthread_t thd;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} ThreadPlatform_t;

static void platformMutexLock(ThreadPlatform_t* platform) { pthread_mutex_lock(&platform->mtx); }

static void platformMutexUnlock(ThreadPlatform_t* platform)
{
    pthread_mutex_unlock(&platform->mtx);
}

static void platformMutexCVNotify(ThreadPlatform_t* platform)
{
    pthread_cond_signal(&platform->cv);
}

static void platformMutexCVWait(ThreadPlatform_t* platform)
{
    pthread_cond_wait(&platform->cv, &platform->mtx);
}

static void platformMutexCVWaitTimed(ThreadPlatform_t* platform, uint32_t milliseconds)
{
    struct timespec ts;
    uint32_t overflow;

    /* Grab current time.*/
    clock_gettime(CLOCK_REALTIME, &ts);

    /* Add on how many milliseconds we wish to wait for. */
    ts.tv_nsec = ts.tv_nsec + (milliseconds * 1000000);

    /* Handle overflow to bring tv_nsec into [0, 999999999] range. */
    overflow = (uint32_t)(ts.tv_nsec / 1000000000);
    ts.tv_nsec = ts.tv_nsec - (overflow * 1000000000);
    ts.tv_sec = ts.tv_sec + overflow;

    pthread_cond_timedwait(&platform->cv, &platform->mtx, &ts);
}

#elif VN_CORE_FEATURE(WINTHREADS)

#include <stdbool.h>
#include <Windows.h>

#define VN_THREADLOOP_SIGNATURE() static DWORD WINAPI threadLoop(LPVOID data)

typedef struct ThreadPlatform
{
    HANDLE thd;
    CRITICAL_SECTION mtx;
    CONDITION_VARIABLE cv;
} ThreadPlatform_t;

static inline void platformMutexLock(ThreadPlatform_t* platform)
{
    EnterCriticalSection(&platform->mtx);
}

static inline void platformMutexUnlock(ThreadPlatform_t* platform)
{
    LeaveCriticalSection(&platform->mtx);
}

static inline void platformMutexCVNotify(ThreadPlatform_t* platform)
{
    WakeConditionVariable(&platform->cv);
}

static inline void platformMutexCVWait(ThreadPlatform_t* platform)
{
    SleepConditionVariableCS(&platform->cv, &platform->mtx, INFINITE);
}

static inline void platformMutexCVWaitTimed(ThreadPlatform_t* platform, uint32_t milliseconds)
{
    SleepConditionVariableCS(&platform->cv, &platform->mtx, milliseconds);
}

#endif

#if VN_CORE_FEATURE(THREADING)

/*------------------------------------------------------------------------------*/

typedef enum ThreadJobType
{
    TJT_Simple = 0,
    TJT_Sliced,
} ThreadJobType_t;

typedef struct ThreadJobSimple
{
    JobFunction_t function;
    void* data;
} ThreadJobSimple_t;

typedef struct ThreadJobSliced
{
    SlicedJobFunction_t function;
    const void* executeContext;
    JobIndex_t index;
    SliceOffset_t offset;
} ThreadJobSliced_t;

typedef struct ThreadJob
{
    union JobData
    {
        ThreadJobSimple_t simple;
        ThreadJobSliced_t sliced;
    } data;

    ThreadJobType_t type;
} ThreadJob_t;

typedef struct Thread
{
    bool busy;
    bool wait;
    bool dead;
    int32_t retval;
    int32_t idx;
    ThreadPlatform_t platform;
    ThreadJob_t job;
} Thread_t;

static int32_t executeThreadJob(ThreadJob_t* job)
{
    if (job->type == TJT_Simple) {
        ThreadJobSimple_t* simple = &job->data.simple;

        if (simple->function) {
            return simple->function(simple->data);
        }
        return 0;
    }

    if (job->type == TJT_Sliced) {
        const ThreadJobSliced_t* sliced = &job->data.sliced;

        if (sliced->function) {
            return sliced->function(sliced->executeContext, sliced->index, sliced->offset);
        }
        return 0;
    }

    return -1;
}

VN_THREADLOOP_SIGNATURE()
{
    Thread_t* thread = (Thread_t*)data;
    ThreadPlatform_t* platform = &thread->platform;

    while (!thread->dead) {
        platformMutexLock(platform);

        while (!thread->busy && !thread->dead) {
            platformMutexCVWaitTimed(platform, 500);
        }

        platformMutexUnlock(platform);

        if (!thread->dead) {
            thread->retval = executeThreadJob(&thread->job);

            platformMutexLock(platform);

            thread->busy = 0;

            if (thread->wait) {
                thread->wait = 0;
                platformMutexCVNotify(platform);
            }

            platformMutexUnlock(platform);
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

#if VN_CORE_FEATURE(PTHREADS)

static void threadLaunch(Thread_t* thrd)
{
    ThreadPlatform_t* platform = &thrd->platform;
    pthread_mutex_init(&platform->mtx, NULL);
    pthread_cond_init(&platform->cv, NULL);
    pthread_create(&platform->thd, NULL, threadLoop, thrd);
}

static void threadStop(Thread_t* thrd)
{
    ThreadPlatform_t* plat = &thrd->platform;
    pthread_join(plat->thd, NULL);
    pthread_cond_destroy(&plat->cv);
    pthread_mutex_destroy(&plat->mtx);
}

#elif VN_CORE_FEATURE(WINTHREADS)

static inline void threadLaunch(Thread_t* thrd)
{
    ThreadPlatform_t* platform = &thrd->platform;
    InitializeCriticalSection(&platform->mtx);
    InitializeConditionVariable(&platform->cv);
    platform->thd = CreateThread(NULL, 0, threadLoop, thrd, 0, NULL);
}

static inline void threadStop(Thread_t* thrd)
{
    ThreadPlatform_t* plat = &thrd->platform;
    WaitForSingleObject(plat->thd, INFINITE);
    CloseHandle(plat->thd);
    DeleteCriticalSection(&plat->mtx);
}

#endif

/*------------------------------------------------------------------------------*/

int32_t threadingInitialise(Memory_t memory, Logger_t log, ThreadManager_t* mgr, uint32_t numThreads)
{
    if (mgr == NULL) {
        return -1;
    }

    if (numThreads > 0) {
        mgr->threads = VN_CALLOC_T_ARR(memory, Thread_t, numThreads);

        if (mgr->threads == NULL) {
            VN_ERROR(log, "Failed to allocate memory for thread data\n");
            return -1;
        }
    }

    mgr->memory = memory;
    mgr->log = log;
    mgr->numThreads = numThreads;

    for (int32_t i = 0; i < (int32_t)numThreads; i++) {
        Thread_t* thrd = &mgr->threads[i];
        thrd->idx = i;
        threadLaunch(thrd);
    }

    return 0;
}

void threadingRelease(ThreadManager_t* mgr)
{
    for (uint32_t i = 0; i < mgr->numThreads; i++) {
        Thread_t* thread = &mgr->threads[i];
        ThreadPlatform_t* plat = &thread->platform;

        platformMutexLock(plat);
        thread->dead = true;
        platformMutexCVNotify(plat);
        platformMutexUnlock(plat);
    }

    for (uint32_t i = 0; i < mgr->numThreads; i++) {
        Thread_t* thrd = &mgr->threads[i];
        threadStop(thrd);
    }

    if (mgr->numThreads > 0) {
        VN_FREE(mgr->memory, mgr->threads);
        mgr->numThreads = 0;
    }
}

static Thread_t* threadSubmitLock(ThreadManager_t* mgr, uint32_t threadIndex)
{
    if (threadIndex >= mgr->numThreads) {
        return NULL;
    }

    Thread_t* thread = &mgr->threads[threadIndex];
    ThreadPlatform_t* plat = &thread->platform;

    platformMutexLock(plat);

    /* Thread is already busy with work on it, this indicates user error. */
    if (thread->busy == true) {
        platformMutexUnlock(plat);
        return NULL;
    }

    thread->busy = true;

    return thread;
}

static void threadSubmitUnlock(Thread_t* thread)
{
    ThreadPlatform_t* plat = &thread->platform;
    platformMutexCVNotify(plat);
    platformMutexUnlock(plat);
}

static bool threadingSubmitJob(ThreadManager_t* mgr, uint32_t threadIndex, JobFunction_t function, void* data)
{
    Thread_t* thread = threadSubmitLock(mgr, threadIndex);

    if (!thread) {
        VN_ERROR(mgr->log, "Failed to retrieve and lock thread for index: %u\n", threadIndex);
        return false;
    }

    ThreadJobSimple_t* simple = &thread->job.data.simple;
    simple->function = function;
    simple->data = data;
    thread->job.type = TJT_Simple;

    threadSubmitUnlock(thread);
    return true;
}

static bool threadingSubmitSlicedJob(ThreadManager_t* mgr, uint32_t threadIndex,
                                     SlicedJobFunction_t function, const void* executeContext,
                                     JobIndex_t index, SliceOffset_t offset)
{
    Thread_t* thread = threadSubmitLock(mgr, threadIndex);

    if (!thread) {
        VN_ERROR(mgr->log, "Failed to retrieve and lock thread for index: %u\n", threadIndex);
        return false;
    }

    ThreadJobSliced_t* sliced = &thread->job.data.sliced;
    sliced->function = function;
    sliced->executeContext = executeContext;
    sliced->index = index;
    sliced->offset = offset;
    thread->job.type = TJT_Sliced;

    threadSubmitUnlock(thread);
    return true;
}

static bool threadingWaitJob(ThreadManager_t* mgr, uint32_t threadIndex)
{
    if (threadIndex >= mgr->numThreads) {
        return -1;
    }

    Thread_t* thread = &mgr->threads[threadIndex];
    ThreadPlatform_t* plat = &thread->platform;

    platformMutexLock(plat);

    /* Loop until thread is no longer busy processing work. */
    while (thread->busy == 1) {
        thread->wait = 1;
        platformMutexCVWait(plat);
    }

    const bool res = (thread->retval == 0) ? true : false;
    platformMutexUnlock(plat);

    return res;
}

int32_t threadingGetNumCores(void)
{
#if VN_OS(BROWSER)
    if (!emscripten_has_threading_support()) {
        return 1;
    }

    return emscripten_num_logical_cores();

#elif VN_OS(LINUX) || VN_OS(IOS) || VN_OS(TVOS)

    return (int32_t)sysconf(_SC_NPROCESSORS_CONF);

#elif VN_OS(WINDOWS)

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int32_t)sysinfo.dwNumberOfProcessors;

#else

    return 1;

#endif
}

uint32_t threadingGetNumThreads(ThreadManager_t* mgr) { return mgr ? mgr->numThreads : 1; }

#else

int32_t threadingInitialise(Memory_t memory, Logger_t log, ThreadManager_t* mgr, uint32_t numThreads)
{
    return 0;
}

void threadingRelease(ThreadManager_t* mgr) {}

bool threadingExecuteJobs(ThreadManager_t* mgr, int32_t (*function)(void*), void* jobs,
                          uint32_t jobCount, uint32_t jobByteSize)
{
    uint8_t* jobData = (uint8_t*)jobs;
    uint32_t jobOffset = 0;

    bool res = true;

    for (uint32_t i = 0; i < jobCount; ++i) {
        res &= function((void*)(jobData + jobOffset));
        jobOffset += jobByteSize;
    }

    return res;
}

int32_t threadingGetNumCores() { return 1; }

uint32_t threadingGetNumThreads(ThreadManager_t* mgr) { return 1; }

#endif

bool threadingExecuteJobs(ThreadManager_t* mgr, JobFunction_t function, void* jobs,
                          uint32_t jobCount, uint32_t jobByteSize)
{
    if (jobCount == 1) {
        return (function(jobs) == 0);
    }

    uint32_t threadIdx = 0;
    uint8_t* jobData = (uint8_t*)jobs;
    uint32_t jobOffset = 0;
    bool res = true;

    /* Keep running whilst there are jobs to do. */
    while (jobCount > 0) {
        threadIdx = 0;

        /* Schedule up to thread-counts worth of work. */
        for (; (jobCount > 0) && (threadIdx < mgr->numThreads); threadIdx += 1) {
            threadingSubmitJob(mgr, threadIdx, function, (void*)(jobData + jobOffset));
            jobOffset += jobByteSize;
            jobCount -= 1;
        }

        /* Run a single remainder on calling thread. */
        if (jobCount) {
            res &= (function((void*)(jobData + jobOffset)) == 0);
            jobOffset += jobByteSize;
            jobCount -= 1;
        }

        /* Wait for busy threads to finish. */
        for (uint32_t i = 0; i < threadIdx; i += 1) {
            res &= threadingWaitJob(mgr, i);
        }
    }

    return res;
}

bool threadingExecuteSlicedJobs(ThreadManager_t* mgr, SlicedJobFunction_t function,
                                const void* executeContext, size_t totalSize)
{
    return threadingExecuteSlicedJobsWithPostRun(mgr, function, NULL, executeContext, totalSize);
}

bool threadingExecuteSlicedJobsWithPostRun(ThreadManager_t* mgr, SlicedJobFunction_t function,
                                           SlicedJobFunction_t postRunFunction,
                                           const void* executeContext, size_t totalSize)
{
    if (!mgr) {
        return false;
    }
    const size_t totalJobCount = threadingGetNumThreads(mgr);
    assert(totalJobCount > 0);
    const size_t perSliceCount = totalSize / totalJobCount;

    JobIndex_t index = {.current = 0, .last = totalJobCount - 1};
    SliceOffset_t offset = {.offset = 0, .count = perSliceCount};
    uint32_t threadIdx = 0;
    bool res = true;

    /* Schedule up to thread-counts worth of work. */
    size_t jobCount = totalJobCount;
    for (; (jobCount > 1) && (threadIdx < mgr->numThreads); threadIdx += 1) {
        threadingSubmitSlicedJob(mgr, threadIdx, function, executeContext, index, offset);

        index.current += 1;
        offset.offset += perSliceCount;
        jobCount -= 1;
    }

    /* Run a single remainder on calling thread. */
    if (jobCount) {
        offset.count = totalSize - offset.offset;
        res &= (function(executeContext, index, offset) == 0);
    }

    /* Wait for busy threads to finish. */
    for (uint32_t i = 0; i < threadIdx; i += 1) {
        res &= threadingWaitJob(mgr, i);
    }

    /* Post run function call */
    if (postRunFunction) {
        index.current = 0;
        offset.offset = 0;

        for (size_t i = 0; i < totalJobCount; ++i) {
            offset.count = (i == (totalJobCount - 1)) ? (totalSize - offset.offset) : perSliceCount;

            res &= (postRunFunction(executeContext, index, offset) == 0);
            offset.offset += perSliceCount;
            index.current += 1;
        }
    }

    return res;
}

/*------------------------------------------------------------------------------*/

#if VN_CORE_FEATURE(PTHREADS)

typedef struct Mutex
{
    Memory_t memory;
    pthread_mutex_t mtx;
} Mutex_t;

int32_t mutexInitialise(Memory_t memory, Mutex_t** mutex)
{
    if (!mutex) {
        return -1;
    }

    Mutex_t* res = VN_MALLOC_T(memory, Mutex_t);
    res->memory = memory;
    pthread_mutex_init(&res->mtx, NULL);
    *mutex = res;
    return 0;
}

void mutexRelease(Mutex_t* mutex)
{
    if (mutex) {
        pthread_mutex_destroy(&mutex->mtx);
        VN_FREE(mutex->memory, mutex);
    }
}

void mutexLock(Mutex_t* mutex) { pthread_mutex_lock(&mutex->mtx); }

void mutexUnlock(Mutex_t* mutex) { pthread_mutex_unlock(&mutex->mtx); }

#elif VN_CORE_FEATURE(WINTHREADS)

typedef struct Mutex
{
    Memory_t memory;
    CRITICAL_SECTION mtx;
} Mutex_t;

int32_t mutexInitialise(Memory_t memory, Mutex_t** mutex)
{
    if (!mutex) {
        return -1;
    }

    Mutex_t* res = VN_MALLOC_T(memory, Mutex_t);
    res->memory = memory;
    InitializeCriticalSection(&res->mtx);
    *mutex = res;
    return 0;
}

void mutexRelease(Mutex_t* mutex)
{
    if (mutex) {
        DeleteCriticalSection(&mutex->mtx);
        VN_FREE(mutex->memory, mutex);
    }
}

void mutexLock(Mutex_t* mutex)
{
    if (mutex) {
        EnterCriticalSection(&mutex->mtx);
    }
}

void mutexUnlock(Mutex_t* mutex)
{
    if (mutex) {
        LeaveCriticalSection(&mutex->mtx);
    }
}

#else

int32_t mutexInitialise(Memory_t memory, Mutex_t** mutex)
{
    VN_UNUSED(memory);
    *mutex = NULL;
    return 0;
}

void mutexRelease(Mutex_t* mutex) { VN_UNUSED(mutex); }

void mutexLock(Mutex_t* mutex) { VN_UNUSED(mutex); }

void mutexUnlock(Mutex_t* mutex) { VN_UNUSED(mutex); }

#endif

/*------------------------------------------------------------------------------*/
