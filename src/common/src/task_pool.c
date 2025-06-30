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

#include <LCEVC/common/task_pool.h>
//
#include <LCEVC/common/bitutils.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/deque.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/threads.h>
//
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Forward declarations
static void scheduleTask(LdcTaskPool* pool, LdcTask* task, LdcTask** head);
#ifdef VN_SDK_LOG_ENABLE_DEBUG
static void taskPoolDump(LdcTaskPool* pool, const LdcTaskGroup* group);
#endif

// Common task creation
//
static LdcTask* addTask(LdcTaskPool* pool, LdcTaskGroup* group, const LdcTaskDependency* inputs,
                        uint32_t inputsCount, LdcTaskDependency output, LdcTaskFunction function,
                        LdcTaskFunction completion, uint32_t iterations, uint32_t maxIterationsPerPart,
                        size_t dataSize, const void* data, const char* name)
{
    assert(pool);
    assert(pool->running);
    assert(function);
    assert(iterations > 0);
    assert(!inputs || group);
    assert(inputs || inputsCount == 0);

    LdcMemoryAllocation allocation = {0};

    // Allocate task block with extra for task data on end
    // NB: there is a wasted byte which will likely round up to a machine word - no great loss
    // but may be worth clearing up once everything else is stable.
    dataSize = VNAlignSize(dataSize, sizeof(uint32_t));
    LdcTask* task =
        (LdcTask*)VNAllocateZeroArray(pool->shortTermAllocator, &allocation, uint8_t,
                                      sizeof(LdcTask) + dataSize + sizeof(uint32_t) * inputsCount);

    if (task == NULL) {
        VNLogError("Cannot allocate task.");
        return NULL;
    }

    ldcVectorAppend(&pool->tasks, &allocation);
    pool->pendingTaskCount++;
    if (group) {
        group->tasksCount++;
    }

    // Fill in slot
    task->pool = pool;
    task->group = group;
    task->output = output;
    task->taskFunction = function;
    task->completionFunction = completion;
    task->iterationsTotalCount = iterations;
    task->iterationsCompletedCount = 0;
    task->maxIterationsPerPart = maxIterationsPerPart;

    // Copy task data
    if (dataSize) {
        assert(data);
        memcpy(task->data, data, dataSize);
    }
    task->dataSize = dataSize;

    // Input dependencies - stored after task data
    if (inputsCount && inputs) {
        task->inputs = (uint32_t*)(task->data + dataSize);
        task->inputsCount = inputsCount;
        for (uint32_t i = 0; i < inputsCount; ++i) {
            assert(inputs[i] < group->dependenciesCount);
            task->inputs[i] = inputs[i];
        }
    } else {
        task->inputs = NULL;
        task->inputsCount = 0;
    }

    // Debugging Name
    task->name = name;

    scheduleTask(pool, task, NULL);

    return task;
}

// Try to remove from head of list, given pointer to head
//
// NB: Called with task pool locked
//
static inline LdcTask* getNextTask(LdcTask** head)
{
    LdcTask* task = *head;

    if (task != NULL) {
        // If list is not empty, move pointer to next in list
        *head = task->nextTask;
    }
    return task;
}

// Task is done - free up it's memory
//
// NB: Called with task pool locked
//
static void removeTask(LdcTaskPool* pool, LdcTask* task)
{
    if (task->group) {
        assert(task->group->tasksCount > 0);
        task->group->tasksCount--;
    }

    LdcMemoryAllocation* alloc = ldcVectorFindUnordered(&pool->tasks, ldcVectorCompareAllocationPtr, task);

    if (!alloc) {
        VNLogError("Cannot find task in pool.");
        return;
    }

    VNFree(pool->shortTermAllocator, alloc);

    ldcVectorRemoveReorder(&pool->tasks, alloc);
}

// Low level dependency bit operations
//
static inline void dependencyMetBitSet(LdcTaskGroup* group, LdcTaskDependency dependency)
{
    assert(group);
    assert(dependency < group->dependenciesCount);
    group->dependenciesMet[dependency >> 6] |= (1LL << (dependency & 63));
}

static inline bool dependencyMetBitGet(const LdcTaskGroup* group, LdcTaskDependency dependency)
{
    assert(group);
    assert(dependency < group->dependenciesCount);
    return (group->dependenciesMet[dependency >> 6] & (1LL << (dependency & 63))) != 0;
}

