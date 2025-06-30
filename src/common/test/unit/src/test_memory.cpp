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
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>

#include <climits>

class MemoryTest : public testing::Test
{
public:
    LdcMemoryAllocator* allocator;

    MemoryTest()
        : allocator(ldcMemoryAllocatorMalloc())
    {}

    ~MemoryTest() override {}

    MemoryTest(const MemoryTest&) = delete;
    MemoryTest(MemoryTest&&) = delete;
    void operator=(const MemoryTest&) = delete;
    void operator=(MemoryTest&&) = delete;
};

struct S
{
    int a;
    int b;
    int c;
    int d;
};

TEST_F(MemoryTest, Allocate)
{
    LdcMemoryAllocation memi = {0};
    int* ptri = VNAllocate(allocator, &memi, int);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    memset(memi.ptr, 42, sizeof(int));
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems = {0};
    S* ptrs = VNAllocate(allocator, &mems, S);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    memset(mems.ptr, 0, sizeof(S));
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);
}

TEST_F(MemoryTest, AllocateArray)
{
    const int arraySize = 1000;

    LdcMemoryAllocation memi = {0};

    int* ptri = VNAllocateArray(allocator, &memi, int, arraySize);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    memset(memi.ptr, 42, sizeof(int) * arraySize);
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems = {0};
    S* ptrs = VNAllocateArray(allocator, &mems, S, arraySize);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    memset(mems.ptr, 42, sizeof(S) * arraySize);
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);
}

TEST_F(MemoryTest, AllocateZero)
{
    LdcMemoryAllocation memi = {0};
    int* ptri = VNAllocateZero(allocator, &memi, int);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    EXPECT_EQ(*ptri, 0);
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems = {0};
    S* ptrs = VNAllocateZero(allocator, &mems, S);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    EXPECT_EQ(ptrs->a, 0);
    EXPECT_EQ(ptrs->b, 0);
    EXPECT_EQ(ptrs->c, 0);
    EXPECT_EQ(ptrs->d, 0);
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);
}

TEST_F(MemoryTest, AllocateZeroArray)
{
    const int arraySize = 1000;
    LdcMemoryAllocation memi = {0};

    int* ptri = VNAllocateZeroArray(allocator, &memi, int, arraySize);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    for (int i = 0; i < arraySize; ++i) {
        EXPECT_EQ(ptri[i], 0);
    }
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems = {0};
    S* ptrs = VNAllocateZeroArray(allocator, &mems, S, arraySize);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    for (int i = 0; i < arraySize; ++i) {
        EXPECT_EQ(ptrs[i].a, 0);
        EXPECT_EQ(ptrs[i].b, 0);
        EXPECT_EQ(ptrs[i].c, 0);
        EXPECT_EQ(ptrs[i].d, 0);
    }
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);
}

TEST_F(MemoryTest, AllocateAligned)
{
    const size_t blockSize = 4096;

    // Check from single byte up to 64k alignment
    for (int i = 0; i < 16; ++i) {
        LdcMemoryAllocation memAligned = {0};
        const uintptr_t mask = ((1 << i) - 1);

        uint8_t* ptr =
            VNAllocateAlignedArray(allocator, &memAligned, uint8_t, ((uintptr_t)1) << i, blockSize);

        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, memAligned.ptr);

        EXPECT_EQ((uintptr_t)ptr & mask, 0);

        memset(memAligned.ptr, 42, blockSize);

        VNFree(allocator, &memAligned);
        EXPECT_EQ(memAligned.ptr, nullptr);
        EXPECT_EQ(memAligned.size, 0);
    }
}

#ifdef __SSE2__
#include <emmintrin.h>

TEST_F(MemoryTest, AllocateAlignedSSE)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem = {0};

    __m128i* ptr = VNAllocateArray(allocator, &mem, __m128i, 4096);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, mem.ptr);

    // Check 16 byte alignment
    const uintptr_t mask = 16 - 1;
    EXPECT_EQ((uintptr_t)ptr & mask, 0);

    // Scribble on vector
    for (uint32_t i = 0; i < vectorSize; ++i) {
        ptr[i] = _mm_set1_epi32(42);
    }

    VNFree(allocator, &mem);
    EXPECT_EQ(mem.ptr, nullptr);
    EXPECT_EQ(mem.size, 0);
}
#endif

#ifdef __AVX__
#include <immintrin.h>
TEST_F(MemoryTest, AllocateAlignedAVX)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem = {0};

    __m256i* ptr = VNAllocateArray(allocator, &mem, __m256i, 4096);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, mem.ptr);

    // Check 32 byte alignment
    const uintptr_t mask = 32 - 1;
    EXPECT_EQ((uintptr_t)ptr & mask, 0);

    // Scribble on vector
    for (uint32_t i = 0; i < vectorSize; ++i) {
        ptr[i] = _mm256_set1_epi32(42);
    }

    VNFree(allocator, &mem);
    EXPECT_EQ(mem.ptr, nullptr);
    EXPECT_EQ(mem.size, 0);
}
#endif

#if VN_CORE_FEATURE(NEON)
#include <arm_neon.h>

TEST_F(MemoryTest, AllocateAlignedNEON)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem = {0};

    uint16x8_t* ptr = VNAllocateArray(allocator, &mem, uint16x8_t, 4096);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, mem.ptr);

    // Check 8 byte alignment
    const uintptr_t mask = 16 - 1;
    EXPECT_EQ((uintptr_t)ptr & mask, 0);

    static const uint16_t src[8] = {40, 41, 42, 43, 44, 45, 46, 47};

    // Scribble on vector
    for (uint32_t i = 0; i < vectorSize; ++i) {
        ptr[i] = vld1q_u16(src);
    }

    VNFree(allocator, &mem);
    EXPECT_EQ(mem.ptr, nullptr);
    EXPECT_EQ(mem.size, 0);
}
#endif
