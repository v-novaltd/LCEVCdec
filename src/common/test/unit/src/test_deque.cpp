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
#include <LCEVC/common/deque.h>

#include <deque>
#include <random>

class TestDeque : public testing::Test
{
public:
    const uint32_t kSize = 8;

    TestDeque() {}
    ~TestDeque() override {}

    TestDeque(const TestDeque&) = delete;
    TestDeque(TestDeque&&) = delete;
    void operator=(const TestDeque&) = delete;
    void operator=(TestDeque&&) = delete;

    struct Element
    {
        uint32_t a;
        uint32_t b;
    };

    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
        ldcDequeInitialize(&deque, kSize, sizeof(Element), allocator);
        gen.seed(42);
    }

    void TearDown() override { ldcDequeDestroy(&deque); }

    LdcMemoryAllocator* allocator;
    LdcDeque deque;

    std::minstd_rand gen;
    std::uniform_int_distribution<uint32_t> distribAction{1, 4};
    std::uniform_int_distribution<uint32_t> distribRepeat{1, 200};
    std::uniform_int_distribution<uint32_t> distribValue;
};

TEST_F(TestDeque, CreateDestroy)
{
    EXPECT_EQ(ldcDequeReserved(&deque), kSize - 1);
    EXPECT_EQ(ldcDequeSize(&deque), 0);
    EXPECT_EQ(ldcDequeIsEmpty(&deque), true);
    EXPECT_EQ(ldcDequeIsFull(&deque), false);
}

TEST_F(TestDeque, PushPop)
{
    EXPECT_EQ(ldcDequeIsEmpty(&deque), true);
    EXPECT_EQ(ldcDequeIsFull(&deque), false);

    {
        const Element e1{1, 2};
        const Element e2{3, 4};
        ldcDequeBackPush(&deque, &e1);
        ldcDequeBackPush(&deque, &e2);

        EXPECT_EQ(ldcDequeIsEmpty(&deque), false);
        EXPECT_EQ(ldcDequeIsFull(&deque), false);
        EXPECT_EQ(ldcDequeSize(&deque), 2);

        Element ep = {0, 0};
        ldcDequeFrontPop(&deque, &ep);
        EXPECT_EQ(e1.a, ep.a);
        EXPECT_EQ(e1.b, ep.b);

        ldcDequeFrontPop(&deque, &ep);
        EXPECT_EQ(e2.a, ep.a);
        EXPECT_EQ(e2.b, ep.b);
    }

    {
        const Element e1{5, 6};
        const Element e2{7, 8};
        const Element e3{9, 20};
        ldcDequeFrontPush(&deque, &e1);
        ldcDequeFrontPush(&deque, &e2);
        ldcDequeFrontPush(&deque, &e3);

        EXPECT_EQ(ldcDequeIsEmpty(&deque), false);
        EXPECT_EQ(ldcDequeIsFull(&deque), false);
        EXPECT_EQ(ldcDequeSize(&deque), 3);

        Element ep = {0, 0};
        ldcDequeBackPop(&deque, &ep);
        EXPECT_EQ(e1.a, ep.a);
        EXPECT_EQ(e1.b, ep.b);

        ldcDequeBackPop(&deque, &ep);
        EXPECT_EQ(e2.a, ep.a);
        EXPECT_EQ(e2.b, ep.b);

        ldcDequeBackPop(&deque, &ep);
        EXPECT_EQ(e3.a, ep.a);
        EXPECT_EQ(e3.b, ep.b);
    }

    EXPECT_EQ(ldcDequeIsEmpty(&deque), true);
    EXPECT_EQ(ldcDequeIsFull(&deque), false);
    EXPECT_EQ(ldcDequeSize(&deque), 0);
}