// Dependency has been met - record value and reschedule tasks as appropriate.
//
static void dependencyMet(LdcTaskGroup* group, LdcTaskDependency dependency, void* value)
{
    assert(group);
    assert(group->pool);
    assert(dependency < group->dependenciesCount);

    if (dependencyMetBitGet(group, dependency)) {
        // Already met and valid - it allows one task to defer dependency resolution to a spawned task
        return;
    }

    VNLogVerbose("dependencyMet: Group:%p  %d %p", (void*)group, dependency, (void*)group);

    // Fill in dependency
    dependencyMetBitSet(group, dependency);
    group->dependencyValues[dependency] = value;

    // Go through tasks waiting on this dependency, and reschedule them
    LdcTask** head = &group->waitingTasks[dependency];
    while (*head != NULL) {
        const LdcTask* next = (*head)->nextTask;
        VNUnused(next);
        scheduleTask(group->pool, *head, head);
        assert(*head == next);
    }
}

// Check if all a tasks dependencies have been met
//
static inline bool taskDependenciesMet(const LdcTask* task)
{
    const LdcTaskGroup* group = task->group;
    assert(group);

    for (uint32_t i = 0; i < task->inputsCount; ++i) {
        if (!dependencyMetBitGet(group, task->inputs[i])) {
            return false;
        }
    }

    return true;
}

// Find lowest unmet dependency in task
//
static inline LdcTaskDependency taskLowestUnmetDependency(const LdcTask* task)
{
    const LdcTaskGroup* group = task->group;
    assert(group);

    for (uint32_t i = 0; i < task->inputsCount; ++i) {
        if (!dependencyMetBitGet(group, task->inputs[i])) {
            return task->inputs[i];
        }
    }

    return kTaskDependencyInvalid;
}

// Check if a task has given dependency as one of it's inputs
//
static inline bool taskDependsOnInput(const LdcTask* task, LdcTaskDependency dep)
{
    for (uint32_t i = 0; i < task->inputsCount; ++i) {
        if (task->inputs[i] == dep) {
            return true;
        }
    }

    return false;
}

// Task is done - store any output value, pass on dependency, and clean up
//
static inline void finishTask(LdcTaskPool* pool, LdcTask* task, void* value)
{
    task->outputValue = value;
    task->state = LdcTaskStateDone;
    pool->pendingTaskCount--;

    if (task->output != kTaskDependencyInvalid) {
        assert(task->group);
        assert(task->output < task->group->dependenciesCount);
        dependencyMet(task->group, task->output, value);
    }

    // If task will not be waited for - clear it away
    if (task->detached) {
        removeTask(pool, task);
    }
}

// Do the task work
//
// Task pool mutex should be locked at this point
//
static inline void runTask(LdcTaskPool* pool, LdcTaskPart* taskPart)
{
    assert(pool);
    assert(taskPart);
    assert(taskPart->task);

    void* value = 0;

    LdcTask* const task = taskPart->task;

    if (task->activeParts == 0) {
        assert(task->state == LdcTaskStateReady);
        task->state = LdcTaskStateRunning;
    } else {
        assert(task->state == LdcTaskStateRunning);
    }
    task->activeParts += 1;

    if (task->taskFunction) {
        // Do the work with pool unlocked
        threadMutexUnlock(&pool->mutex);
        value = task->taskFunction(task, taskPart);
        threadMutexLock(&pool->mutex);
    }

    task->activeParts -= 1;
    if (task->activeParts == 0) {
        task->state = LdcTaskStateReady;
    }

    // Record the contribution of this part
    task->iterationsCompletedCount += taskPart->count;

    // Are we there yet?
    if (task->iterationsCompletedCount == task->iterationsTotalCount) {
        // Task is done
        //

        if (task->completionFunction) {
            // Do the completion with pool unlocked
            LdcTaskPart part = {task, task->iterationsTotalCount, 0};
            threadMutexUnlock(&pool->mutex);
            value = task->completionFunction(task, &part);
            threadMutexLock(&pool->mutex);
        }

        finishTask(pool, task, value);
    }
}

