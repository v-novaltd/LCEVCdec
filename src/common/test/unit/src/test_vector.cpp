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
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/vector.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

template <typename T, int RES = 100>
class Vector : public testing::Test
{
public:
    LdcMemoryAllocator* allocator;

    LdcVector vector{};

    Vector()
        : allocator(ldcMemoryAllocatorMalloc())
    {
        ldcVectorInitialize(&vector, RES, sizeof(T), allocator);
    }

    ~Vector() override {}

    Vector(const Vector&) = delete;
    Vector(Vector&&) = delete;
    void operator=(const Vector&) = delete;
    void operator=(Vector&&) = delete;

    std::minstd_rand randomGen;
};

using Vector_uint32 = Vector<uint32_t>;
TEST_F(Vector_uint32, Append)
{
    const int count = 50;
    // Append  integers
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorAppend(&vector, &i);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);

    // Check each element
    for (uint32_t i = 0; i < count; ++i) {
        ASSERT_EQ(i, *((uint32_t*)ldcVectorAt(&vector, i)));
    }
}

TEST_F(Vector_uint32, AppendGrow)
{
    const int count = 500;
    // Append integers
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorAppend(&vector, &i);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    // Check each element
    for (uint32_t i = 0; i < count; ++i) {
        ASSERT_EQ(i, *((uint32_t*)ldcVectorAt(&vector, i)));
    }
}

static int compare_uint32(const void* lhs, const void* rhs)
{
    const uint32_t a = *((uint32_t*)lhs);
    const uint32_t b = *((uint32_t*)rhs);
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

TEST_F(Vector_uint32, InsertForward)
{
    const int count = 500;
    // Insert integers in order
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &i);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    // Check each element
    for (uint32_t i = 0; i < count; ++i) {
        ASSERT_EQ(i, *((uint32_t*)ldcVectorAt(&vector, i)));
    }
}

TEST_F(Vector_uint32, InsertReverse)
{
    const int count = 500;
    // Insert integers in reverse order
    for (uint32_t i = count - 1; i != ~0u; --i) {
        ldcVectorInsert(&vector, compare_uint32, &i);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    // Check each element
    for (uint32_t i = 0; i < count; ++i) {
        ASSERT_EQ(i, *((uint32_t*)ldcVectorAt(&vector, i)));
    }
}

TEST_F(Vector_uint32, InsertShuffled)
{
    constexpr int count = 5000;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert elements
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    // Check each element
    for (uint32_t i = 0; i < count; ++i) {
        ASSERT_EQ(i, *((uint32_t*)ldcVectorAt(&vector, i)));
    }
}

TEST_F(Vector_uint32, InsertFind)
{
    constexpr int count = 5001;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert half elements
    for (uint32_t i = 0; i < count / 2; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count / 2);
    ASSERT_GE(ldcVectorReserved(&vector), count / 2);

    // Find each element
    for (uint32_t i = 0; i < count / 2; ++i) {
        uint32_t* ptr = (uint32_t*)ldcVectorFind(&vector, compare_uint32, &elements[i]);

        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(*ptr, elements[i]);
    }

    // Find other half - should not be found
    for (uint32_t i = count / 2; i < count; ++i) {
        uint32_t* ptr = (uint32_t*)ldcVectorFind(&vector, compare_uint32, &elements[i]);

        ASSERT_EQ(ptr, nullptr);
    }
}

TEST_F(Vector_uint32, InsertRemove)
{
    constexpr int count = 5000;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert elements
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Remove each element
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t* ptr = (uint32_t*)ldcVectorFind(&vector, compare_uint32, &elements[i]);
        ASSERT_NE(ptr, nullptr);
        ldcVectorRemove(&vector, ptr);
    }

    ASSERT_EQ(ldcVectorSize(&vector), 0);
}

TEST_F(Vector_uint32, InsertRemoveIdxInOrder)
{
    constexpr int count = 5000;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert elements
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Remove each element
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorRemoveIdx(&vector, 0);
    }

    ASSERT_EQ(ldcVectorSize(&vector), 0);
}

TEST_F(Vector_uint32, InsertRemoveIdx)
{
    constexpr int count = 5000;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert elements
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Remove each element
    for (uint32_t i = 0; i < count; ++i) {
        int idx = ldcVectorFindIdx(&vector, compare_uint32, &elements[i]);
        ASSERT_NE(idx, -1);
        ldcVectorRemoveIdx(&vector, idx);
    }

    ASSERT_EQ(ldcVectorSize(&vector), 0);
}

TEST_F(Vector_uint32, InsertRemoveReorder)
{
    constexpr int count = 5000;

    std::vector<uint32_t> elements(count);

    for (uint32_t i = 0; i < count; ++i) {
        elements[i] = i;
    }

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Insert elements
    for (uint32_t i = 0; i < count; ++i) {
        ldcVectorInsert(&vector, compare_uint32, &elements[i]);
    }

    // Check size
    ASSERT_EQ(ldcVectorSize(&vector), count);
    ASSERT_GE(ldcVectorReserved(&vector), count);

    std::shuffle(elements.begin(), elements.end(), randomGen);

    // Remove each element
    for (uint32_t i = 0; i < count; ++i) {
        void* ptr = ldcVectorFindUnordered(&vector, compare_uint32, &elements[i]);
        ASSERT_NE(ptr, nullptr);
        ldcVectorRemove(&vector, ptr);
    }

    ASSERT_EQ(ldcVectorSize(&vector), 0);
}
