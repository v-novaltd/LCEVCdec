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

#ifndef VN_LCEVC_COMMON_TASK_POOL_H
#define VN_LCEVC_COMMON_TASK_POOL_H

// NOLINTBEGIN(modernize-use-using)

/*! @file
 *  @brief A general threaded task runner.
 *
 * Currently, operates a simple FIFO of tasks, consumed by threads, but it is designed such that
 * it can be improved to use Lazy Binary-Splitting (https://terpconnect.umd.edu/~barua/ppopp164.pdf)
 */
#include <LCEVC/build_config.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Forward declarations -- these types are broadly treated as opaque.
 */
typedef struct LdcTaskPool LdcTaskPool;
typedef struct LdcTask LdcTask;
typedef struct LdcTaskGroup LdcTaskGroup;

/*! Passed to taskFunction to describe the task and which iterations it is responsible for.
 */
typedef struct LdcTaskPart
{
    LdcTask* task;  // Task of which this is a part
    uint32_t start; // First iteration in this part of task
    uint32_t count; // Number of iterations in this part
} LdcTaskPart;

/*! Identifies one of the dependencies in a group
 *
 * This avoids those names because it pulls in a lot of conceptual complexity.
 */
typedef uint32_t LdcTaskDependency;

#define kTaskDependencyInvalid (~0U) // NOLINT

/*! Maximum number of dependencies within a task group.
 */
#define kTaskPoolMaxDependencies 16384 // NOLINT

/*! Task work function pointer
 */
typedef void* (*LdcTaskFunction)(LdcTask* task, const LdcTaskPart* part);

/*! Initialize the task pool.
 *
 *  @param[out]     taskPool            The TaskPool to initialize.
 *  @param[in]      longTermAllocator   The memory allocator to use for pool lifetime
 *  @param[in]      shortTermAllocator  The memory allocator to use for task/group lifetime
 *  @param[in]      threadsCount        The number of threads to use within the pool (excluding
 *                                      calling thread).
 *  @param[in]      reservedTaskCount   The number of tasks slots to reserve at start.
 *
 * If threadsCount == 0, then the pool will not spawn any extra threads, and will call the
 * task functions deterministically on the caller thread as soon as the are ready during
 * ldcTaskPoolAdd() or ldcTaskDependencyMet().
 *
 *  @return                             True on success
 */
bool ldcTaskPoolInitialize(LdcTaskPool* taskPool, LdcMemoryAllocator* longTermAllocator,
                           LdcMemoryAllocator* shortTermAllocator, uint32_t threadsCount,
                           uint32_t reservedTaskCount);

/*! Release the task pool.
 *
 *  @param[in]      taskPool        The TaskPool to destroy.
 */
void ldcTaskPoolDestroy(LdcTaskPool* taskPool);

/*! Add a new stand alone task to the pool with no dependencies
 *
 *  @param[in]      taskPool            The task pool the task to be added to.
 *  @param[in]      taskFunction        The function to call (possibly repeatedly for many iterations) to execute the task.
 *  @param[in]      completionFunction  The function to call one task has been executed.
 *  @param[in]      iterations          The number of 'things' that comprise this task.
 *  @param[in]      dataSize            Size in bytes of task specific input that that is copied into task.
 *  @param[in]      data                Task specific data to copy into task.
 *  @param[in]      name                The tasks name - for logging.
 *
 *  @return                             Pointer to task block of new task, or NULL if failed.
 */
LdcTask* ldcTaskPoolAdd(LdcTaskPool* taskPool, LdcTaskFunction taskFunction,
                        LdcTaskFunction completionFunction, uint32_t iterations, size_t dataSize,
                        const void* data, const char* name);

/*! Add a new task to the pool that is part of a group connected with dependencies
 *
 * This is the base task function supported by the pool - a function with some parameters and
 * a number of iterations. There are various `taskPoolAddXXX()` functions that use wrapper
 * functions support various more specialised operations.
 *
 * The task does not need clearing by lcdTaskWait() - any outputs can be picked up from the group.
 *
 *  @param[in]      taskGroup               The TaskGroup that this tasks belongs to
 *  @param[in]      inputs                  The set of input dependencies within group required before this task can start.
 *  @param[in]      inputsCount             The number of inputs.
 *  @param[in]      output                  The output dependency within the group that is resolved by this task.
 *  @param[in]      taskFunction            The function to call (possibly repeatedly for many iterations) to execute the task.
 *  @param[in]      completionFunction      The function to call one task has been executed.
 *  @param[in]      iterations              The number of 'things' that comprise this task.
 *  @param[in]      maxIterationsPerPart    The maximum number of 'things' that can be executed in each call to taskFunction.
 *  @param[in]      dataSize                Size in bytes of task specific input that that is copied into task.
 *  @param[in]      data                    Task specific data to copy into task.
 *  @param[in]      name                    The groups name - for logging.
 *
 *  @return                                 True if successful.
 */
bool ldcTaskGroupAdd(LdcTaskGroup* taskGroup, const LdcTaskDependency* inputs, uint32_t inputsCount,
                     LdcTaskDependency output, LdcTaskFunction taskFunction,
                     LdcTaskFunction completionFunction, uint32_t iterations,
                     uint32_t maxIterationsPerPart, size_t dataSize, const void* data, const char* name);