// Task is ready to run
//
// Task pool mutex should be locked at this point
//
static inline void readyTask(LdcTaskPool* pool, LdcTask* task, bool grouped)
{
    VNLogVerbose("scheduleTask: %s %s ready: %p %s", pool->multiThreaded ? "Multi" : "Single",
                 grouped ? "Group" : "Standalone", (void*)task, task->name);

    task->state = LdcTaskStateReady;
    if (!task->taskFunction && !task->completionFunction) {
        // Null task - just finish it
        // (This is a shortcut for connecting a bunch of input dependencies to a single output)
        task->iterationsCompletedCount = task->iterationsTotalCount;
        finishTask(pool, task, NULL);
    } else if (pool->multiThreaded) {
        LdcTaskPart part = {0};
        part.task = task;
        part.start = 0;

        // Multithreaded - add to ready list and kick workers
        if (task->iterationsTotalCount == 1) {
            // Simple task - single part
            part.count = task->iterationsTotalCount;
            ldcDequeBackPush(&pool->readyParts, &part);
        } else {
            // Split by number of threads
            // NB: THis should really happen dynamically at thread level via work stealing
            const uint32_t perPart =
                (task->iterationsTotalCount + pool->threadCount - 1) / pool->threadCount;
            uint32_t remaining = task->iterationsTotalCount;
            while (remaining > 0) {
                part.count = minU32(remaining, perPart);
                ldcDequeBackPush(&pool->readyParts, &part);

                part.start += part.count;
                remaining -= part.count;
            }
        }
        threadCondVarBroadcast(&pool->condVarReady);
    } else {
        // Single threaded - run task now
        LdcTaskPart part = {0};
        part.task = task;
        part.start = 0;
        part.count = task->iterationsTotalCount;
        runTask(pool, &part);
    }
}

// Update task's list and state
//
// Task pool mutex should be locked at this point
//
static void scheduleTask(LdcTaskPool* pool, LdcTask* task, LdcTask** head)
{
    assert(pool);
    assert(task);
    assert(task->pool == pool);

    // New standalone task - add to ready list
    if (task->group == NULL) {
        assert(task->state == LdcTaskStateNone);
        assert(head == NULL);

        // Ready
        readyTask(pool, task, false);
        return;
    }

    // Task is part of a group ...
    //
    if (task->group->blocked) {
        // Group is blocked - add to blocked list
        task->state = LdcTaskStateBlocked;
        task->nextTask = task->group->blockedTasks;
        task->group->blockedTasks = task;
        task->group->blockedTasksCount++;
        return;
    }

    if (taskDependenciesMet(task)) {
        // Grouped task is ready - move from waiting to  ready list
        if (head) {
            // Remove from current list
            assert(task->state == LdcTaskStateWaiting);
            assert(*head == task);
            getNextTask(head);
            task->group->waitingTasksCount--;
        } else {
            assert(task->state == LdcTaskStateNone);
        }

        // Ready
        readyTask(pool, task, true);
        return;
    }

    // Grouped task that is still waiting
    const LdcTaskDependency lowestUnmetDependency = taskLowestUnmetDependency(task);
    assert(lowestUnmetDependency != kTaskDependencyInvalid);

    // Already on the right list
    if (task->state == LdcTaskStateWaiting && task->lowestUnmetDependency == lowestUnmetDependency) {
        return;
    }

    // Remove from current list
    if (head) {
        assert(task->state == LdcTaskStateWaiting);
        assert(*head == task);
        getNextTask(head);
        task->group->waitingTasksCount--;
    } else {
        assert(task->state == LdcTaskStateNone);
    }

    // Add to correct list
    task->lowestUnmetDependency = lowestUnmetDependency;
    task->nextTask = task->group->waitingTasks[lowestUnmetDependency];
    task->group->waitingTasks[lowestUnmetDependency] = task;
    task->group->waitingTasksCount++;

    task->state = LdcTaskStateWaiting;
}

// The per thread worker function
//
static intptr_t taskThreadWorker(void* argument)
{
    // Private thread data
    LdcTaskThread* taskThread = (LdcTaskThread*)argument;
    assert(taskThread);
    LdcTaskPool* pool = taskThread->taskPool;
    assert(pool);

    for (;;) {
        threadMutexLock(&pool->mutex);

        // Closing down?
        if (!pool->running) {
            threadMutexUnlock(&pool->mutex);
            break;
        }

        const bool gotPart = ldcDequeFrontPop(&pool->readyParts, &taskThread->part);

        if (!gotPart) {
            // Wait for something to be ready ...
            threadCondVarWait(&pool->condVarReady, &pool->mutex);
        } else {
            // Run it
            runTask(pool, &taskThread->part);
            taskThread->part.task = NULL;
            threadCondVarBroadcast(&pool->condVarCompleted);
        }

        threadMutexUnlock(&pool->mutex);
    }

    return 0;
}

