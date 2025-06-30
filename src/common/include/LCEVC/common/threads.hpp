/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova International Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_LCEVC_COMMON_THREADS_HPP
#define VN_LCEVC_COMMON_THREADS_HPP

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/threads.h>
#include <LCEVC/common/memory.h>

namespace lcevc_dec::common {

// Wrapper for LdcThread
//
class Thread {
public:
    Thread() {
    }

    Thread(::ThreadFunction function, void* argument) {
        launch(function, argument);
    }

    ~Thread() {
        ::threadJoin(&m_thread, NULL);
    }

    void launch(::ThreadFunction function, void* argument) {
        ::threadCreate(&m_thread, function, argument);
    }

    VNNoCopyNoMove(Thread);
private:
    ::Thread m_thread;
};

// Wrapper for LdcThreadMutex
//
class Mutex {
public:
    Mutex(){
        threadMutexInitialize(&m_threadMutex);
    }
    ~Mutex() {
        threadMutexDestroy(&m_threadMutex);
    }

    VNNoCopyNoMove(Mutex);
private:
    friend class ScopedLock;

    ::ThreadMutex m_threadMutex;
};

// Scoped lock for LdcThreadMutex
//
class ScopedLock {
public:
    ScopedLock(Mutex & mutex) : m_threadMutex(mutex.m_threadMutex) {
        threadMutexLock(&m_threadMutex);
    }
    ScopedLock(::ThreadMutex& threadMutex) : m_threadMutex(threadMutex) {
        threadMutexLock(&m_threadMutex);
    }
    ~ScopedLock() {
        threadMutexUnlock(&m_threadMutex);
    }

    VNNoCopyNoMove(ScopedLock);

private:
    friend class CondVar;
    ::ThreadMutex& m_threadMutex;
};

// Wrapper for LdcThreadCondVar
//
class CondVar {
public:
    CondVar(){
        threadCondVarInitialize(&m_condVar);
    }
    ~CondVar() {
        threadCondVarDestroy(&m_condVar);
    }

    void signal() {
        threadCondVarSignal(&m_condVar);
    }

    void broadcast() {
        threadCondVarBroadcast(&m_condVar);
    }

    void wait(ScopedLock &lock) {
        threadCondVarWait(&m_condVar, &lock.m_threadMutex);
    }

    bool waitDeadline(ScopedLock &lock, uint64_t deadline) {
        return threadCondVarWaitDeadline(&m_condVar, &lock.m_threadMutex, deadline) == 0;
    }

    void wait(::ThreadMutex &mutex) {
        threadCondVarWait(&m_condVar, &mutex);
    }
    bool waitDeadline(::ThreadMutex &mutex, uint64_t deadline) {
        return threadCondVarWaitDeadline(&m_condVar, &mutex, deadline) == 0;
    }

    VNNoCopyNoMove(CondVar);
private:
    ::ThreadCondVar m_condVar;
};

} // namespace lcevc_dec::common

#endif // VN_LCEVC_COMMON_THREADS_H
