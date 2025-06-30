/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.h>

#include <atomic>
#include <cstdio>

// Utility functions for testing with void*
namespace {
void* intToVoidPtr(int val) { return reinterpret_cast<void*>(static_cast<intptr_t>(val)); }
int intFromVoidPtr(void* ptr) { return static_cast<int>(reinterpret_cast<intptr_t>(ptr)); }
} // namespace

TEST(TaskPool, Create)
{
    LdcTaskPool taskPool;

    // Some threads, some reserved tasks
    ASSERT_TRUE(ldcTaskPoolInitialize(&taskPool, ldcMemoryAllocatorMalloc(),
                                      ldcMemoryAllocatorMalloc(), 8, 100));
    ldcTaskPoolDestroy(&taskPool);

    // No reserved tasks
    ASSERT_TRUE(ldcTaskPoolInitialize(&taskPool, ldcMemoryAllocatorMalloc(),
                                      ldcMemoryAllocatorMalloc(), 1, 0));
    ldcTaskPoolDestroy(&taskPool);
}

struct TestParam
{
    unsigned numThreads;
    unsigned numTasks;
    unsigned count;
};

class TaskPoolTest : public testing::TestWithParam<TestParam>
{
public:
    LdcMemoryAllocator* longTermAllocator{ldcMemoryAllocatorMalloc()};
    LdcMemoryAllocator* shortTermAllocator{ldcMemoryAllocatorMalloc()};

    LdcTaskPool taskPool{};

    TaskPoolTest()
    {
        EXPECT_TRUE(ldcTaskPoolInitialize(&taskPool, longTermAllocator, shortTermAllocator,
                                          GetParam().numThreads, GetParam().numTasks));
        ldcDiagnosticsLogLevel(LdcLogLevelInfo);
    }

    ~TaskPoolTest() override { ldcTaskPoolDestroy(&taskPool); }

    TaskPoolTest(const TaskPoolTest&) = delete;
    TaskPoolTest(TaskPoolTest&&) = delete;
    void operator=(const TaskPoolTest&) = delete;
    void operator=(TaskPoolTest&&) = delete;
};

TEST_P(TaskPoolTest, Tasks1)
{
    int count = 0;
    struct Data
    {
        int a;
        int b;
        int* countPtr;
    };
    Data data = {1, 2, &count};

    LdcTask* task = ldcTaskPoolAdd(
        &taskPool,
        [](LdcTask* thisTask, const LdcTaskPart* /*part*/) -> void* {
            auto& thisData = VNTaskData(thisTask, Data);

            EXPECT_EQ(thisData.a, 1);
            EXPECT_EQ(thisData.b, 2);

            EXPECT_EQ(*thisData.countPtr, 0);
            (*thisData.countPtr)++;

            // Scribble on data - should be a copy
            thisData.a = 3;
            thisData.b = 4;
            return intToVoidPtr(42);
        },
        NULL, 1, sizeof(data), &data, "test");

    EXPECT_NE(task, nullptr);

    void* output = nullptr;
    EXPECT_TRUE(ldcTaskWait(task, &output));

    // Check original data was not changes
    EXPECT_EQ(data.a, 1);
    EXPECT_EQ(data.b, 2);

    EXPECT_EQ(count, 1);

    // Did return value come back?
    EXPECT_EQ(output, intToVoidPtr(42));
}

// Same as reserved task count
TEST_P(TaskPoolTest, Tasks100)
{
    const int kNumTasks = 100;
    std::atomic_int taskCount = 0;

    LdcTask* tasks[kNumTasks] = {0};

    for (int i = 0; i < kNumTasks; ++i) {
        struct Data
        {
            int taskNum;
            std::atomic_int* countPtr;
        };
        Data data = {i, &taskCount};

        LdcTask* task = ldcTaskPoolAdd(
            &taskPool,
            [](LdcTask* thisTask, const LdcTaskPart* /*part*/) -> void* {
                auto& thisData = VNTaskData(thisTask, Data);
                thisData.countPtr->fetch_add(1);
                return intToVoidPtr(42 + thisData.taskNum);
            },
            NULL, 1, sizeof(data), &data, "test");

        EXPECT_NE(task, nullptr);

        tasks[i] = task;
    }

    for (int i = 0; i < kNumTasks; ++i) {
        void* output = nullptr;
        EXPECT_TRUE(ldcTaskWait(tasks[i], &output));
        EXPECT_EQ(output, intToVoidPtr(42 + i));
    }

    EXPECT_EQ(taskCount, kNumTasks);
}

// Larger than reserved task count
TEST_P(TaskPoolTest, Tasks)
{
    const int numTasks = GetParam().count;
    std::atomic_int taskCount = 0;

    std::vector<LdcTask*> tasks(numTasks);

    for (int i = 0; i < numTasks; ++i) {
        struct Data
        {
            int taskNum;
            std::atomic_int* countPtr;
        };
        Data data = {i, &taskCount};

        LdcTask* task = ldcTaskPoolAdd(
            &taskPool,
            [](LdcTask* thisTask, const LdcTaskPart* /*part*/) -> void* {
                auto& thisData = VNTaskData(thisTask, Data);
                thisData.countPtr->fetch_add(1);
                return intToVoidPtr(42 + thisData.taskNum);
            },
            NULL, 1, sizeof(data), &data, "test");

        EXPECT_NE(task, nullptr);

        tasks[i] = task;
    }

    for (int i = 0; i < numTasks; ++i) {
        void* output = nullptr;
        EXPECT_TRUE(ldcTaskWait(tasks[i], &output));
        EXPECT_EQ(output, intToVoidPtr(42 + i));
    }

    EXPECT_EQ(taskCount, numTasks);
}