bool ldcTaskPoolInitialize(LdcTaskPool* pool, LdcMemoryAllocator* longTermAllocator,
                           LdcMemoryAllocator* shortTermAllocator, uint32_t threadCount,
                           uint32_t reservedTaskCount)
{
    VNClear(pool);

    pool->longTermAllocator = longTermAllocator;
    pool->shortTermAllocator = shortTermAllocator;

    // Reserved slots for tasks
    ldcVectorInitialize(&pool->tasks, sizeof(LdcMemoryAllocation), maxU32(1, reservedTaskCount),
                        pool->longTermAllocator);

    // Mutexes for thread sync.
    VNCheck(threadMutexInitialize(&pool->mutex) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&pool->condVarReady) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&pool->condVarCompleted) == ThreadResultSuccess);

    // Threads
    pool->multiThreaded = (threadCount > 0);

    pool->threadCount = threadCount;
    pool->running = true;

    if (pool->multiThreaded) {
        threadMutexLock(&pool->mutex);

        VNAllocateZeroArray(longTermAllocator, &pool->threads, LdcTaskThread, threadCount);

        ldcDequeInitialize(&pool->readyParts, threadCount * 2, sizeof(LdcTaskPart), pool->longTermAllocator);

        LdcTaskThread* taskThreads = VNAllocationPtr(pool->threads, LdcTaskThread);
        for (uint32_t thr = 0; thr < threadCount; ++thr) {
            taskThreads[thr].taskPool = pool;
            taskThreads[thr].part.task = NULL;
            threadCreate(&taskThreads[thr].thread, taskThreadWorker, &taskThreads[thr]);
        }

        threadMutexUnlock(&pool->mutex);
    }

    return true;
}

void ldcTaskPoolDestroy(struct LdcTaskPool* pool)
{
    if (pool->multiThreaded) {
        threadMutexLock(&pool->mutex);

        assert(pool->running);

        // Tell threads to stop
        pool->running = false;

        // Kick all the threads to say something is happening
        threadCondVarBroadcast(&pool->condVarReady);

        threadMutexUnlock(&pool->mutex);

        // Wait for threads to stop
        LdcTaskThread* taskThreads = VNAllocationPtr(pool->threads, LdcTaskThread);
        for (uint32_t thr = 0; thr < pool->threadCount; ++thr) {
            threadJoin(&taskThreads[thr].thread, NULL);
        }
        VNFree(pool->longTermAllocator, &pool->threads);

        ldcDequeDestroy(&pool->readyParts);
    }

    // At this point - there will be no other threads sharing the data
    //
    // Release any remaining tasks
    //
    for (uint32_t task = 0; task < ldcVectorSize(&pool->tasks); ++task) {
        VNFree(pool->longTermAllocator, ldcVectorAt(&pool->tasks, task));
    }

    ldcVectorDestroy(&pool->tasks);
}

void ldcTaskPoolWait(struct LdcTaskPool* pool)
{
    assert(pool);

    if (pool->multiThreaded) {
        threadMutexLock(&pool->mutex);
        assert(pool->running);

        for (;;) {
            if (pool->pendingTaskCount == 0) {
                // No tasks remaining
                break;
            }
            threadCondVarWait(&pool->condVarCompleted, &pool->mutex);
        }

        threadMutexUnlock(&pool->mutex);
    } else {
        assert(pool->running);
    }
}

uint32_t ldcTaskPoolGetNumThreads(const struct LdcTaskPool* pool)
{
    //
    return pool->threadCount;
}

// Task that is not part of any group - no dependencies involved.
//
LdcTask* ldcTaskPoolAdd(LdcTaskPool* pool, LdcTaskFunction function, LdcTaskFunction completion,
                        uint32_t iterations, size_t dataSize, const void* data, const char* name)
{
    assert(pool);

    threadMutexLock(&pool->mutex);

    // No group or inputs/outputs
    LdcTask* task = addTask(pool, NULL, NULL, 0, kTaskDependencyInvalid, function, completion,
                            iterations, iterations, dataSize, data, name);

    threadMutexUnlock(&pool->mutex);

    return task;
}

// Task that is part of a group - with 0 or more input and 0 or 1 output dependencies.
//
bool ldcTaskGroupAdd(LdcTaskGroup* group, const LdcTaskDependency* inputs, uint32_t inputsCount,
                     LdcTaskDependency output, LdcTaskFunction function, LdcTaskFunction completion,
                     uint32_t iterations, uint32_t maxIterationsPerPart, size_t dataSize,
                     const void* data, const char* name)
{
    assert(group);
    assert(group->pool);

    // For later used by work stealing
    VNUnused(maxIterationsPerPart);

    threadMutexLock(&group->pool->mutex);

    LdcTask* task = addTask(group->pool, group, inputs, inputsCount, output, function, completion,
                            iterations, iterations, dataSize, data, name);

    threadMutexUnlock(&group->pool->mutex);

    if (!task) {
        return false;
    }

    // Group tasks don't use ldcTaskWait - pick up outputs from the group
    ldcTaskNoWait(task);
    return true;
}

