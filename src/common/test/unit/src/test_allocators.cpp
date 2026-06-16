/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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
#include "gtest/gtest.h"

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/recycling_allocator.h>
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/common/simple_allocator.h>
#include <LCEVC/utility/md5.h>
//
#include <gtest/gtest.h>
//
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <random>
#include <string>

// A general set of allocator tests that are parameterized on the various allocator types.
//
namespace {

enum class AllocatorKind
{
    None,
    SystemMalloc,
    RollingArena,
    Simple,
    Recycling,
};

struct SimpleStruct
{
    int a;
    int b;
    int c;
    int d;
};

class MemoryAllocatorTest : public ::testing::TestWithParam<AllocatorKind>
{
protected:
    void SetUp() override
    {
        // Choose allocator
        allocatorKind = GetParam();

        // Fixed seed for random number generation
        generator.seed(42);

        switch (allocatorKind) {
            case AllocatorKind::SystemMalloc:
                //
                allocator = ldcMemoryAllocatorMalloc();
                break;
            case AllocatorKind::RollingArena:
                //
                allocator = ldcRollingArenaInitialize(&rollingArena, ldcMemoryAllocatorMalloc(), 32, 1024);
                break;
            case AllocatorKind::Simple:
                //
                allocator =
                    ldcMemorySimpleAllocatorInitialize(&simpleAllocator, ldcMemoryAllocatorMalloc());
                break;
            case AllocatorKind::Recycling:
                //
                allocator =
                    ldcRecyclingAllocatorInitialize(&recyclingAllocator, ldcMemoryAllocatorMalloc(), 8);
                break;
            default: FAIL(); break;
        }

        ASSERT_NE(allocator, nullptr);
    }

    void TearDown() override
    {
        expectAllocatorClean();

        switch (allocatorKind) {
            case AllocatorKind::SystemMalloc: break;
            case AllocatorKind::RollingArena: ldcRollingArenaDestroy(&rollingArena); break;
            case AllocatorKind::Simple: ldcMemorySimpleAllocatorDestroy(&simpleAllocator); break;
            case AllocatorKind::Recycling: ldcRecyclingAllocatorDestroy(&recyclingAllocator); break;
            default: FAIL(); break;
        }

        allocator = nullptr;
    }

    void expectAllocatorClean()
    {
        switch (allocatorKind) {
            case AllocatorKind::RollingArena:
                EXPECT_EQ(rollingArena.slotFront, rollingArena.slotBack);
                EXPECT_EQ(rollingArena.bufferFront, rollingArena.bufferBack);
                for (uint32_t i = 0; i < kRollingArenaMaxBuffers; ++i) {
                    EXPECT_EQ(rollingArena.buffers[i].allocationCount, 0u);
                }
                break;
            case AllocatorKind::Simple:
                EXPECT_TRUE(ldcMemoryAllocatorSimpleDebugCheck(&simpleAllocator));
                break;

            case AllocatorKind::SystemMalloc:
            case AllocatorKind::Recycling: break;
            default: FAIL(); break;
        }
    }

    // Pseudorandom number via seeded generator
    uint32_t random(uint32_t limit) { return limit ? (distribU32(generator) % limit) : 0; }

    void hash(void* ptr, size_t size, uint8_t digestOut[16])
    {
        md5.reset();
        md5.update(static_cast<uint8_t*>(ptr), size);
        md5.digest(digestOut);
    }

    std::minstd_rand generator;
    std::uniform_int_distribution<uint32_t> distribU32{0, std::numeric_limits<uint32_t>::max()};

    lcevc_dec::utility::MD5 md5;

    AllocatorKind allocatorKind{AllocatorKind::None};
    LdcMemoryAllocator* allocator{};

