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

#ifndef VN_LCEVC_COMMON_DETAIL_TASK_POOL_H
#define VN_LCEVC_COMMON_DETAIL_TASK_POOL_H

// NOLINTBEGIN(modernize-use-using)

#include <LCEVC/common/bitutils.h>
#include <LCEVC/common/deque.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>
#include <LCEVC/common/vector.h>

/*! The underlying task pool
 */
typedef struct LdcTaskPool
{
    LdcMemoryAllocator* longTermAllocator;
    LdcMemoryAllocator* shortTermAllocator;

    uint32_t threadCount;

    // Per thread data
    LdcMemoryAllocation threads;

    // Per task data - vector of allocations
    LdcVector tasks;

    // Number of not done tasks
    uint32_t pendingTaskCount;

    // True if multithreaded - tasks are handled by seperate thread workers
    bool multiThreaded;

    // Thread workers are running
    bool running;

    // A deque of ready task parts
    // This will move to per-thread later when work stealing is sorted (and the reason
    // why this is a deque vs. a simple list)
    LdcDeque readyParts;

    // Mutex for locking whole pool
    // NB: THis is way too conservative - but is a good starting point for getting
    // everything correct
    ThreadMutex mutex;

    // Condition variable that is signalled when tasks become ready to run.
    //
    // NB: THis is way too conservative - but is a good starting point for getting
    // everything correct
    ThreadCondVar condVarReady;

    // Condition variable that is signalled when tasks have been completed
    //
    ThreadCondVar condVarCompleted;
} LdcTaskPool;

/*! Per thread state
 */
typedef struct LdcTaskThread
{
    LdcTaskPool* taskPool;

    // The thread
    Thread thread;

    // Current task part being processed by this thread
    LdcTaskPart part;

    // Pointer to first ready task on this thread
    // Once stable - ready lists will move to be per-thread.
    // LdcTask* readyTasks;
} LdcTaskThread;

/*! Running state of task.
 */
typedef enum LdcTaskState
{
    LdcTaskStateNone = 0,

    LdcTaskStateWaiting,
    LdcTaskStateReady,
    LdcTaskStateRunning,
    LdcTaskStateBlocked,
    LdcTaskStateDone,
} LdcTaskState;

/*! State of an ongoing task
 *
 * Can be split into several parts that cover the same overall task
 */
typedef struct LdcTask
{
    // The pool that this task belongs to.
    LdcTaskPool* pool;
    // Optional group that this task belongs to.
    LdcTaskGroup* group;

    const char* name; // Name used in debug dumps

    // Function to carry out task.
    LdcTaskFunction taskFunction;
    // Function to call once task is completed
    LdcTaskFunction completionFunction;

    // A set of group dependencies that need to be met before task can run. If != zero, then `group` must be set
    LdcTaskDependency* inputs;
    uint32_t inputsCount;

    // The group dependency that will be met by this task. If != kTaskDependencyInvalid, then `group` must be set
    LdcTaskDependency output;

    // Total number of things in overall task
    uint32_t iterationsTotalCount;

    // Limit on number of iterations handled by each part
    uint32_t maxIterationsPerPart;

    // Updated by threads as task progresses
    uint32_t iterationsCompletedCount;

    LdcTaskState state;
    LdcTaskDependency lowestUnmetDependency;

    // Number of task parts in progress
    uint32_t activeParts;

    // Next task in waiting list
    LdcTask* nextTask;

    // For future LBS control, will `:
    //  - uint32_t stopSplittingThreshold;
    //  - int32_t profitableParallelismThreshold;

    // The output value from the task
    void* outputValue;

    // True if the caller will not wait for the task - set via ldcTaskNoWait()
    bool detached;

    size_t dataSize; // Size of per-task parameter data
    uint8_t data[1]; // Variable size array of per task data - will be allocated following LdcTask structure
    // NB: DO NOT PUT ANY MORE MEMBERS HERE
} LdcTask;

/*! The connections between group of tasks
 *
 * A Task group is a collection of tasks that may have dependencies between them.
 * Each dependency can carry a 'void *' pointer from one task to the next
 *
 * NB: the actual data to use may also be implied by the particular dependency and the receiving
 * task's configuration.
 */
typedef struct LdcTaskGroup
{
    // Pool that holds this group of tasks
    LdcTaskPool* pool;

    const char* name; // Name used in debug dumps

    // Tasks remaining in this group
    uint32_t tasksCount;

    // True if group is blocked - added tasks will not be scheduled
    bool blocked;

    // List of tasks that are waiting to be scheduled when group is no longer blocked
    uint32_t blockedTasksCount;
    LdcTask* blockedTasks;

    // Reserved dependcy slots
    uint32_t dependenciesReserved;

    // Allocation for dependency data
    LdcMemoryAllocation dependencyAllocation;

    // Number of added dependencies
    uint32_t dependenciesCount;

    // Set of the dependencies that have been met within this group
    // Stored as a bitmask in 1 or more uint64_t
    uint64_t* dependenciesMet;

    // The dependency values used to connect this group of tasks together
    void** dependencyValues;

    // List of tasks that are waiting on each dependency - if a task has
    // multiple inputs, it will be on list of the lowest dependency
    //
    uint32_t waitingTasksCount;
    LdcTask** waitingTasks;

} LdcDependencies;

// NOLINTEND(modernize-use-using)
#endif // VN_LCEVC_COMMON_DETAIL_TASK_POOL_H