// Mark task as not needing a wait to clear up
void ldcTaskNoWait(LdcTask* task)
{
    assert(task != NULL);
    assert(task->pool);
    LdcTaskPool* pool = task->pool;

    if (threadMutexLock(&pool->mutex) != 0) {
        return;
    }

    if (task->state == LdcTaskStateDone) {
        // Done already
        removeTask(pool, task);
    } else {
        task->detached = true;
    }

    threadMutexUnlock(&pool->mutex);
}

// Wait for task to finish
bool ldcTaskWait(LdcTask* task, void** outputPtr)
{
    if (task == NULL) {
        return false;
    }

    assert(task->pool);
    LdcTaskPool* pool = task->pool;

    if (threadMutexLock(&pool->mutex) != 0) {
        return false;
    }

    // Wait for task to move to done
    while (task->state != LdcTaskStateDone) {
        threadCondVarWait(&pool->condVarCompleted, &pool->mutex);
    }

    // Save the output value if required
    if (outputPtr) {
        *outputPtr = task->outputValue;
    }

    removeTask(pool, task);

    threadMutexUnlock(&pool->mutex);
    return true;
}

bool ldcTaskWaitMany(LdcTask** tasks, uint32_t tasksCount, void** outputPtrs)
{
    bool accumulatedRet = true;

    for (uint32_t i = 0; i < tasksCount; ++i) {
        void* output = NULL;
        const bool ret = ldcTaskWait(tasks[i], &output);

        if (!ret) {
            accumulatedRet = false;
        }

        if (outputPtrs) {
            outputPtrs[i] = output;
        }
    }

    return accumulatedRet;
}

// Allow new group task scheduling to be paused/started
//
void ldcTaskGroupBlock(LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    LdcTaskPool* pool = group->pool;

    if (threadMutexLock(&pool->mutex) != ThreadResultSuccess) {
        return;
    }

    if (!group->blocked) {
        group->blocked = true;
        assert(group->blockedTasksCount == 0);
        assert(group->blockedTasks == NULL);
    }

    threadMutexUnlock(&pool->mutex);
}

void ldcTaskGroupUnblock(LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    LdcTaskPool* pool = group->pool;

    if (threadMutexLock(&pool->mutex) != ThreadResultSuccess) {
        return;
    }

    if (group->blocked) {
        group->blocked = false;

        // Go through blocked tasks and schedule them
        LdcTask* task = group->blockedTasks;
        while (task != NULL) {
            LdcTask* next = task->nextTask;
            task->nextTask = NULL;
            assert(task->state == LdcTaskStateBlocked);
            task->state = LdcTaskStateNone;
            scheduleTask(pool, task, NULL);
            task = next;
        }

        group->blockedTasksCount = 0;
    }

    threadMutexUnlock(&pool->mutex);
}

// Reserve space for a given number of dependencies in the group - moves any previous data into new block
//
static void taskGroupReserve(struct LdcTaskGroup* group, uint32_t dependenciesReserved)
{
    assert(group);
    assert(dependenciesReserved > group->dependenciesReserved);

    const uint32_t newMetSize = (VNAlignSize(dependenciesReserved, 64) / 64) * sizeof(uint64_t);
    const uint32_t newPtrSize = dependenciesReserved * sizeof(void*);
    LdcMemoryAllocation newAllocation = {0};
    uint8_t* alloc = VNAllocateZeroArray(group->pool->shortTermAllocator, &newAllocation, uint8_t,
                                         newMetSize + newPtrSize * 2);

    uint64_t* const newDependenciesMet = (uint64_t*)alloc;
    alloc += newMetSize;
    LdcTask** const newWaitingTasks = (LdcTask**)alloc;
    alloc += newPtrSize;
    void** const newDependencyValues = (void**)alloc;

    // Move any old data over
    if (VNIsAllocated(group->dependencyAllocation)) {
        const uint32_t metSize = (VNAlignSize(group->dependenciesReserved, 64) / 64) * sizeof(uint64_t);
        const uint32_t ptrSize = group->dependenciesReserved * sizeof(void*);

        memcpy(newDependenciesMet, group->dependenciesMet, metSize);
        memcpy(newWaitingTasks, group->waitingTasks, ptrSize);
        memcpy(newDependencyValues, group->dependencyValues, ptrSize);

        VNFree(group->pool->shortTermAllocator, &group->dependencyAllocation);
    }

    group->dependencyAllocation = newAllocation;
    group->dependenciesMet = newDependenciesMet;
    group->waitingTasks = newWaitingTasks;
    group->dependencyValues = newDependencyValues;

    group->dependenciesReserved = dependenciesReserved;
}