TEST_F(TestDeque, Grow)
{
    for (uint32_t i = 0; i < kSize * 4; ++i) {
        const Element e{1, i};
        ldcDequeFrontPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize * 4; ++i) {
        const Element e{2, i};
        ldcDequeBackPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize * 4; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), true);
        EXPECT_EQ(e.a, 1);
        EXPECT_EQ(e.b, (kSize * 4 - 1) - i);
    }

    for (uint32_t i = 0; i < kSize * 4; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), true);
        EXPECT_EQ(e.a, 2);
        EXPECT_EQ(e.b, i);
    }

    {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), false);
    }
}

TEST_F(TestDeque, GrowWrapped1)
{
    for (uint32_t i = 0; i < kSize / 2; ++i) {
        const Element e{1, i};
        ldcDequeFrontPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize * 4; ++i) {
        const Element e{2, i};
        ldcDequeBackPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize / 2; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), true);
        EXPECT_EQ(e.a, 1);
        EXPECT_EQ(e.b, (kSize / 2 - 1) - i);
    }

    for (uint32_t i = 0; i < kSize * 4; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), true);
        EXPECT_EQ(e.a, 2);
        EXPECT_EQ(e.b, i);
    }

    {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), false);
    }
}

TEST_F(TestDeque, GrowWrapped2)
{
    for (uint32_t i = 0; i < kSize * 4; ++i) {
        const Element e{2, i};
        ldcDequeBackPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize / 2; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeBackPop(&deque, &e), true);
        EXPECT_EQ(e.a, 2);
        EXPECT_EQ(e.b, (kSize * 4 - 1) - i);
    }

    for (uint32_t i = 0; i < kSize / 2 + 1; ++i) {
        const Element e{1, i};
        ldcDequeFrontPush(&deque, &e);
    }

    for (uint32_t i = 0; i < kSize / 2 + 1; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), true);
        EXPECT_EQ(e.a, 1);
        EXPECT_EQ(e.b, (kSize / 2) - i);
    }

    for (uint32_t i = 0; i < kSize * 4 - kSize / 2; ++i) {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeBackPop(&deque, &e), true);
        EXPECT_EQ(e.a, 2);
        EXPECT_EQ(e.b, (kSize * 4 - kSize / 2 - 1) - i);
    }

    {
        Element e{0, 0};
        EXPECT_EQ(ldcDequeFrontPop(&deque, &e), false);
    }
}

// Random walk of repeated pushes and pops - match against a reference std::deque
TEST_F(TestDeque, Random)
{
    const uint32_t kCount = 10000;
    std::deque<uint32_t> reference;

    for (uint32_t i = 0; i < kCount; ++i) {
        const uint32_t repeat = distribRepeat(gen);
        const uint32_t action = distribAction(gen);

        for (uint32_t j = 0; j < repeat; ++j) {
            switch (action) {
                case 1: {
                    // Push Front
                    Element e{3, distribValue(gen)};
                    ldcDequeFrontPush(&deque, &e);
                    reference.push_front(e.b);
                    break;
                }
                case 2: {
                    // Push Back
                    Element e{3, distribValue(gen)};
                    ldcDequeBackPush(&deque, &e);
                    reference.push_back(e.b);
                    break;
                }
                case 3: {
                    // Pop Front
                    if (ldcDequeIsEmpty(&deque)) {
                        EXPECT_EQ(reference.size(), 0);
                    } else {
                        Element e{0, 0};
                        EXPECT_TRUE(ldcDequeFrontPop(&deque, &e));
                        EXPECT_EQ(3, e.a);
                        EXPECT_EQ(reference.front(), e.b);
                        reference.pop_front();
                    }
                    break;
                }
                case 4: {
                    // Pop Back
                    if (ldcDequeIsEmpty(&deque)) {
                        EXPECT_EQ(reference.size(), 0);
                    } else {
                        Element e{0, 0};
                        EXPECT_TRUE(ldcDequeBackPop(&deque, &e));
                        EXPECT_EQ(3, e.a);
                        EXPECT_EQ(reference.back(), e.b);
                        reference.pop_back();
                    }
                    break;
                }
            }
        }
        EXPECT_EQ(ldcDequeSize(&deque), reference.size());
    }
}
