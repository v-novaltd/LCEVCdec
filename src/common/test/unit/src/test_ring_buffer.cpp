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

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <LCEVC/common/ring_buffer.h>
#include <LCEVC/common/threads.h>

class TestRingBuffer : public testing::Test
{
public:
    const uint32_t kRingSize = 8;

    TestRingBuffer() {}
    ~TestRingBuffer() override {}

    TestRingBuffer(const TestRingBuffer&) = delete;
    TestRingBuffer(TestRingBuffer&&) = delete;
    void operator=(const TestRingBuffer&) = delete;
    void operator=(TestRingBuffer&&) = delete;

    struct Element
    {
        uint32_t a;
        uint32_t b;
    };

    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
        ldcRingBufferInitialize(&ringBuffer, kRingSize, sizeof(Element), allocator);
    }

    void TearDown() override { ldcRingBufferDestroy(&ringBuffer); }

    LdcMemoryAllocator* allocator;
    LdcRingBuffer ringBuffer;
};

TEST_F(TestRingBuffer, CreateDestroy)
{
    EXPECT_EQ(ldcRingBufferCapacity(&ringBuffer), kRingSize - 1);
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 0);
    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);
}

TEST_F(TestRingBuffer, PushPop)
{
    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);

    constexpr Element e{1, 2};
    ldcRingBufferPush(&ringBuffer, &e);

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 1);

    Element ep = {0, 0};
    ldcRingBufferPop(&ringBuffer, &ep);

    EXPECT_EQ(e.a, ep.a);
    EXPECT_EQ(e.b, ep.b);

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 0);
}

TEST_F(TestRingBuffer, PushPopFull)
{
    for (uint32_t i = 0; i < kRingSize - 1; ++i) {
        const Element e{i, 2 * i};
        ldcRingBufferPush(&ringBuffer, &e);
    }
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 7);

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), true);

    {
        constexpr Element e{0, 0};
        EXPECT_FALSE(ldcRingBufferTryPush(&ringBuffer, &e));
    }

    for (uint32_t i = 0; i < kRingSize - 1; ++i) {
        Element e{0};
        ldcRingBufferPop(&ringBuffer, &e);
        EXPECT_EQ(e.a, i);
        EXPECT_EQ(e.b, 2 * i);
    }

    {
        Element e{0, 0};
        EXPECT_FALSE(ldcRingBufferTryPop(&ringBuffer, &e));
    }

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 0);
}

TEST_F(TestRingBuffer, PushPopFullWrapped)
{
    // Move halfway through ring
    for (uint32_t i = 0; i < kRingSize / 2; ++i) {
        constexpr Element e{0, 0};
        ldcRingBufferPush(&ringBuffer, &e);
    }

    for (uint32_t i = 0; i < kRingSize / 2; ++i) {
        Element e{0, 0};
        ldcRingBufferPop(&ringBuffer, &e);
    }
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 0);
    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);

    // Now fill ring, wrapping over end
    for (uint32_t i = 0; i < kRingSize - 1; ++i) {
        const Element e{i, 2 * i};
        ldcRingBufferPush(&ringBuffer, &e);
    }
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 7);

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), true);

    {
        constexpr Element e{0, 0};
        EXPECT_FALSE(ldcRingBufferTryPush(&ringBuffer, &e));
    }

    for (uint32_t i = 0; i < kRingSize - 1; ++i) {
        Element e{0};
        ldcRingBufferPop(&ringBuffer, &e);
        EXPECT_EQ(e.a, i);
        EXPECT_EQ(e.b, 2 * i);
    }

    {
        Element e{0, 0};
        EXPECT_FALSE(ldcRingBufferTryPop(&ringBuffer, &e));
    }

    EXPECT_EQ(ldcRingBufferIsEmpty(&ringBuffer), true);
    EXPECT_EQ(ldcRingBufferIsFull(&ringBuffer), false);
    EXPECT_EQ(ldcRingBufferSize(&ringBuffer), 0);
}