bool ldcTaskGroupInitialize(LdcTaskGroup* group, LdcTaskPool* pool, uint32_t dependenciesReserved)
{
    assert(group);
    assert(pool);
    assert(dependenciesReserved <= kTaskPoolMaxDependencies);

    threadMutexLock(&pool->mutex);

    // Is pool good?
    assert(pool->running);

    // Set up group with no dependencies or tasks (so far)
    VNClear(group);
    group->pool = pool;

    taskGroupReserve(group, dependenciesReserved);

    threadMutexUnlock(&pool->mutex);
    return true;
}

void ldcTaskGroupDestroy(struct LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    LdcTaskPool* pool = group->pool;
    threadMutexLock(&pool->mutex);

    VNFree(pool->shortTermAllocator, &group->dependencyAllocation);

    group->pool = NULL;
    group->dependenciesReserved = 0;

    threadMutexUnlock(&pool->mutex);
}

void ldcTaskGroupWait(struct LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    LdcTaskPool* pool = group->pool;
    assert(pool->running);

    while (group->tasksCount != 0) {
        threadCondVarWait(&pool->condVarCompleted, &pool->mutex);
    }

    threadMutexUnlock(&group->pool->mutex);
}

uint32_t ldcTaskGroupGetTaskCount(const struct LdcTaskGroup* group, uint32_t* waitingPtr)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    uint32_t ret = group->tasksCount;
    if (waitingPtr) {
        *waitingPtr = group->waitingTasksCount;
    }
    threadMutexUnlock(&group->pool->mutex);
    return ret;
}

uint32_t ldcTaskGroupGetWaitingCount(const struct LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    uint32_t ret = group->waitingTasksCount;

    threadMutexUnlock(&group->pool->mutex);
    return ret;
}

bool ldcTaskGroupFindOutputSetFromInput(const struct LdcTaskGroup* group, LdcTaskDependency input,
                                        LdcTaskDependency* outputs, uint32_t outputsMax,
                                        uint32_t* outputsCount)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);
    LdcTaskPool* taskPool = group->pool;

    assert(input < group->dependenciesCount);

    // Go through all tasks finding ones in group
    //
    // If this becomes a problem, could add a per taskGroup link through tasks.
    //
    uint32_t count = 0;
    bool r = true;
    for (uint32_t i = 0; i < ldcVectorSize(&taskPool->tasks); ++i) {
        LdcMemoryAllocation* taskAlloc = ldcVectorAt(&taskPool->tasks, i);
        LdcTask* task = VNAllocationPtr(*taskAlloc, LdcTask);
        if (task->group != group) {
            continue;
        }

        if (taskDependsOnInput(task, input) && task->output != kTaskDependencyInvalid) {
            if (count < outputsMax) {
                outputs[count] = task->output;
            } else {
                r = false;
            }
            count++;
        }
    }

    threadMutexUnlock(&group->pool->mutex);

    *outputsCount = count;
    return r;
}

// Dependencies
//
LdcTaskDependency ldcTaskDependencyAdd(LdcTaskGroup* group)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    LdcTaskDependency dep = group->dependenciesCount;

    if (group->dependenciesCount >= group->dependenciesReserved) {
        taskGroupReserve(group, group->dependenciesReserved * 2);
    }

    group->dependenciesCount++;

    threadMutexUnlock(&group->pool->mutex);
    return dep;
}

LdcTaskDependency ldcTaskDependencyAddMet(LdcTaskGroup* group, void* value)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    const LdcTaskDependency dependency = group->dependenciesCount;

    if (group->dependenciesCount < group->dependenciesReserved) {
        group->dependenciesCount++;
    } else {
        taskGroupReserve(group, group->dependenciesReserved * 2);
    }
    // Mark dependency as set
    group->dependencyValues[dependency] = value;
    dependencyMetBitSet(group, dependency);

    threadMutexUnlock(&group->pool->mutex);
    return dependency;
}

bool ldcTaskDependencyIsMet(const LdcTaskGroup* group, LdcTaskDependency dependency)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    assert(dependency < group->dependenciesCount);

    bool ret = dependencyMetBitGet(group, dependency);

    threadMutexUnlock(&group->pool->mutex);
    return ret;
}

bool ldcTaskDependencySetIsMet(const LdcTaskGroup* group, const LdcTaskDependency* deps, uint32_t depsCount)
{
    assert(group);
    assert(group->pool);
    threadMutexLock(&group->pool->mutex);

    for (uint32_t i = 0; i < depsCount; ++i) {
        const LdcTaskDependency dep = deps[i];
        assert(dep < group->dependenciesCount);

        if (!dependencyMetBitGet(group, dep)) {
            threadMutexUnlock(&group->pool->mutex);
            return false;
        }
    }

    threadMutexUnlock(&group->pool->mutex);

    return true;
}