TEST_P(TaskPoolTest, WaitMany)
{
    constexpr int kNumTasks = 30;
    std::atomic_int taskCount = 0;

    LdcTask* tasks[kNumTasks] = {nullptr};

    for (uint32_t i = 0; i < kNumTasks; ++i) {
        struct Data
        {
            uint32_t taskNum;
            std::atomic_int* countPtr;
        };
        Data data = {i, &taskCount};

        LdcTask* task = ldcTaskPoolAdd(
            &taskPool,
            [](LdcTask* thisTask, const LdcTaskPart* /*part*/) -> void* {
                auto& thisData = VNTaskData(thisTask, Data);
                thisData.countPtr->fetch_add(1);
                return intToVoidPtr(100 + thisData.taskNum);
            },
            NULL, 1, sizeof(data), &data, "waitmany");

        EXPECT_NE(task, nullptr);
        tasks[i] = task;
    }

    void* outputs[kNumTasks] = {nullptr};
    EXPECT_TRUE(ldcTaskWaitMany(tasks, kNumTasks, outputs));

    for (int i = 0; i < kNumTasks; ++i) {
        EXPECT_EQ(outputs[i], intToVoidPtr(100 + i));
    }

    EXPECT_EQ(taskCount, kNumTasks);
}

// Tasks that spawn other tasks

TEST_P(TaskPoolTest, SubTasks)
{
    const int numTasks = GetParam().count;
    static constexpr int kNumSubTasks = 120;

    std::atomic_int taskCount = 0;

    std::vector<LdcTask*> tasks(numTasks);

    for (int i = 0; i < numTasks; ++i) {
        struct Data
        {
            int taskNum;
            std::atomic_int* countPtr;
        };
        Data data = {i, &taskCount};

        LdcTask* task = ldcTaskPoolAdd(
            &taskPool,
            [](LdcTask* thisTask, const LdcTaskPart* /*part*/) -> void* {
                auto& thisData = VNTaskData(thisTask, Data);

                for (int j = 0; j < kNumSubTasks; ++j) {
                    struct SubData
                    {
                        int taskNum;
                        std::atomic_int* countPtr;
                    };
                    SubData subData = {j, thisData.countPtr};

                    LdcTask* subTask = ldcTaskPoolAdd(
                        thisTask->pool,
                        [](LdcTask* thisSubTask, const LdcTaskPart* /*part*/) -> void* {
                            auto& thisSubData = VNTaskData(thisSubTask, SubData);
                            thisSubData.countPtr->fetch_add(1);
                            return nullptr;
                        },
                        NULL, 1, sizeof(subData), &subData, "subtest");
                    EXPECT_NE(subTask, nullptr);
                    // Don't care about output of this task
                    ldcTaskNoWait(subTask);
                }

                return intToVoidPtr(42 + thisData.taskNum);
            },
            NULL, 1, sizeof(data), &data, "test");

        EXPECT_NE(task, nullptr);
        tasks[i] = task;
    }

    // Wait for all tasks in pool to finish.
    ldcTaskPoolWait(&taskPool);

    // Get output of top level tasks
    for (int i = 0; i < numTasks; ++i) {
        void* output = nullptr;
        EXPECT_TRUE(ldcTaskWait(tasks[i], &output));
        EXPECT_EQ(output, intToVoidPtr(42 + i));
    }

    // Is pool really empty?
    EXPECT_EQ(taskPool.tasks.size, 0);
    EXPECT_EQ(taskPool.pendingTaskCount, 0);

    // Check that the right number of subtask calls happened.
    EXPECT_EQ(taskCount, numTasks * kNumSubTasks);
}

//
TEST_P(TaskPoolTest, TaskGroupInit)
{
    LdcTaskGroup group;

    EXPECT_TRUE(ldcTaskGroupInitialize(&group, &taskPool, 10));
    ldcTaskGroupDestroy(&group);
}

//
struct TaskData
{
    int multiplier;
};

static void* groupTask(LdcTask* task, const LdcTaskPart* /*part*/)
{
    const TaskData& data = VNTaskData(task, TaskData);

    void* input = 0;
    ldcTaskCollectInputs(task, 1, &input);

    threadSleep(100);
    return intToVoidPtr(intFromVoidPtr(input) * data.multiplier);
}

