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
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <set>

struct TestParam
{
    unsigned numThreads;
    unsigned numTasks;
    unsigned count;
};

class TaskPoolWrapperTest : public testing::TestWithParam<TestParam>
{
public:
    LdcMemoryAllocator* longTermAllocator{ldcMemoryAllocatorMalloc()};
    LdcMemoryAllocator* shortTermAllocator{ldcMemoryAllocatorMalloc()};

    LdcTaskPool taskPool{};

    uint32_t someData{0x12345678};

    std::atomic<uint32_t> callCount;
    std::set<uint32_t> calledSet;
    std::set<uint32_t> completedSet;
    std::mutex mutex;

    TaskPoolWrapperTest()
    {
        EXPECT_TRUE(ldcTaskPoolInitialize(&taskPool, longTermAllocator, shortTermAllocator,
                                          GetParam().numThreads, GetParam().numTasks));
    }

    ~TaskPoolWrapperTest() override { ldcTaskPoolDestroy(&taskPool); }

    TaskPoolWrapperTest(const TaskPoolWrapperTest&) = delete;
    TaskPoolWrapperTest(TaskPoolWrapperTest&&) = delete;
    void operator=(const TaskPoolWrapperTest&) = delete;
    void operator=(TaskPoolWrapperTest&&) = delete;
};

struct SlicedTaskData
{
    TaskPoolWrapperTest* self;
    uint32_t value;
    uint32_t count;
};

static bool slicedFn(void* argument, uint32_t offset, uint32_t count)
{
    EXPECT_NE(argument, nullptr);
    const SlicedTaskData& data = *(SlicedTaskData*)argument;
    EXPECT_EQ(data.value, 0xf00dfade);
    TaskPoolWrapperTest* self = data.self;
    EXPECT_EQ(self->someData, 0x12345678);

    {
        std::scoped_lock lock(self->mutex);

        for (uint32_t i = offset; i < offset + count; ++i) {
            EXPECT_EQ(self->calledSet.count(i), 0);
            self->calledSet.insert(i);
        }
    }
    return true;
}

static bool completionFn(void* argument, uint32_t count)
{
    EXPECT_NE(argument, nullptr);
    const SlicedTaskData& data = *(SlicedTaskData*)argument;
    EXPECT_EQ(data.value, 0xf00dfade);
    TaskPoolWrapperTest* self = data.self;
    EXPECT_EQ(self->someData, 0x12345678);

    EXPECT_EQ(data.count, count);

    {
        std::scoped_lock lock(self->mutex);

        for (uint32_t i = 0; i < count; ++i) {
            EXPECT_EQ(self->calledSet.count(i), 1);
            EXPECT_EQ(self->completedSet.count(i), 0);
            self->completedSet.insert(i);
        }
    }

    return true;
}

TEST_P(TaskPoolWrapperTest, Sliced)
{
    const uint32_t count = GetParam().count;
    SlicedTaskData data = {this, 0xf00dfade, count};

    const bool r =
        ldcTaskPoolAddSlicedDeferred(&taskPool, NULL, slicedFn, NULL, &data, sizeof(data), count);
    EXPECT_EQ(taskPool.pendingTaskCount, 0);
    EXPECT_TRUE(r);

    // visited whole domain?
    EXPECT_EQ(calledSet.size(), count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_EQ(calledSet.count(i), 1);
    }
}

TEST_P(TaskPoolWrapperTest, SlicedWithCompletion)
{
    const uint32_t count = GetParam().count;
    SlicedTaskData data = {this, 0xf00dfade, count};

    const bool r = ldcTaskPoolAddSlicedDeferred(&taskPool, NULL, slicedFn, completionFn, &data,
                                                sizeof(data), count);

    EXPECT_EQ(taskPool.pendingTaskCount, 0);
    EXPECT_TRUE(r);

    // visited whole domain?
    EXPECT_EQ(calledSet.size(), count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_EQ(calledSet.count(i), 1);
    }

    // completed whole domain?
    EXPECT_EQ(completedSet.size(), count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_EQ(completedSet.count(i), 1);
    }
}

INSTANTIATE_TEST_SUITE_P(TaskPoolWrapper, TaskPoolWrapperTest,
                         testing::Values(
                             // clang-format off
                             // numThreads,numTasks,count
                             TestParam{1, 100, 1},
                             TestParam{1, 100, 2},
                             TestParam{1, 100, 4000},
                             TestParam{4, 100, 1},
                             TestParam{4, 100, 2},
                             TestParam{4, 100, 3},
                             TestParam{4, 100, 4},
                             TestParam{4, 100, 5},
                             TestParam{4, 100, 6},
                             TestParam{4, 100, 10},
                             TestParam{4, 100, 11},
                             TestParam{4, 100, 17},
                             TestParam{4, 100, 150},
                             TestParam{4, 100, 1001},
                             TestParam{255, 100, 1},
                             TestParam{255, 100, 6},
                             TestParam{255, 100, 10},
                             TestParam{255, 100, 17},
                             TestParam{255, 100, 150},
                             TestParam{255, 100, 254},
                             TestParam{255, 100, 255},
                             TestParam{255, 100, 256},
                             TestParam{255, 100, 1001} // clang-format on
                             ));
