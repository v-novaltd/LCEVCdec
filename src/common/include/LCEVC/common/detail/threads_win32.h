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
 * Win32 definitions for common threading utilities.
 *
 * Uses SharedReadWriteLocks which appear to give the best performance - with the notable
 * exception of WakeAllConditionVariable() on an SRWLOCK which is extraordinarily slow.
 */
#ifndef VN_LCEVC_COMMON_DETAIL_THREADS_WIN32_H
#define VN_LCEVC_COMMON_DETAIL_THREADS_WIN32_H
#include <LCEVC/common/platform.h>

enum ThreadResult
{
    ThreadResultSuccess = 0,
    ThreadResultError = EINVAL,
    ThreadResultTimeout = ETIMEDOUT,
    ThreadResultAgain = EAGAIN,
};

// Win32 Threads
//
struct Thread
{
    HANDLE thread;
    ThreadFunction function;
    void* argument;
    intptr_t result;
};

// Win32 Mutexes
//
struct ThreadMutex
{
    SRWLOCK lock;
};

static inline void threadYield(void) { SwitchToThread(); }

static inline int threadMutexInitialize(ThreadMutex* mutex)
{
    InitializeSRWLock(&mutex->lock);
    return ThreadResultSuccess;
}

static inline int threadMutexDestroy(ThreadMutex* mutex)
{
    VNUnused(mutex);
    // SRWLOCK do not need destroying
    return ThreadResultSuccess;
}

static inline int threadMutexLock(ThreadMutex* mutex)
{
    AcquireSRWLockExclusive(&mutex->lock);
    return ThreadResultSuccess;
}

static inline int threadMutexTrylock(ThreadMutex* mutex)
{
    TryAcquireSRWLockExclusive(&mutex->lock);
    return ThreadResultSuccess;
}

static inline int threadMutexUnlock(ThreadMutex* mutex)
{
    ReleaseSRWLockExclusive(&mutex->lock);
    return ThreadResultSuccess;
}

// Win32 Conditional variables
//
struct ThreadCondVar
{
    CONDITION_VARIABLE condVar;
};

static inline int threadCondVarInitialize(ThreadCondVar* condVar)
{
    InitializeConditionVariable(&condVar->condVar);
    return ThreadResultSuccess;
}

static inline void threadCondVarDestroy(ThreadCondVar* condVar)
{
    VNUnused(condVar);
    // CONDITION_VARIABLEs do not need destroying
}

static inline int threadCondVarSignal(ThreadCondVar* condVar)
{
    WakeConditionVariable(&condVar->condVar);
    return ThreadResultSuccess;
}

static inline int threadCondVarBroadcast(ThreadCondVar* condVar)
{
    WakeAllConditionVariable(&condVar->condVar);
    return ThreadResultSuccess;
}

static inline int threadCondVarWait(ThreadCondVar* condVar, ThreadMutex* mutex)
{
    SleepConditionVariableSRW(&condVar->condVar, &mutex->lock, INFINITE, 0);
    return ThreadResultSuccess;
}

#endif // VN_LCEVC_COMMON_DETAIL_THREADS_WIN32_H
