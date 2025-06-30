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
#include <LCEVC/common/diagnostics_buffer.h>
#include <LCEVC/common/threads.h>
#include <LCEVC/utility/md5.h>

#include <cstdint>
#include <deque>
#include <random>

template <uint32_t CAPACITY, uint32_t VAR_DATA_SIZE>
class TestDiagnosticsBuffer : public testing::Test
{
public:
    const uint32_t kCapacity = CAPACITY;
    const uint32_t kVarDataSize = VAR_DATA_SIZE;

    TestDiagnosticsBuffer() {}
    ~TestDiagnosticsBuffer() override {}

    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
        ldcDiagnosticsBufferInitialize(&diagnosticsBuffer, CAPACITY, VAR_DATA_SIZE, allocator);

        // Fixed seed for random walk
        gen.seed(42);
    }

    void TearDown() override { ldcDiagnosticsBufferDestroy(&diagnosticsBuffer); }

    std::minstd_rand gen;
    std::uniform_int_distribution<uint32_t> distrib{0, INT_MAX};
    uint32_t random(uint32_t limit) { return distrib(gen) % limit; }

    LdcMemoryAllocator* allocator;
    LdcDiagnosticsBuffer diagnosticsBuffer;
};

using TestDiagnosticsBufferSmall = TestDiagnosticsBuffer<8, 1024>;

TEST_F(TestDiagnosticsBufferSmall, CreateDestroy)
{
    EXPECT_EQ(ldcDiagnosticsBufferCapacity(&diagnosticsBuffer), kCapacity);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
}

TEST_F(TestDiagnosticsBufferSmall, PushPop)
{
    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);

    const LdcDiagRecord dr{NULL, 1};
    ldcDiagnosticsBufferPush(&diagnosticsBuffer, &dr, NULL, 0);

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 1);

    LdcDiagRecord drp = {NULL, 0};
    ldcDiagnosticsBufferPop(&diagnosticsBuffer, &drp, NULL, 0, NULL);

    EXPECT_EQ(dr.site, drp.site);
    EXPECT_EQ(dr.timestamp, drp.timestamp);

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
}

TEST_F(TestDiagnosticsBufferSmall, PushPopFull)
{
    for (uint32_t i = 0; i < kCapacity - 1; ++i) {
        const LdcDiagRecord d{NULL, i};
        ldcDiagnosticsBufferPush(&diagnosticsBuffer, &d, NULL, 0);
    }
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 7);

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), true);

    for (uint32_t i = 0; i < kCapacity - 1; ++i) {
        LdcDiagRecord d{0};
        ldcDiagnosticsBufferPop(&diagnosticsBuffer, &d, NULL, 0, NULL);
        EXPECT_EQ(d.timestamp, i);
    }

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
}

TEST_F(TestDiagnosticsBufferSmall, PushPopFullWrapped)
{
    // Move halfway through ring
    for (uint32_t i = 0; i < kCapacity / 2; ++i) {
        const LdcDiagRecord d{NULL, 0};
        ldcDiagnosticsBufferPush(&diagnosticsBuffer, &d, NULL, 0);
    }

    for (uint32_t i = 0; i < kCapacity / 2; ++i) {
        LdcDiagRecord d{0, 0};
        ldcDiagnosticsBufferPop(&diagnosticsBuffer, &d, NULL, 0, NULL);
    }
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);

    // Now fill ring, wrapping over end
    for (uint32_t i = 0; i < kCapacity - 1; ++i) {
        const LdcDiagRecord d{NULL, i};
        ldcDiagnosticsBufferPush(&diagnosticsBuffer, &d, NULL, 0);
    }
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 7);

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), true);

    for (uint32_t i = 0; i < kCapacity - 1; ++i) {
        LdcDiagRecord d{NULL, 0};
        ldcDiagnosticsBufferPop(&diagnosticsBuffer, &d, NULL, 0, NULL);
        EXPECT_EQ(d.timestamp, i);
    }

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
}

TEST_F(TestDiagnosticsBufferSmall, PushPopVar)
{
    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);

    const LdcDiagRecord dr{NULL, 1};
    const uint8_t varDataIn[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ldcDiagnosticsBufferPush(&diagnosticsBuffer, &dr, varDataIn, sizeof(varDataIn));

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 1);

    LdcDiagRecord drp = {NULL, 0};
    uint8_t varDataOut[16];
    size_t varSize = 0;
    ldcDiagnosticsBufferPop(&diagnosticsBuffer, &drp, varDataOut, sizeof(varDataOut), &varSize);

    EXPECT_EQ(dr.site, drp.site);
    EXPECT_EQ(dr.timestamp, drp.timestamp);

    EXPECT_EQ(varSize, sizeof(varDataIn));
    EXPECT_EQ(memcmp(varDataIn, varDataOut, sizeof(varDataIn)), 0);

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);
    EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 0);
}

