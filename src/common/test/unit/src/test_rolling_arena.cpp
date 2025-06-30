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
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/utility/md5.h>

#include <algorithm>
#include <climits>
#include <deque>
#include <random>
#include <vector>

template <uint32_t INITIAL_SLOT_COUNT, uint32_t INITIAL_CAPACITY>
class RollingArena : public testing::Test
{
public:
    const int kInitialSlotCount = INITIAL_SLOT_COUNT;
    const int kInitialCapacity = INITIAL_CAPACITY;

    RollingArena()
        : runtimeAllocator(ldcMemoryAllocatorMalloc())
    {
        allocator =
            ldcRollingArenaInitialize(&arena, runtimeAllocator, kInitialSlotCount, kInitialCapacity);
    }

    ~RollingArena() override {}

    void checkEmpty() const
    {
        EXPECT_EQ(arena.slotFront, arena.slotBack);
        EXPECT_EQ(arena.bufferFront, arena.bufferBack);

        for (uint32_t i = 0; i < kRollingArenaMaxBuffers; ++i) {
            EXPECT_EQ(arena.buffers[i].allocationCount, 0);
        }
    }

    void hash(void* ptr, size_t size, uint8_t digestOut[16])
    {
        md5.reset();
        md5.update((const uint8_t*)ptr, size);
        md5.digest(digestOut);
    }

    LdcMemoryAllocator* runtimeAllocator;
    LdcMemoryAllocatorRollingArena arena;
    LdcMemoryAllocator* allocator;

    std::minstd_rand gen;
    std::uniform_int_distribution<uint32_t> distrib{0, INT_MAX};
    uint32_t random(uint32_t limit) { return limit ? (distrib(gen) % limit) : 0; }

    lcevc_dec::utility::MD5 md5;

    RollingArena(const RollingArena&) = delete;
    RollingArena(RollingArena&&) = delete;
    void operator=(const RollingArena&) = delete;
    void operator=(RollingArena&&) = delete;
};

struct S
{
    int a;
    int b;
    int c;
    int d;
};

using RollingArenaSmall = RollingArena<32, 1024>;

