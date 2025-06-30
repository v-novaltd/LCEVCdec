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

/*
 * Pthread definitions for common threading utilities.
 *
 * Covers linux, android, macOS and emscripten.
 */
#ifndef VN_LCEVC_COMMON_DETAIL_THREADS_PTHREAD_H
#define VN_LCEVC_COMMON_DETAIL_THREADS_PTHREAD_H

#include <errno.h>

enum ThreadResult
{
    ThreadResultSuccess = 0,
    ThreadResultError = EINVAL,
    ThreadResultTimeout = ETIMEDOUT,
    ThreadResultAgain = EAGAIN,
};

// Pthread Threads
//
struct Thread
{
    pthread_t thread;
    ThreadFunction function;
    void* argument;
    intptr_t result;
};

static inline void threadYield(void) { sched_yield(); }

// Pthread Mutexes
//
struct ThreadMutex
{
    pthread_mutex_t mutex;
};

static inline int threadMutexInitialize(ThreadMutex* mutex)
{
    return pthread_mutex_init(&mutex->mutex, NULL);
}

static inline int threadMutexDestroy(ThreadMutex* mutex)
{
    return pthread_mutex_destroy(&mutex->mutex);
}

static inline int threadMutexLock(ThreadMutex* mutex) { return pthread_mutex_lock(&mutex->mutex); }

static inline int threadMutexTrylock(ThreadMutex* mutex)
{
    return pthread_mutex_trylock(&mutex->mutex);
}

static inline int threadMutexUnlock(ThreadMutex* mutex)
{
    return pthread_mutex_unlock(&mutex->mutex);
}

// Pthread Conditional Variables
//
struct ThreadCondVar
{
    pthread_cond_t condVar;
};

static inline int threadCondVarInitialize(ThreadCondVar* condVar)
{
    const int ret = pthread_cond_init(&condVar->condVar, NULL);
    return ret;
}

static inline void threadCondVarDestroy(ThreadCondVar* condVar)
{
    pthread_cond_destroy(&condVar->condVar);
}

static inline int threadCondVarSignal(ThreadCondVar* condVar)
{
    return pthread_cond_signal(&condVar->condVar);
}

static inline int threadCondVarBroadcast(ThreadCondVar* condVar)
{
    return pthread_cond_broadcast(&condVar->condVar);
}

static inline int threadCondVarWait(ThreadCondVar* condVar, ThreadMutex* mutex)
{
    return pthread_cond_wait(&condVar->condVar, &mutex->mutex);
}

#endif // VN_LCEVC_COMMON_DETAIL_THREADS_PTHREAD_H