void* ldcTaskDependencyGet(const LdcTaskGroup* group, LdcTaskDependency dependency)
{
    assert(group);
    assert(group->pool);

    threadMutexLock(&group->pool->mutex);
    assert(dependency < group->dependenciesCount);
    assert(dependencyMetBitGet(group, dependency));

    void* ret = group->dependencyValues[dependency];

    threadMutexUnlock(&group->pool->mutex);

    return ret;
}

void ldcTaskDependencyMet(LdcTaskGroup* group, LdcTaskDependency dependency, void* value)
{
    assert(group);
    assert(group->pool);

    threadMutexLock(&group->pool->mutex);

    dependencyMet(group, dependency, value);

    threadMutexUnlock(&group->pool->mutex);
}

void* ldcTaskDependencyWait(const LdcTaskGroup* group, LdcTaskDependency dependency)
{
    assert(group);
    assert(group->pool);

    threadMutexLock(&group->pool->mutex);
    assert(dependency < group->dependenciesCount);

    while (!dependencyMetBitGet(group, dependency)) {
        threadCondVarWait(&group->pool->condVarCompleted, &group->pool->mutex);
    }

    void* ret = group->dependencyValues[dependency];

    threadMutexUnlock(&group->pool->mutex);

    return ret;
}

//
bool ldcTaskCollectInputs(const LdcTask* task, size_t numInputs, void** inputs)
{
    assert(task);
    assert(task->pool);
    assert(task->group);
    const LdcTaskGroup* group = task->group;

    assert(task->inputs != 0);
    assert(numInputs <= task->inputsCount);

    threadMutexLock(&task->pool->mutex);

    unsigned int idx = 0;
    for (; idx < numInputs; ++idx) {
        LdcTaskDependency dep = task->inputs[idx];
        assert(dependencyMetBitGet(group, dep));
        inputs[idx] = group->dependencyValues[dep];
    }

    threadMutexUnlock(&task->pool->mutex);

    return idx == numInputs;
}

LdcTaskDependency ldcTaskRemoveOutput(LdcTask* task)
{
    assert(task);
    assert(task->pool);

    threadMutexLock(&task->pool->mutex);
    const LdcTaskDependency output = task->output;
    task->output = kTaskDependencyInvalid;
    threadMutexUnlock(&task->pool->mutex);

    return output;
}

// Sliced task - can be used to defer the work from a parent.
//
typedef struct TaskWrapperSlicedDefer
{
    bool (*function)(void* argument, uint32_t offset, uint32_t count);
    bool (*completion)(void* argument, uint32_t count);

    uint8_t argument[1];
} TaskWrapperSlicedDefer;

static void* taskWrapperSlicedDefer(LdcTask* task, const LdcTaskPart* part)
{
    const TaskWrapperSlicedDefer* data = &VNTaskData(task, TaskWrapperSlicedDefer);

    data->function((void*)data->argument, part->start, part->count);

    return NULL;
}

static void* taskWrapperSlicedDeferComplete(LdcTask* task, const LdcTaskPart* part)
{
    VNUnused(part);

    const TaskWrapperSlicedDefer* data = &VNTaskData(task, TaskWrapperSlicedDefer);

    data->completion((void*)data->argument, part->start);

    return NULL;
}

bool ldcTaskPoolAddSlicedDeferred(LdcTaskPool* pool, LdcTask* parent,
                                  bool (*function)(void* argument, uint32_t offset, uint32_t count),
                                  bool (*completion)(void* argument, uint32_t count),
                                  void* argument, uint32_t argumentSize, uint32_t totalSize)
{
    assert(pool);

    LdcTaskGroup* group = NULL;
    const LdcTaskDependency* inputs = 0;
    uint32_t inputsCount = 0;
    LdcTaskDependency output = kTaskDependencyInvalid;

    if (parent) {
        assert(parent->group);
        // Take dependencies from parent task and lock until the new task has
        // been added to avoid other threads seeing the parent with no output
        threadMutexLock(&pool->mutex);
        group = parent->group;
        inputs = parent->inputs;
        inputsCount = parent->inputsCount;
        output = parent->output;
        parent->output = kTaskDependencyInvalid;
    }

    /// Append Sliced argument block onto the end of the task block
    uint32_t dataSize = sizeof(TaskWrapperSlicedDefer) + argumentSize;
    uint8_t* dataAllocation = alloca(dataSize);
    TaskWrapperSlicedDefer* data = (TaskWrapperSlicedDefer*)dataAllocation;
    data->function = function;
    data->completion = completion;
    memcpy(data->argument, argument, argumentSize);

    if (!parent) {
        threadMutexLock(&pool->mutex);
    }

    LdcTask* task = addTask(pool, group, inputs, inputsCount, output, taskWrapperSlicedDefer,
                            completion ? taskWrapperSlicedDeferComplete : NULL, totalSize,
                            totalSize, dataSize, dataAllocation, "slicedDefer");

    threadMutexUnlock(&pool->mutex);

    if (task == NULL) {
        return false;
    }

    // It is deferred from a parent ask - don't wait
    if (parent) {
        ldcTaskNoWait(task);
        return true;
    }

    // No parent - wait for task to finish
    if (!ldcTaskWait(task, NULL)) {
        VNLogError("ldcTaskWait failed");
    }
    return true;
}

