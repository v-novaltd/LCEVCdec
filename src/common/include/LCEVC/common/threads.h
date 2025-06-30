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

#ifndef VN_LCEVC_COMMON_THREADS_H
#define VN_LCEVC_COMMON_THREADS_H

#include <LCEVC/common/platform.h>
#include <stdbool.h>
#include <stdint.h>

/*! @file
 * @brief Threads and inter-thread communications.
 *
 * A subset of the C11 concurrency support library - such that it is a clean implementation for
 * pthreads and win32, and is 'enough' for the decoder threading requirements.
 *
 * A custom implementation can be provided by defining VN_SDK_THREADS_CUSTOM, and proving
 * `threads_custom.h` in the include path.
 */

#ifdef __cplusplus
extern "C"
{
#endif

/*! Possible thread priorities
 */
typedef enum ThreadPriority
{
    ThreadPriorityIdle = 1,
    ThreadPriorityNormal = 2,
    ThreadPriorityHigh = 3,
} ThreadPriority;

/*! Opaque type for threads.
 *
 */
typedef struct Thread Thread;

/*! Type for thread functions
 *
 * @param[in] argument		A value passed in from thread creation.
 * @return 	    			A value passed back to the `threadJoin()` call.
 */
typedef intptr_t (*ThreadFunction)(void* argument);

/*! Create a new thread, given a thread function and an argument.
 *
 * @param[in] thread        An unused thread object.
 * @param[in] function      The function to run in the thread.
 * @param[in] argument      THe argument to pass to the function.
 *
 * @return          		0 if successful, an error otherwise.
 */
int threadCreate(Thread* thread, ThreadFunction function, void* argument);

/*! Recommend a thread priority
 *
 * @param[in] thread        A thread object that had `threadCreate()` called.
 * @param[in] priority      The recommended thread priority.
 */
void threadSetPriority(Thread* thread, ThreadPriority priority);

/*! Exit the current thread - with a result of 0 passed back the `threadJoin()`
 *
 */
void threadExit(void);

/*! Wait for a previously created thread to finish.
 *
 * If resultPtr is not NULL, write the return value from the thread function to it.
 *
 * @param[in]  thread           A thread object that had `threadCreate()` called.
 * @param[out] resultPtr        If not NULL, where to write the result from the thread.
 */
int threadJoin(Thread* thread, intptr_t* resultPtr);

/*! Sleep the current thread for a given time.
 *
 * @param[in] milliseconds      The minimum amount of time to sleep this thread.
 */
int threadSleep(uint32_t milliseconds);

/*! Give other threads a scheduling opportunity.
 *
 */
static inline void threadYield(void);

/*! Try to associate a name with the current thread.
 *
 * Typically used by debuggers and performance tracing.
 *
 * @param[in] name      A string to sue as the name
 */
void threadSetName(const char* name);

/*! Return the number of cores
 *
 * @return               Number of cores available for threading
 */
int32_t threadNumCores(void);

/*! Opaque type for mutexes.
 *
 */
typedef struct ThreadMutex ThreadMutex;

/*! Initialise a mutex.
 *
 * @param[in] mutex         An unused mutex object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadMutexInitialize(ThreadMutex* mutex);

/*! Destroy a mutex.
 *
 * @param[in] mutex         An  initialized a mutex. mutex object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadMutexDestroy(ThreadMutex* mutex);

/*! Lock a mutex.
 *
 * If the mutex is locked, this will block the thread until it is available.
 *
 * @param[in] mutex         An initialized mutex object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadMutexLock(ThreadMutex* mutex);

/*! Try to lock a mutex.
 *
 * If the mutex is already locked, returns `ThreadResultAgain.
 * Does not block.
 *
 * @param[in] mutex         An initialized mutex object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadMutexTrylock(ThreadMutex* mutex);

/*! Unlock a mutex.
 *
 * @param[in] mutex         An initialized mutex object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadMutexUnlock(ThreadMutex* mutex);

/*! Opaque type for condition Variables.
 */
typedef struct ThreadCondVar ThreadCondVar;

/*! Initialise a condition variable.
 *
 * @param[in] condvar       An unused condition variable object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadCondVarInitialize(ThreadCondVar* condvar);

/*! Destroy an existing condition variable.
 *
 * @param[in] condvar       An initialized  condition variable object.
 */
static inline void threadCondVarDestroy(ThreadCondVar* condvar);

/*! Try to signal one thread that is waiting on the condition variable.
 *
 * If there are any threads waiting on this condition variable, at least
 * one will be woken. Any of the other waiting threads may wake up, so all threads should
 * check the guarded state to see if they should proceed.
 *
 * @param[in] condvar       An initialized  condition variable object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadCondVarSignal(ThreadCondVar* condvar);

/*! Signal all threads that are waiting on the condition variable.
 *
 * All threads that are waiting on this condition variable will be woken up.
 *
 * @param[in] condvar       An initialized  condition variable object.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadCondVarBroadcast(ThreadCondVar* condvar);

/*! Wait to be signalled by another thread.
 *
 * Given a locked mutex and a condition variable, the mutex will be unlocked,
 * and the thread will be put to sleep to wait for a signal. Once signalled, the mutex
 * will be relocked, ant the thread woken.
 *
 * If there are several threads waiting on the same variable and mutex - more that one may get
 * woken up by a signal. They will each get their turn with the locked state.
 *
 * @param[in] condvar       An initialized condition variable object.
 * @param[in] mutex         An initialized mutex object, locked by this thread.
 * @return                  0 if successful, an error otherwise.
 */
static inline int threadCondVarWait(ThreadCondVar* condvar, ThreadMutex* mutex);

/*! Wait to be signalled by another thread, with a timeout
 *
 * Given a locked mutex and a condition variable, the mutex will be unlocked,
 * and the thread will be put to sleep to wait for a signal. Once signalled, the mutex
 * will be relocked, ant the thread woken. If a signal has not arrived within the given timeout,
 * the function will unblock, lock the mutex and return.
 *
 * If there are several threads waiting on the same variable and mutex - more that one may get
 * woken up by a signal. They will each get their turn with the locked state.
 *
 * @param[in] condVar       An initialized condition variable object.
 * @param[in] mutex         An initialized mutex object, locked by this thread.
 * @param[in] deadline      The local time to stop waiting
 * @return                  0 if successful or ThreadResultTimeout if the time expired.
 */
int threadCondVarWaitDeadline(ThreadCondVar* condVar, ThreadMutex* mutex, uint64_t deadline);

/*! Get a local time in microseconds with offset
 *
 * This will be based on some system-side start time eg: CLOCK_MONOTONIC, CLOCK_REALTIME or QueryPerformanceCounter()
 *
 * @param[in] microseconds      Offset from current time
 */
uint64_t threadTimeMicroseconds(int32_t microseconds);

// Implementation specific declarations
//
#if VN_SDK_FEATURE(THREADS_CUSTOM)
#include "threads_custom.h"
#elif VN_OS(WINDOWS)
#include "detail/threads_win32.h"
#else
#include "detail/threads_pthread.h"
#endif

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_THREADS_H