TEST_F(TestDiagnosticsBufferSmall, PushPopVarMany)
{
    const int kNumRecords = 1000;

    EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    EXPECT_EQ(ldcDiagnosticsBufferIsFull(&diagnosticsBuffer), false);

    const LdcDiagRecord dr1{NULL, 1};
    const LdcDiagRecord dr2{NULL, 2};
    const uint8_t varDataIn[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    for (int i = 0; i < kNumRecords; ++i) {
        uint32_t size = i % sizeof(varDataIn);

        ldcDiagnosticsBufferPush(&diagnosticsBuffer, &dr1, varDataIn, size);
        ldcDiagnosticsBufferPush(&diagnosticsBuffer, &dr2, varDataIn, size);

        EXPECT_EQ(ldcDiagnosticsBufferSize(&diagnosticsBuffer), 2);

        LdcDiagRecord drp = {NULL, 0};
        uint8_t varDataOut[sizeof(varDataIn) * 2];
        size_t varSize = 0;

        ldcDiagnosticsBufferPop(&diagnosticsBuffer, &drp, varDataOut, sizeof(varDataOut), &varSize);

        EXPECT_EQ(dr1.site, drp.site);
        EXPECT_EQ(dr1.timestamp, drp.timestamp);

        EXPECT_EQ(varSize, size);
        EXPECT_EQ(memcmp(varDataIn, varDataOut, size), 0);

        ldcDiagnosticsBufferPop(&diagnosticsBuffer, &drp, varDataOut, sizeof(varDataOut), &varSize);

        EXPECT_EQ(dr2.site, drp.site);
        EXPECT_EQ(dr2.timestamp, drp.timestamp);

        EXPECT_EQ(varSize, size);
        EXPECT_EQ(memcmp(varDataIn, varDataOut, size), 0);

        EXPECT_EQ(ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer), true);
    }
}

using TestDiagnosticsBufferLarge = TestDiagnosticsBuffer<32768, 1024 * 1024>;

// Random walk of pushes and pops - checking with matching std::deque
//

TEST_F(TestDiagnosticsBufferLarge, Random)
{
    const size_t kLoopCount = 10000;
    const size_t kRepeatMax = 50;
    const size_t kVarDataMax = 500;

    struct Reference
    {
        LdcDiagRecord record;
        uint32_t varDataSize;
        uint8_t digest[16];
    };

    std::deque<Reference> referenceBuffer;
    LdcDiagSite site;
    uint32_t pendingData = 0;

    for (uint32_t i = 0; i < kLoopCount; ++i) {
        switch (random(1024) % 2) {
            case 0: {
                // Push random number of records
                //
                const uint32_t repeat = random(kRepeatMax);
                for (uint32_t j = 0; j < repeat; ++j) {
                    if (ldcDiagnosticsBufferIsFull(&diagnosticsBuffer)) {
                        break;
                    }

                    Reference ref;
                    lcevc_dec::utility::MD5 md5;

                    const int varDataSize = random(kVarDataMax);
                    std::vector<uint8_t> varData(varDataSize);
                    for (auto& d : varData) {
                        d = random(256);
                    }

                    if (varDataSize > 0) {
                        md5.update(varData.data(), static_cast<uint32_t>(varData.size()));
                        md5.digest(ref.digest);
                    }
                    ref.varDataSize = varDataSize;

                    // Push test record
                    ref.record.site = &site;
                    ref.record.timestamp = random(90000);
                    ref.record.threadId = random(3000);
                    ldcDiagnosticsBufferPush(&diagnosticsBuffer, &ref.record, varData.data(),
                                             static_cast<uint32_t>(varData.size()));

                    // Push reference record
                    pendingData += ref.varDataSize;
                    referenceBuffer.push_back(ref);
                }
            } break;
            case 1: {
                // Pop random number of records
                //
                const uint32_t repeat = random(kRepeatMax);

                for (uint32_t j = 0; j < repeat; ++j) {
                    if (ldcDiagnosticsBufferIsEmpty(&diagnosticsBuffer)) {
                        ASSERT_TRUE(referenceBuffer.empty());
                        break;
                    }

                    // Get reference record
                    Reference ref = referenceBuffer.front();
                    referenceBuffer.pop_front();
                    pendingData -= ref.varDataSize;

                    // Pop test record
                    LdcDiagRecord record;
                    std::vector<uint8_t> varDataOut(kVarDataMax);
                    size_t varDataSize = 0;
                    ldcDiagnosticsBufferPop(&diagnosticsBuffer, &record, varDataOut.data(),
                                            static_cast<uint32_t>(varDataOut.size()), &varDataSize);

                    ASSERT_EQ(ref.record.site, record.site);
                    ASSERT_EQ(ref.record.timestamp, record.timestamp);
                    ASSERT_EQ(ref.record.threadId, record.threadId);

                    if (ref.varDataSize > 0 && varDataSize == 0) {
                        // Dropped data - check that what we think is pending data size really is
                        // bigger that the variable data capacity (with some leeway for wrapping)
                        EXPECT_GT(pendingData, kVarDataSize - 2 * kVarDataMax);
                    } else {
                        ASSERT_EQ(ref.varDataSize, varDataSize);
                    }

                    if (varDataSize > 0) {
                        lcevc_dec::utility::MD5 md5;
                        uint8_t digest[16] = {0};

                        md5.update(varDataOut.data(), varDataSize);
                        md5.digest(digest);

                        ASSERT_EQ(memcmp(digest, ref.digest, sizeof(digest)), 0);
                    }
                }
                break;
            }
        }
    }
}