/*! Wait until all pending tasks in pool are complete
 *
 */
void ldcTaskPoolWait(LdcTaskPool* taskPool);

/*! Wait until a given task is complete
 *
 *  @param[in]      task                    Pointer to the task to wait for, or NULL.
 *  @param[in]      outputPtr               If not NULL, a pointer to where to store the output of the task.
 *
 *  If the task is NULL, this will return false immediately - allowing something like:
 *
 *     if(!ldCTaskWait(ldcTaskPoolAdd(....))) { ... }
 *
 *  If the return is true the task pointer will no longer be valid - the task block will have ben cleared up.
 *
 *  @return                                 True if task completed, or NULL if task pointer was NULL
 */
bool ldcTaskWait(LdcTask* task, void** outputPtr);

/*! Wait until a given set of tasks is complete
 *
 *  @param[in]      tasks                   Pointer to an array of task pointers.
 *  @param[in]      tasksCount              Number of task pointers in `tasks` array.
 *  @param[out]     outputPtrs              Pointer to an array of `tasksCount` void * where output
 *                                          of completed tasks will be written.
 *
 *  @return                                 True if all task completed and return not NULL.
 */
bool ldcTaskWaitMany(LdcTask** tasks, uint32_t tasksCount, void** outputPtrs);

/*! Indicate that tasks will not be waited for, and can be cleared up once it has been executed.
 *
 *  @param[in]      task                    Pointer to the task to mark as detached
 *
 * NB: the task pointer will not be valid after this call, as it could be executed and freed already.
 */
void ldcTaskNoWait(LdcTask* task);

/*! Initialize a task group
 *
 *  @param[out]     taskGroup               The task group to initialize
 *  @param[in]      taskPool                The task pool that this group is will live in.
 *  @param[in]      maxDependenciesCount    The maximum number of dependencies that can exist in this group.
 *
 *  If the return is true the task pointer will no longer be valid - the task block will have ben cleared up.
 *
 *  @return                                 True on success
 */
bool ldcTaskGroupInitialize(LdcTaskGroup* taskGroup, LdcTaskPool* taskPool, uint32_t maxDependenciesCount);

/*! Release the task group
 *
 *  @param[in]      taskGroup        The TaskGroup to destroy.
 */
void ldcTaskGroupDestroy(struct LdcTaskGroup* taskGroup);

/*! Wait for group to have no remaining tasks
 *
 *  @param[in]      taskGroup        The TaskGroup to wait for.
 */
void ldcTaskGroupWait(struct LdcTaskGroup* taskGroup);

/*! Get the number of waiting tasks in group
 *
 *  @param[in]      taskGroup        The TaskGroup to query.
 *
 *  @return                          The number of waiting tasks remaining in group
 */
uint32_t ldcTaskGroupGetWaitingCount(const struct LdcTaskGroup* taskGroup);

/*! Get the number of tasks remaining in group, and optionally, number of blocked tasks
 *
 * The two counts will be from the same point in time.
 *
 *  @param[in]      taskGroup        The TaskGroup to query.
 *  @param[out]     waiting          If not NULL, a pointer to where the number of tasks that are
 *                                   still waiting for dependencies is written.
 *
 *  @return                          THe number of tasks remaining in group
 */
uint32_t ldcTaskGroupGetTaskCount(const struct LdcTaskGroup* taskGroup, uint32_t* waiting);

/*! Get the remaining dependencies that are outputs of tasks that all depend on a given input
 *
 * The input should not have been marked as 'met' yet.
 *
 *  Used to find out the dependencies that when met, mean that the input is no longer needed.
 *
 *  @param[in]      taskGroup        The TaskGroup to query.
 *  @param[in]      dep              The input dependency of interest
 *  @param[out]     outputs          Buffer to populate with dependent task groups
 *  @param[in]      outputsMax       Max size of `output` buffer
 *  @param[out]     outputsCount     Number of dependencies populated into `outputs`
 *
 *  @return                          THe set of output dependencies that are produced from the input.
 */
bool ldcTaskGroupFindOutputSetFromInput(const struct LdcTaskGroup* taskGroup, LdcTaskDependency dep,
                                        LdcTaskDependency* outputs, uint32_t outputsMax,
                                        uint32_t* outputsCount);

/*! Allocate the next dependency index, and mark it as unmet
 *
 *  @param[in]      taskGroup   The task group from which to allocate a dependency.
 *
 *  @return                     The new dependency - marked as unmet, or kTaskDependencyInvalid if it failed
 */
LdcTaskDependency ldcTaskDependencyAdd(LdcTaskGroup* taskGroup);

/*! Supply a value of a dependency, and mark it as met.
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      dependency  The dependency with the group that has been met.
 *  @param[in]      value       The data to attach to the met dependency.
 */
void ldcTaskDependencyMet(LdcTaskGroup* taskGroup, LdcTaskDependency dependency, void* value);

