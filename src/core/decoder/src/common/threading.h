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

#ifndef VN_DEC_CORE_THREADING_H_
#define VN_DEC_CORE_THREADING_H_

#include "common/platform.h"

typedef struct Logger* Logger_t;
typedef struct Memory* Memory_t;
typedef struct Thread Thread_t;

typedef struct ThreadManager
{
    Memory_t memory;
    Logger_t log;
    Thread_t* threads;
    uint32_t numThreads;
} ThreadManager_t;

typedef struct JobIndex
{
    size_t current; /**< Index of current job, will be between 0 and job-count - 1. */
    size_t last;    /**< Index of the last job in this batch, will always be job-count - 1. */
} JobIndex_t;

typedef struct SliceOffset
{
    size_t offset; /**< Offset this job slice starts from (inclusive). */
    size_t count;  /**< Number of elements this slice should process. */
} SliceOffset_t;

typedef int32_t (*JobFunction_t)(void*);
typedef int32_t (*SlicedJobFunction_t)(const void*, JobIndex_t index, SliceOffset_t offset);

static inline bool isFirstSlice(JobIndex_t index) { return index.current == 0; }
static inline bool isLastSlice(JobIndex_t index) { return index.current == index.last; }

/*------------------------------------------------------------------------------
 * Threading API
 *----------------------------------------------------------------------------*/

/*! \brief init the thread engine
 *
 *  \param mgr the number of threads to start
 *
 *  \returns 0 on success */
int32_t threadingInitialise(Memory_t memory, Logger_t log, ThreadManager_t* mgr, uint32_t numThreads);

/*! \brief Release the threading engine. */
void threadingRelease(ThreadManager_t* mgr);

/*! \brief Get the number of cores on this system
 *
 *  \return <0 on error, otherwise number of cores. */
int32_t threadingGetNumCores(void);

/*! \brief Get the number of threads */
uint32_t threadingGetNumThreads(ThreadManager_t* mgr);

/*! \brief Executes a list of jobs, this call blocks.
 *
 * This will loop over the jobs array submitting the jobs to available threads,
 * then waiting on the jobs to complete before returning, the calling thread will
 * also execute jobs.
 *
 *  \param mgr          The threading manager.
 *  \param function     The function that the job will run on.
 *  \param jobs         The array of jobs to operate upon.
 *  \param jobCount     The number of jobs in the jobs array.
 *  \param jobByteSize  The byte size of each job element (sizeof(job_type)).
 *
 *  \return True if the jobs executed correctly and jobs returned success.
 */
bool threadingExecuteJobs(ThreadManager_t* mgr, JobFunction_t function, void* jobs,
                          uint32_t jobCount, uint32_t jobByteSize);

/*! \brief Generates and executes a list of sliced jobs. This call blocks.
 *
 *  This evenly slices the `totalSize` range up amongst all available threads
 *  (and the calling thread), and invoking them.
 *
 *  Each job is passed a unique index between 0 and number of slices - 1 (inclusive).
 *  The index passed in correlates directly to the sub-range the job is meant to
 *  work on. E.g. first slice of offset 0 will have an index of 0, the last slice will
 *  have index (number of slices - 1).
 *
 *  \param mgr             The threading manager
 *  \param function        The function that will be run on multiple threads.
 *  \param executeContext  Immutable state passed to each invocation of `function`
 *  \param totalSize       The range to even slice over multiple invocations of `function`
 *
 *  \return True when all invocations returned 0, otherwise false.
 */
bool threadingExecuteSlicedJobs(ThreadManager_t* mgr, SlicedJobFunction_t function,
                                const void* executeContext, size_t totalSize);

/*! \brief Generates and executes a list of sliced jobs, once all jobs have completed
 *        the `postRunFunction` will be invoked for each job on the calling thread.
 *        This call blocks.
 */
bool threadingExecuteSlicedJobsWithPostRun(ThreadManager_t* mgr, SlicedJobFunction_t function,
                                           SlicedJobFunction_t postRunFunction,
                                           const void* executeContext, size_t totalSize);

/*------------------------------------------------------------------------------
 * Mutex
 *----------------------------------------------------------------------------*/
typedef struct Mutex Mutex_t;

int32_t mutexInitialise(Memory_t memory, Mutex_t** mutex);
void mutexRelease(Mutex_t* mutex);
void mutexLock(Mutex_t* mutex);
void mutexUnlock(Mutex_t* mutex);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_THREADING_H_ */