TEST_P(TaskPoolTest, TaskGroupSimple)
{
    LdcTaskGroup group;

    EXPECT_TRUE(ldcTaskGroupInitialize(&group, &taskPool, 10));

    // Add a couple of pending tasks
    //
    LdcTaskDependency in1 = ldcTaskDependencyAdd(&group);
    LdcTaskDependency out1 = ldcTaskDependencyAdd(&group);
    TaskData task1Data = {37};
    ldcTaskGroupAdd(&group, &in1, 1, out1, groupTask, NULL, 1, 1, sizeof(task1Data), &task1Data, "test1");

    LdcTaskDependency in2 = out1;
    LdcTaskDependency out2 = ldcTaskDependencyAdd(&group);
    TaskData task2Data = {91};
    ldcTaskGroupAdd(&group, &in2, 1, out2, groupTask, NULL, 1, 1, sizeof(task2Data), &task2Data, "test2");

    // Satisfy the input
    ldcTaskDependencyMet(&group, in1, intToVoidPtr(42));

    void* result = ldcTaskDependencyWait(&group, out2);

    EXPECT_EQ(result, intToVoidPtr(42 * 37 * 91));

    ldcTaskGroupDestroy(&group);
}

//
struct TaskTreeData
{
    uint64_t expect;
};

static void* groupTreeTask(LdcTask* task, const LdcTaskPart* /*part*/)
{
    const TaskTreeData& data = VNTaskData(task, TaskTreeData);
    VNUnused(data);
    void* inputs[2] = {0};
    EXPECT_TRUE(ldcTaskCollectInputs(task, 2, inputs));

    int r = intFromVoidPtr(inputs[0]) * intFromVoidPtr(inputs[1]);

    threadSleep(100);
    return intToVoidPtr(r);
}

TEST_P(TaskPoolTest, TaskGroupTree)
{
    LdcTaskGroup group;

    EXPECT_TRUE(ldcTaskGroupInitialize(&group, &taskPool, 15));

    LdcTaskDependency inputs[8];
    LdcTaskDependency outputsRow1[4];
    LdcTaskDependency outputsRow2[2];

    for (int i = 0; i < 8; ++i) {
        inputs[i] = ldcTaskDependencyAdd(&group);
        EXPECT_NE(inputs[i], kTaskDependencyInvalid);
    }
    // Add a tree of pending tasks
    //
    for (int i = 0; i < 4; ++i) {
        LdcTaskDependency in1 = inputs[i * 2 + 0];
        LdcTaskDependency in2 = inputs[i * 2 + 1];
        LdcTaskDependency out = ldcTaskDependencyAdd(&group);
        EXPECT_NE(out, kTaskDependencyInvalid);

        outputsRow1[i] = out;

        TaskData taskTreeData = {i * 2 + 1 + i * 2 + 0};
        LdcTaskDependency taskInputs[] = {in1, in2};
        ldcTaskGroupAdd(&group, taskInputs, VNArraySize(taskInputs), out, groupTreeTask, NULL, 1, 1,
                        sizeof(taskTreeData), &taskTreeData, "test1");
    }

    for (int i = 0; i < 2; ++i) {
        LdcTaskDependency in1 = outputsRow1[i * 2 + 0];
        LdcTaskDependency in2 = outputsRow1[i * 2 + 1];
        LdcTaskDependency out = ldcTaskDependencyAdd(&group);
        EXPECT_NE(out, kTaskDependencyInvalid);

        outputsRow2[i] = out;

        TaskData taskTreeData = {i * 2 + 1 + i * 2 + 0};
        LdcTaskDependency taskInputs[] = {in1, in2};
        ldcTaskGroupAdd(&group, taskInputs, VNArraySize(taskInputs), out, groupTreeTask, NULL, 1, 1,
                        sizeof(taskTreeData), &taskTreeData, "test2");
    }

    LdcTaskDependency in1 = outputsRow2[0];
    LdcTaskDependency in2 = outputsRow2[1];
    LdcTaskDependency out = ldcTaskDependencyAdd(&group);
    EXPECT_NE(out, kTaskDependencyInvalid);

    TaskData taskTreeData = {132};
    LdcTaskDependency taskInputs[] = {in1, in2};
    ldcTaskGroupAdd(&group, taskInputs, VNArraySize(taskInputs), out, groupTreeTask, NULL, 1, 1,
                    sizeof(taskTreeData), &taskTreeData, "test3");

    // Satisfy the inputs
    for (int i = 0; i < 8; ++i) {
        ldcTaskDependencyMet(&group, inputs[i], intToVoidPtr(i + 1));
    }

    void* result = ldcTaskDependencyWait(&group, out);

    EXPECT_EQ(result, intToVoidPtr(40320));

    ldcTaskGroupDestroy(&group);
}

INSTANTIATE_TEST_SUITE_P(TaskPool, TaskPoolTest,
                         testing::Values(
                             // clang-format off
                             // numThreads,numTasks,count
                             TestParam{0, 10, 100},
                             TestParam{0, 100, 100},
                             TestParam{1, 100, 100},
                             TestParam{1, 100, 100},
                             TestParam{2, 100, 100},
                             TestParam{7, 100, 100},
                             TestParam{16, 1, 100},
                             TestParam{16, 10, 100},
                             TestParam{16, 1000, 100},
                             TestParam{9, 10, 543},
                             TestParam{13, 10, 1230}
                             // clang-format off
                             ));
