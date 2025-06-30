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

#include <gtest/gtest.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.h>

namespace {

void* intToPtr(int v) { return reinterpret_cast<void*>(static_cast<intptr_t>(v)); }
int ptrToInt(void* p) { return static_cast<int>(reinterpret_cast<intptr_t>(p)); }

struct IncData
{
    int value;
};

static void* incTask(LdcTask* task, const LdcTaskPart*)
{
    const IncData& data = VNTaskData(task, IncData);
    void* input = nullptr;
    EXPECT_TRUE(ldcTaskCollectInputs(task, 1, &input));
    return intToPtr(ptrToInt(input) + data.value);
}

} // namespace

TEST(TaskGroup, DependencyReserveGrowth)
{
    const uint32_t kStartCount = 2;
    const uint32_t kNumTasks = 32;

    LdcTaskPool pool;
    ASSERT_TRUE(ldcTaskPoolInitialize(&pool, ldcMemoryAllocatorMalloc(), ldcMemoryAllocatorMalloc(), 1, 8));

    LdcTaskGroup group;
    ASSERT_TRUE(ldcTaskGroupInitialize(&group, &pool, kStartCount));

    EXPECT_EQ(group.dependenciesReserved, kStartCount);
    EXPECT_EQ(group.dependenciesCount, 0);

    // An input dependency for each task, and a final output dependency
    LdcTaskDependency deps[kNumTasks + 1];
    for (uint32_t i = 0; i < kNumTasks + 1; ++i) {
        deps[i] = ldcTaskDependencyAdd(&group);
        EXPECT_NE(deps[i], kTaskDependencyInvalid);
    }

    EXPECT_EQ(group.dependenciesCount, kNumTasks + 1);
    EXPECT_GE(group.dependenciesReserved, kNumTasks + 1);
    EXPECT_LE(group.dependenciesReserved, kNumTasks * 2);

    for (uint32_t i = 0; i < kNumTasks; ++i) {
        IncData data{1};
        LdcTaskDependency in = deps[i];
        LdcTaskDependency out = deps[i + 1];
        EXPECT_TRUE(ldcTaskGroupAdd(&group, &in, 1, out, incTask, NULL, 1, 1, sizeof(data), &data, "inc"));
    }

    // Poke first dependency ...
    ldcTaskDependencyMet(&group, deps[0], intToPtr(0));

    // ... and wait for last
    void* result = ldcTaskDependencyWait(&group, deps[kNumTasks]);
    EXPECT_EQ(ptrToInt(result), kNumTasks);

    EXPECT_EQ(ldcTaskDependencyGet(&group, deps[kNumTasks]), result);

    uint32_t waiting = 0;
    EXPECT_EQ(ldcTaskGroupGetTaskCount(&group, &waiting), 0);
    EXPECT_EQ(waiting, 0);

    ldcTaskGroupDestroy(&group);
    ldcTaskPoolDestroy(&pool);
}