    LdcMemoryAllocatorRollingArena rollingArena{};
    LdcMemorySimpleAllocator simpleAllocator{};
    ldcMemoryRecyclingAllocator recyclingAllocator{};
};

TEST_P(MemoryAllocatorTest, Allocate)
{
    LdcMemoryAllocation intAllocation = {};
    VNAllocate(allocator, &intAllocation, int, "");
    int* intPtr = VNAllocationPtr(intAllocation, int);
    EXPECT_NE(intPtr, nullptr);
    EXPECT_EQ(intPtr, intAllocation.ptr);
    std::memset(intAllocation.ptr, 42, sizeof(int));
    VNFree(allocator, &intAllocation);
    EXPECT_EQ(intAllocation.ptr, nullptr);
    EXPECT_EQ(intAllocation.size, 0u);

    LdcMemoryAllocation structAllocation = {};
    VNAllocate(allocator, &structAllocation, SimpleStruct, "");
    SimpleStruct* structPtr = VNAllocationPtr(structAllocation, SimpleStruct);
    EXPECT_NE(structPtr, nullptr);
    EXPECT_EQ(structPtr, structAllocation.ptr);
    std::memset(structAllocation.ptr, 0, sizeof(SimpleStruct));
    VNFree(allocator, &structAllocation);
    EXPECT_EQ(structAllocation.ptr, nullptr);
    EXPECT_EQ(structAllocation.size, 0u);

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateArray)
{
    constexpr int kArraySize = 1000;

    LdcMemoryAllocation intAllocation = {};
    VNAllocateArray(allocator, &intAllocation, int, kArraySize, "");
    int* intPtr = VNAllocationPtr(intAllocation, int);
    EXPECT_NE(intPtr, nullptr);
    EXPECT_EQ(intPtr, intAllocation.ptr);
    std::memset(intAllocation.ptr, 42, sizeof(int) * kArraySize);
    VNFree(allocator, &intAllocation);
    EXPECT_EQ(intAllocation.ptr, nullptr);
    EXPECT_EQ(intAllocation.size, 0u);

    LdcMemoryAllocation structAllocation = {};
    VNAllocateArray(allocator, &structAllocation, SimpleStruct, kArraySize, "");
    SimpleStruct* structPtr = VNAllocationPtr(structAllocation, SimpleStruct);
    EXPECT_NE(structPtr, nullptr);
    EXPECT_EQ(structPtr, structAllocation.ptr);
    std::memset(structAllocation.ptr, 42, sizeof(SimpleStruct) * kArraySize);
    VNFree(allocator, &structAllocation);
    EXPECT_EQ(structAllocation.ptr, nullptr);
    EXPECT_EQ(structAllocation.size, 0u);

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateZero)
{
    LdcMemoryAllocation intAllocation = {};
    VNAllocateZero(allocator, &intAllocation, int, "");
    int* intPtr = VNAllocationPtr(intAllocation, int);
    EXPECT_NE(intPtr, nullptr);
    EXPECT_EQ(intPtr, intAllocation.ptr);
    EXPECT_EQ(*intPtr, 0);
    VNFree(allocator, &intAllocation);
    EXPECT_EQ(intAllocation.ptr, nullptr);
    EXPECT_EQ(intAllocation.size, 0u);

    LdcMemoryAllocation structAllocation = {};
    VNAllocateZero(allocator, &structAllocation, SimpleStruct, "");
    SimpleStruct* structPtr = VNAllocationPtr(structAllocation, SimpleStruct);
    EXPECT_NE(structPtr, nullptr);
    EXPECT_EQ(structPtr, structAllocation.ptr);
    EXPECT_EQ(structPtr->a, 0);
    EXPECT_EQ(structPtr->b, 0);
    EXPECT_EQ(structPtr->c, 0);
    EXPECT_EQ(structPtr->d, 0);
    VNFree(allocator, &structAllocation);
    EXPECT_EQ(structAllocation.ptr, nullptr);
    EXPECT_EQ(structAllocation.size, 0u);

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateZeroArray)
{
    constexpr int kArraySize = 1000;

    LdcMemoryAllocation intAllocation = {};
    VNAllocateZeroArray(allocator, &intAllocation, int, kArraySize, "");
    int* intPtr = VNAllocationPtr(intAllocation, int);
    EXPECT_NE(intPtr, nullptr);
    EXPECT_EQ(intPtr, intAllocation.ptr);
    for (int i = 0; i < kArraySize; ++i) {
        EXPECT_EQ(intPtr[i], 0);
    }
    VNFree(allocator, &intAllocation);
    EXPECT_EQ(intAllocation.ptr, nullptr);
    EXPECT_EQ(intAllocation.size, 0u);

    LdcMemoryAllocation structAllocation = {};
    VNAllocateZeroArray(allocator, &structAllocation, SimpleStruct, kArraySize, "");
    SimpleStruct* structPtr = VNAllocationPtr(structAllocation, SimpleStruct);
    EXPECT_NE(structPtr, nullptr);
    EXPECT_EQ(structPtr, structAllocation.ptr);
    for (int i = 0; i < kArraySize; ++i) {
        EXPECT_EQ(structPtr[i].a, 0);
        EXPECT_EQ(structPtr[i].b, 0);
        EXPECT_EQ(structPtr[i].c, 0);
        EXPECT_EQ(structPtr[i].d, 0);
    }
    VNFree(allocator, &structAllocation);
    EXPECT_EQ(structAllocation.ptr, nullptr);
    EXPECT_EQ(structAllocation.size, 0u);

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeInOrder)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint32_t, "");
        uint32_t* ptr = VNAllocationPtr(ma[i], uint32_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = 0; i < kCount; ++i) {
        VNFree(allocator, &ma[i]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeReverse)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint64_t, "");
        uint64_t* ptr = VNAllocationPtr(ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = kCount - 1; i >= 0; --i) {
        VNFree(allocator, &ma[i]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeShuffle)
{
    const int kCount = 10;
    LdcMemoryAllocation ma[kCount] = {};
    std::vector<uint32_t> indices;
    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint64_t, "");
        uint64_t* ptr = VNAllocationPtr(ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
        indices.push_back(i);
    }

    std::shuffle(indices.begin(), indices.end(), generator);

    for (int idx : indices) {
        VNFree(allocator, &ma[idx]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeInOrder100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint32_t, "");
        uint32_t* ptr = VNAllocationPtr(ma[i], uint32_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = 0; i < kCount; ++i) {
        VNFree(allocator, &ma[i]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeReverse100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};

    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint64_t, "");
        uint64_t* ptr = VNAllocationPtr(ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
    }

    for (int i = kCount - 1; i >= 0; --i) {
        VNFree(allocator, &ma[i]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateFreeShuffle100)
{
    const int kCount = 100;
    LdcMemoryAllocation ma[kCount] = {};
    std::vector<uint32_t> indices;
    for (int i = 0; i < kCount; ++i) {
        VNAllocate(allocator, &ma[i], uint64_t, "");
        uint64_t* ptr = VNAllocationPtr(ma[i], uint64_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, ma[i].ptr);
        indices.push_back(i);
    }

    std::shuffle(indices.begin(), indices.end(), generator);

    for (int idx : indices) {
        VNFree(allocator, &ma[idx]);
    }

    expectAllocatorClean();
}

TEST_P(MemoryAllocatorTest, AllocateAligned)
{
    constexpr size_t kBlockSize = 4096;

    for (int i = 15; i >= 0; --i) {
        LdcMemoryAllocation allocation = {};
        const uintptr_t mask = (static_cast<uintptr_t>(1) << i) - 1;

        VNAllocateAlignedArray(allocator, &allocation, uint8_t, static_cast<uintptr_t>(1) << i,
                               kBlockSize, "");
        uint8_t* ptr = VNAllocationPtr(allocation, uint8_t);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(ptr, allocation.ptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & mask, 0u);

        std::memset(allocation.ptr, 42, kBlockSize);

        VNFree(allocator, &allocation);
        EXPECT_EQ(allocation.ptr, nullptr);
        EXPECT_EQ(allocation.size, 0u);
    }

    expectAllocatorClean();
}

#if VN_SDK_FEATURE(SSE)
#include <emmintrin.h>

TEST_P(MemoryAllocatorTest, AllocateAlignedSSE)
{
    constexpr size_t kVectorSize = 4096;
    LdcMemoryAllocation allocation = {};

    VNAllocateArray(allocator, &allocation, __m128i, kVectorSize, "");
    __m128i* ptr = VNAllocationPtr(allocation, __m128i);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, allocation.ptr);

    const uintptr_t mask = 16 - 1;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & mask, 0u);

    for (uint32_t i = 0; i < kVectorSize; ++i) {
        ptr[i] = _mm_set1_epi32(42);
    }

    VNFree(allocator, &allocation);
    EXPECT_EQ(allocation.ptr, nullptr);
    EXPECT_EQ(allocation.size, 0u);

    expectAllocatorClean();
}
#endif

#ifdef __AVX__
#include <immintrin.h>

TEST_P(MemoryAllocatorTest, AllocateAlignedAVX)
{
    constexpr size_t kVectorSize = 4096;
    LdcMemoryAllocation allocation = {};

    VNAllocateArray(allocator, &allocation, __m256i, kVectorSize, "");
    __m256i* ptr = VNAllocationPtr(allocation, __m256i);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, allocation.ptr);

    const uintptr_t mask = 32 - 1;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & mask, 0u);

    for (uint32_t i = 0; i < kVectorSize; ++i) {
        ptr[i] = _mm256_set1_epi32(42);
    }

    VNFree(allocator, &allocation);
    EXPECT_EQ(allocation.ptr, nullptr);
    EXPECT_EQ(allocation.size, 0u);

    expectAllocatorClean();
}
#endif