// Debugging
//
#ifdef VN_SDK_LOG_ENABLE_DEBUG
static const char* depsSetAsString(char* dest, size_t destSize, const LdcTaskDependency* deps,
                                   uint32_t depsCount, const LdcTaskGroup* group)
{
    dest[0] = 0;

    char* cp = dest;

    if (!deps) {
        return dest;
    }

    for (uint32_t i = 0; i < depsCount; ++i) {
        const LdcTaskDependency dep = deps[i];
        cp += snprintf(cp, destSize - (cp - dest), "%d%s ", dep,
                       (group && dependencyMetBitGet(group, dep)) ? "*" : "");
    }

    return dest;
}

static void taskPoolDump(LdcTaskPool* pool, const LdcTaskGroup* group)
{
    static const char* stateNames[] = {
        "None", "Waiting", "Ready", "Running", "Blocked", "Done",
    };

    VNLogDebugF("Task Pool %p", (void*)pool);
    // Current threads
    VNLogDebugF("  Threads: %d", pool->threadCount);
    LdcTaskThread* threads = VNAllocationPtr(pool->threads, LdcTaskThread);
    for (uint32_t id = 0; id < pool->threadCount; ++id) {
        const char* name = threads[id].part.task
                               ? ((threads[id].part.task->name) ? threads[id].part.task->name : "")
                               : "";
        VNLogDebugF("  %d: %p (%d,%d) %s", id, (void*)threads[id].part.task, threads[id].part.start,
                    threads[id].part.count, name);
    }

    // Current tasks
    VNLogDebugF("  Tasks: allocated:%d pending:%d ready:%d", ldcVectorSize(&pool->tasks),
                pool->pendingTaskCount, ldcDequeSize(&pool->readyParts));
    for (uint32_t id = 0; id < ldcVectorSize(&pool->tasks); ++id) {
        LdcMemoryAllocation* taskAllocation = ldcVectorAt(&pool->tasks, id);
        LdcTask* task = VNAllocationPtr(*taskAllocation, LdcTask);
        if (group && task->group != group) {
            continue;
        }
        const char* taskName = task->name ? task->name : "";
        const char* groupName = task->group ? (task->group->name) ? task->group->name : "" : "";

        VNLogDebugF("  %3d: %p %7s Group:%p %s In:%08lx Out:%d Completed:%u Total:%d %s", id,
                    (void*)task, stateNames[task->state], (void*)task->group, groupName, task->inputs,
                    task->output, task->iterationsCompletedCount, task->iterationsTotalCount, taskName);
        if (task->inputs || task->output != kTaskDependencyInvalid) {
            char tmp[256];
            VNLogDebugF("     Inputs:[ %s] -> %d",
                        depsSetAsString(tmp, sizeof(tmp), task->inputs, task->inputsCount, task->group),
                        task->output);
        }
    }

    // Group
    if (group) {
        LdcTaskDependency* metDeps = alloca(group->dependenciesCount * sizeof(LdcTaskDependency));

        const char* groupName = group ? (group->name ? group->name : "") : "";

        VNLogDebugF("  Group: %p %s Tasks:%d Waiting:%d Dependencies:%d", (void*)group, groupName,
                    group->tasksCount, group->waitingTasksCount, group->dependenciesCount);

        uint32_t metDepsCount = 0;
        for (uint32_t i = 0; i < group->dependenciesCount; ++i) {
            if (dependencyMetBitGet(group, i)) {
                metDeps[metDepsCount++] = i;
            }
        }
        char tmp[256];
        VNLogDebugF("     Met:[ %s]", depsSetAsString(tmp, sizeof(tmp), metDeps, metDepsCount, 0));
    }
}

void ldcTaskPoolDump(LdcTaskPool* taskPool, const LdcTaskGroup* taskGroup)
{
    threadMutexLock(&taskPool->mutex);
    taskPoolDump(taskPool, taskGroup);
    threadMutexUnlock(&taskPool->mutex);
}
#endif