TEST_F(RollingArenaSmall, Allocate)
{
    LdcMemoryAllocation memi;
    int* ptri = VNAllocate(allocator, &memi, int);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    memset(memi.ptr, 42, sizeof(int));
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems;
    S* ptrs = VNAllocate(allocator, &mems, S);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    memset(mems.ptr, 0, sizeof(S));
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateArray)
{
    const int kArraySize = 1000;

    LdcMemoryAllocation memi;

    int* ptri = VNAllocateArray(allocator, &memi, int, kArraySize);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    memset(memi.ptr, 42, sizeof(int) * kArraySize);
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems;
    S* ptrs = VNAllocateArray(allocator, &mems, S, kArraySize);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    memset(mems.ptr, 42, sizeof(S) * kArraySize);
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateZero)
{
    LdcMemoryAllocation memi;
    int* ptri = VNAllocateZero(allocator, &memi, int);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    EXPECT_EQ(*ptri, 0);
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems;
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

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateZeroArray)
{
    const int kArraySize = 1000;
    LdcMemoryAllocation memi = {0};

    int* ptri = VNAllocateZeroArray(allocator, &memi, int, kArraySize);
    EXPECT_NE(ptri, nullptr);
    EXPECT_EQ(ptri, memi.ptr);
    for (int i = 0; i < kArraySize; ++i) {
        EXPECT_EQ(ptri[i], 0);
    }
    VNFree(allocator, &memi);
    EXPECT_EQ(memi.ptr, nullptr);
    EXPECT_EQ(memi.size, 0);

    LdcMemoryAllocation mems = {0};
    S* ptrs = VNAllocateZeroArray(allocator, &mems, S, kArraySize);
    EXPECT_NE(ptrs, nullptr);
    EXPECT_EQ(ptrs, mems.ptr);
    for (int i = 0; i < kArraySize; ++i) {
        EXPECT_EQ(ptrs[i].a, 0);
        EXPECT_EQ(ptrs[i].b, 0);
        EXPECT_EQ(ptrs[i].c, 0);
        EXPECT_EQ(ptrs[i].d, 0);
    }
    VNFree(allocator, &mems);
    EXPECT_EQ(mems.ptr, nullptr);
    EXPECT_EQ(mems.size, 0);

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeInOrder)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        uint32_t* ptr = VNAllocate(allocator, &ma[i], uint32_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = 0; i < kCount; ++i) {
        VNFree(allocator, &ma[i]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeReverse)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        uint64_t* ptr = VNAllocate(allocator, &ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = kCount - 1; i >= 0; --i) {
        VNFree(allocator, &ma[i]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeShuffle)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};
    std::vector<uint32_t> indices;
    for (int i = 0; i < kCount; ++i) {
        uint64_t* ptr = VNAllocate(allocator, &ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
        indices.push_back(i);
    }

    std::shuffle(indices.begin(), indices.end(), gen);

    for (int idx : indices) {
        VNFree(allocator, &ma[idx]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeInOrder100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        uint32_t* ptr = VNAllocate(allocator, &ma[i], uint32_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = 0; i < kCount; ++i) {
        VNFree(allocator, &ma[i]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeReverse100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        uint64_t* ptr = VNAllocate(allocator, &ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = kCount - 1; i >= 0; --i) {
        VNFree(allocator, &ma[i]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateFreeShuffle100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};
    std::vector<uint32_t> indices;
    for (int i = 0; i < kCount; ++i) {
        uint64_t* ptr = VNAllocate(allocator, &ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
        indices.push_back(i);
    }

    std::shuffle(indices.begin(), indices.end(), gen);

    for (int idx : indices) {
        VNFree(allocator, &ma[idx]);
    }

    checkEmpty();
}

TEST_F(RollingArenaSmall, AllocateAligned)
{
    const size_t blockSize = 4096;

    // Check from single byte up to 64k alignment
    for (int i = 0; i < 16; ++i) {
        LdcMemoryAllocation memAligned;
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

#if VN_CORE_FEATURE(SSE)
#include <emmintrin.h>

TEST_F(RollingArenaSmall, AllocateAlignedSSE)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem{};

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

#ifdef __AVX__ //  VN_CORE_FEATURE(AVX2)
#include <immintrin.h>
TEST_F(RollingArenaSmall, AllocateAlignedAVX)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem{};

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

TEST_F(RollingArenaSmall, AllocateAlignedNEON)
{
    const size_t vectorSize = 4096;
    LdcMemoryAllocation mem;

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

using RollingArenaLarge = RollingArena<512, 1024 * 1024>;

TEST_F(RollingArenaLarge, RandomAllocations)
{
    const uint32_t kLoopCount = 10000;
    const uint32_t kAllocationMax = 4000;

    const uint32_t kThresholdCentre = 400;
    const uint32_t kThresholdVariance = 100;
    const uint32_t kRepeatMax = 20;

    const uint32_t kFreeRange = 3;

    uint32_t allocTotal = 0;

    lcevc_dec::utility::MD5 md5;

    struct Record
    {
        LdcMemoryAllocation allocation;
        uint32_t size;
        uint8_t digest[16];
    };
    std::deque<Record> allocations;

    for (uint32_t i = 0;; ++i) {
        const uint32_t threshold = kThresholdCentre + random(kThresholdVariance) - kThresholdVariance / 2;

        if ((i < kLoopCount) && allocations.size() < threshold) {
            // Allocate
            const uint32_t repeat = random(kRepeatMax);

            for (uint32_t j = 0; j < repeat; ++j) {
                Record rec{};
                rec.size = random(kAllocationMax) + 1;
                VNAllocateArray(allocator, &rec.allocation, uint8_t, rec.size);
                allocTotal += rec.size;

                // Fill memory with stuff, and hash it
                uint8_t* ptr = (uint8_t*)rec.allocation.ptr;
                for (uint32_t k = 0; k < rec.size; ++k) {
                    *ptr++ = random(256);
                }

                hash(rec.allocation.ptr, rec.size, rec.digest);

                allocations.push_back(rec);
            }
        } else {
            // Free
            const uint32_t repeat = random(kRepeatMax);

            for (uint32_t j = 0; j < repeat; ++j) {
                if (allocations.empty()) {
                    continue;
                }

                VNUnused(kFreeRange);
                // Pick a record to free, random offset from 'end' (scaled by range)
                uint32_t r = random((uint32_t)allocations.size() / kFreeRange);
                ASSERT_LT(r, allocations.size());

                Record rec = *(allocations.begin() + r);
                allocations.erase(allocations.begin() + r);

                // Check the hash
                uint8_t digest[16] = {0};
                hash(rec.allocation.ptr, rec.size, digest);
                EXPECT_EQ(memcmp(digest, rec.digest, sizeof(digest)), 0);

                VNFree(allocator, &rec.allocation);
                allocTotal -= rec.size;
            }
        }

        // Sometimes pick a slot to reallocate
        if (!allocations.empty() && random(100) == 1) {
            // Pick a record - halve time, pick most recent
            size_t r = random((uint32_t)(allocations.size() * 2));
            if (r >= allocations.size()) {
                r = allocations.size() - 1;
            }

            Record& rec = *(allocations.begin() + r);
            allocTotal -= rec.size;

            uint32_t newSize = random(kAllocationMax) + 1;

            // Has the part that will be preserved
            uint32_t preservedSize = std::min(newSize, rec.size);
            uint8_t preservedDigest[16] = {0};
            hash(rec.allocation.ptr, preservedSize, preservedDigest);

            VNReallocateArray(allocator, &rec.allocation, uint8_t, newSize);

            // Did it survive?
            uint8_t digest[16] = {0};
            hash(rec.allocation.ptr, preservedSize, digest);
            EXPECT_EQ(memcmp(digest, preservedDigest, sizeof(digest)), 0);

            // Rehash the whole block
            hash(rec.allocation.ptr, rec.allocation.size, rec.digest);
            rec.size = newSize;

            allocTotal += rec.size;
        }

        if (i >= kLoopCount && allocations.empty()) {
            break;
        }
    }

    ASSERT_TRUE(allocations.empty());

    checkEmpty();

    VNUnused(allocTotal);
}