/*! Allocate the next dependency index, and mark it as met, with the given value
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      value       The data to attach to the met dependency
 *
 *  @return                     The new dependency - marked as met with given value, or kTaskDependencyInvalid if it failed
 */
LdcTaskDependency ldcTaskDependencyAddMet(LdcTaskGroup* taskGroup, void* value);

/*! See if given dependency has been met.
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      dependency  The dependency with the group to query
 *
 * @return                      True if the given dependency has been met.
 */
bool ldcTaskDependencyIsMet(const LdcTaskGroup* taskGroup, LdcTaskDependency dependency);

/*! See if a give set of dependencies have been met
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      deps        Pointer to an array of dependencies
 *  @param[in]      depsCount   The count of dependencies pointed at by `deps`
 *
 *  @return                      True if all the given dependencies have been ,et.
 */
bool ldcTaskDependencySetIsMet(const LdcTaskGroup* taskGroup, const LdcTaskDependency* deps,
                               uint32_t depsCount);

/*! Get the value of a met dependency - will cause an error if the dependency has not been set
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      dependency  The dependency to query.
 *
 *  @return                     The value supplied to ldcTaskDependencyMet() or ldcTaskDependencyAddMet()
 */
void* ldcTaskDependencyGet(const LdcTaskGroup* taskGroup, LdcTaskDependency dependency);

/*! Get the value of a dependency - will wait unit it is met if necessary
 *
 *  @param[in]      taskGroup   The task group containing the dependency.
 *  @param[in]      dependency  The dependency to query.
 *
 *  @return                     The value supplied to ldcTaskDependencyMet() or ldcTaskDependencyAddMet()
 */
void* ldcTaskDependencyWait(const LdcTaskGroup* taskGroup, LdcTaskDependency dependency);

/*! Get the inputs of a task
 *
 * Collect all the 'void *' pointers from the met dependencies association with the
 * task's inputs.
 *
 *  @param[in]      task        The task
 *  @param[in]      numInputs   Number of inputs
 *  @param[in]      inputs      Array of pointers to the inputs
 *
 *  @return                     true if the inputs were available
 */
bool ldcTaskCollectInputs(const LdcTask* task, size_t numInputs, void** inputs);

/*! Clear a task's output and return it's previous value.
 *
 * This is typically used to allow one task to defer output generation to another newly add task.
 *
 *  @param[in]      task        Pointer to the task whose output will be cleared
 *
 *  @return                     The task's previous output dependency.
 */
LdcTaskDependency ldcTaskRemoveOutput(LdcTask* task);

/*! A number of slices - all given same base argument, with per-slide offset and count
 *
 *  Creates the task with the given group and output dependency, and returns immediately.
 *
 *  @param[in]     pool         The task pool to be added to.
 *  @param[in]     parent       If not NULL, task whose dependencies will be inherited.
 *  @param[in]     function     Function to call during task.
 *  @param[in]     completion   If not NULL, function to call at end of task.
 *  @param[in]     argument     Data that is copied and passed as an argument to `function` and `completion`
 *  @param[in]     argumentSize Size in bytes of argument data to be copied.
 *  @param[in]     totalSize    Number of iterations for this task.
 *
 *  @return                     True if task was successfully created.
 */
bool ldcTaskPoolAddSlicedDeferred(LdcTaskPool* pool, LdcTask* parent,
                                  bool (*function)(void* argument, uint32_t offset, uint32_t count),
                                  bool (*completion)(void* argument, uint32_t count),
                                  void* argument, uint32_t argumentSize, uint32_t totalSize);

/*! Block a task group - will stop new tasks being scheduled
 *
 * This can be used to inspect the full task graph for a group, with the
 * scheduler diving and removing steps.
 *
 *  @param[in]      taskGroup   The task group to block.
 */
void ldcTaskGroupBlock(LdcTaskGroup* taskGroup);

/*! Unblock a task group - all pending tasks will be scheduled
 *
 *  @param[in]      taskGroup   The task group to unblock.
 */
void ldcTaskGroupUnblock(LdcTaskGroup* taskGroup);

#ifdef VN_SDK_LOG_ENABLE_DEBUG

/*! Utility function to dump state of task pool to log
 *
 *  @param[in]      taskPool    The task pool to dump
 *  @param[in]      taskGroup   If not NULL, restict to tasks within group
 */
void ldcTaskPoolDump(LdcTaskPool* taskPool, const LdcTaskGroup* taskGroup);
#endif

/*! Utility macro to get a reference to the task data pointer as a particular type.
 *
 *  @param[in]      task        The task to get data for
 *  @param[in]      type        The type (typically a struct) to cast the type data pointer to.

 */
#if !defined(__cplusplus)
#define VNTaskData(task, type) (*((type*)(task)->data))
#else
// NB: clang-tidy wants 'type' to be parenthesized - not valid C++.
#define VNTaskData(task, type) \
    (*(reinterpret_cast<type*>((task)->data))) // NOLINT(bugprone-macro-parentheses,cppcoreguidelines-pro-type-reinterpret-cast)
#endif

#include "detail/task_pool.h"

#ifdef __cplusplus
}
#endif

// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_COMMON_TASK_POOL_H
