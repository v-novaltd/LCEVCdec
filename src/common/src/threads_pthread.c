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

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/threads.h>
//
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

// Threads
//

int32_t threadNumCores(void)
{
#if VN_OS(LINUX) || VN_OS(APPLE) || VN_OS(ANDROID)
    return (int32_t)sysconf(_SC_NPROCESSORS_CONF);
#elif VN_OS(BROWSER)
    if (emscripten_has_threading_support()) {
        return emscripten_num_logical_cores();
    }
    return 1;
#else
    return 1;
#endif
}

static void* threadWrapper(void* arg)
{
    Thread* thread = (Thread*)arg;
    thread->result = 0;
    thread->result = thread->function(thread->argument);
    return (void*)0;
};

int threadCreate(Thread* thread, ThreadFunction function, void* argument)
{
    thread->function = function;
    thread->argument = argument;
    return pthread_create(&thread->thread, NULL, threadWrapper, thread);
}

void threadExit(void) { pthread_exit(NULL); }

int threadJoin(Thread* thread, intptr_t* resultPtr)
{
    int r = pthread_join(thread->thread, NULL);
    if (resultPtr) {
        *resultPtr = thread->result;
    }
    return r;
}

int threadSleep(uint32_t milliseconds)
{
    uint32_t secs = milliseconds / 1000;
    uint32_t msecs = milliseconds % 1000;
    const struct timespec requestedTime = {secs, msecs * 1000000L};
    struct timespec remaining = {0};
    int r = nanosleep(&requestedTime, &remaining);
    return r;
}

void threadSetPriority(Thread* thread, ThreadPriority priority)
{
    struct sched_param prio = {0};
    int policy = 0;

    switch (priority) {
        case ThreadPriorityHigh:
            policy = SCHED_RR;
            prio.sched_priority = 1;
            break;
        case ThreadPriorityNormal:
            policy = SCHED_OTHER;
            prio.sched_priority = 0;
            break;
        case ThreadPriorityIdle:
            policy = 5 /*SCHED_IDLE*/;
            prio.sched_priority = 0;
            break;
    }

    pthread_setschedparam(thread->thread, policy, &prio);
}

#if VN_OS(LINUX) || VN_OS(ANDROID)
extern int pthread_setname_np(pthread_t target_thread, const char* name);
#endif

void threadSetName(const char* name)
{
    if (!name) {
        return;
    }

#if VN_OS(LINUX) || VN_OS(ANDROID)
    extern int pthread_setname_np(pthread_t target_thread, const char* name);
    const int res = pthread_setname_np(pthread_self(), name);
#elif VN_OS(BROWSER)
    emscripten_set_thread_name(pthread_self(), name);
    const int res = 0;
#elif VN_OS(APPLE)
    const int res = pthread_setname_np(name);
#else
    const int res = -1;
#endif
    if (res != 0) {
        VNLogWarning("Cannot set thread name");
    }
}

uint64_t threadTimeMicroseconds(int32_t offset)
{
    if (offset == UINT32_MAX) {
        return UINT64_MAX;
    }

    struct timespec ts;

    // Get current time in usec since epoch
    // NB: Use CLCK_REALTIME - as pthread_cond_timed_wait() uses that
    //
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t uSecs = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000L;

    return uSecs + offset;
}

int threadCondVarWaitDeadline(ThreadCondVar* condVar, ThreadMutex* mutex, uint64_t deadline)
{
    if (deadline == UINT64_MAX) {
        return pthread_cond_wait(&condVar->condVar, &mutex->mutex);
    }

    // Convert microseconds to timespec
    struct timespec ts;
    ts.tv_sec = (long)(deadline / 1000000L);
    ts.tv_nsec = (long)(deadline - ts.tv_sec * 1000000L) * 1000;

    return pthread_cond_timedwait(&condVar->condVar, &mutex->mutex, &ts);
}