#if VN_SDK_FEATURE(NEON)
#include <arm_neon.h>

TEST_P(MemoryAllocatorTest, AllocateAlignedNEON)
{
    constexpr size_t kVectorSize = 4096;
    LdcMemoryAllocation allocation = {};

    VNAllocateArray(allocator, &allocation, uint16x8_t, kVectorSize, "");
    uint16x8_t* ptr = VNAllocationPtr(allocation, uint16x8_t);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr, allocation.ptr);

    const uintptr_t mask = 16 - 1;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & mask, 0u);

    static const uint16_t kData[8] = {40, 41, 42, 43, 44, 45, 46, 47};
    for (uint32_t i = 0; i < kVectorSize; ++i) {
        ptr[i] = vld1q_u16(kData);
    }

    VNFree(allocator, &allocation);
    EXPECT_EQ(allocation.ptr, nullptr);
    EXPECT_EQ(allocation.size, 0u);

    expectAllocatorClean();
}
#endif

TEST_P(MemoryAllocatorTest, RandomAllocations)
{
    constexpr uint32_t kLoopCount = 1000;
    constexpr uint32_t kAllocationMax = 4000;
    constexpr uint32_t kThresholdCentre = 400;
    constexpr uint32_t kThresholdVariance = 100;
    constexpr uint32_t kRepeatMax = 20;
    constexpr uint32_t kFreeRange = 3;

    size_t allocTotal = 0;

    struct Record
    {
        LdcMemoryAllocation allocation;
        size_t size;
        uint8_t digest[16];
    };
    std::deque<Record> allocations;

    for (uint32_t i = 0;; ++i) {
        const uint32_t threshold = kThresholdCentre + random(kThresholdVariance) - kThresholdVariance / 2;

        if ((i < kLoopCount) && allocations.size() < threshold) {
            const uint32_t repeat = random(kRepeatMax);

            for (uint32_t j = 0; j < repeat; ++j) {
                Record rec{};
                rec.size = random(kAllocationMax) + 1;
                VNAllocateArray(allocator, &rec.allocation, uint8_t, rec.size, "");
                ASSERT_TRUE(VNIsAllocated(rec.allocation));
                allocTotal += rec.size;

                uint8_t* ptr = static_cast<uint8_t*>(rec.allocation.ptr);
                for (uint32_t k = 0; k < rec.size; ++k) {
                    ptr[k] = static_cast<uint8_t>(random(256));
                }

                hash(rec.allocation.ptr, rec.size, rec.digest);

                allocations.push_back(rec);
            }
        } else {
            const uint32_t repeat = random(kRepeatMax);

            for (uint32_t j = 0; j < repeat; ++j) {
                if (allocations.empty()) {
                    continue;
                }

                const uint32_t bound = static_cast<uint32_t>(allocations.size() / kFreeRange);
                const uint32_t idx = random(bound ? bound : static_cast<uint32_t>(allocations.size()));
                ASSERT_LT(idx, allocations.size());

                Record rec = *(allocations.begin() + idx);
                allocations.erase(allocations.begin() + idx);

                uint8_t digest[16] = {0};
                hash(rec.allocation.ptr, rec.size, digest);
                EXPECT_EQ(std::memcmp(digest, rec.digest, sizeof(digest)), 0);

                VNFree(allocator, &rec.allocation);
                allocTotal -= rec.size;
            }
        }

        if (!allocations.empty() && random(100) == 1) {
            size_t r = random(static_cast<uint32_t>(allocations.size() * 2));
            if (r >= allocations.size()) {
                r = allocations.size() - 1;
            }

            Record& rec = *(allocations.begin() + r);
            allocTotal -= rec.size;

            size_t newSize = random(kAllocationMax) + 1;
            const size_t preservedSize = std::min(newSize, rec.size);
            uint8_t preservedDigest[16] = {0};
            hash(rec.allocation.ptr, preservedSize, preservedDigest);

            VNReallocateArray(allocator, &rec.allocation, uint8_t, newSize, "");
            ASSERT_TRUE(VNIsAllocated(rec.allocation));

            uint8_t digest[16] = {0};
            hash(rec.allocation.ptr, preservedSize, digest);
            EXPECT_EQ(std::memcmp(digest, preservedDigest, sizeof(digest)), 0);

            uint8_t* ptr = static_cast<uint8_t*>(rec.allocation.ptr);
            for (uint32_t k = 0; k < rec.allocation.size; ++k) {
                ptr[k] = static_cast<uint8_t>(random(256));
            }

            hash(rec.allocation.ptr, rec.allocation.size, rec.digest);
            allocTotal += rec.allocation.size;
            rec.size = rec.allocation.size;
        }

        if (i >= kLoopCount && allocations.empty()) {
            break;
        }
    }

    EXPECT_TRUE(allocations.empty());
    expectAllocatorClean();
    VNUnused(allocTotal);
}

std::string AllocatorKindToString(const ::testing::TestParamInfo<AllocatorKind>& info)
{
    switch (info.param) {
        case AllocatorKind::SystemMalloc: return "SystemMalloc";
        case AllocatorKind::RollingArena: return "RollingArena";
        case AllocatorKind::Simple: return "SimpleAllocator";
        case AllocatorKind::Recycling: return "RecyclingAllocator";
        case AllocatorKind::None: return "None";
    }
    return "Unknown";
}

INSTANTIATE_TEST_SUITE_P(AllAllocators, MemoryAllocatorTest,
                         ::testing::Values(AllocatorKind::SystemMalloc, AllocatorKind::RollingArena,
                                           AllocatorKind::Simple, AllocatorKind::Recycling),
                         AllocatorKindToString);

} // namespace
