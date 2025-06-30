/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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
#include <LCEVC/pixel_processing/dither.h>

#include <array>
#include <memory>
#include <numeric>

// -----------------------------------------------------------------------------

static constexpr uint8_t kValidDitherStrength = 31;
static constexpr uint8_t kInvalidDitherStrength = 32;
static constexpr size_t kInvalidBufferLength = 16385;

// -----------------------------------------------------------------------------

class DitherFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        ldppDitherGlobalInitialize(ldcMemoryAllocatorMalloc(), &dither, 0);
        ldppDitherFrameInitialise(&frame, &dither, 0, 0);
        ldppDitherSliceInitialise(&slice, &frame, 0, 0);
    }

    void TearDown() override { ldppDitherGlobalRelease(&dither); }

    LdppDitherGlobal dither;
    LdppDitherFrame frame;
    LdppDitherSlice slice;
};

// -----------------------------------------------------------------------------

TEST_F(DitherFixture, CheckInvalidStrength)
{
    EXPECT_FALSE(ldppDitherFrameInitialise(&frame, &dither, 0, kInvalidDitherStrength));
    EXPECT_TRUE(ldppDitherFrameInitialise(&frame, &dither, 0, kValidDitherStrength));
}

TEST_F(DitherFixture, CheckInvalidLength)
{
    EXPECT_EQ(ldppDitherGetBuffer(&slice, kInvalidBufferLength), nullptr);
}

TEST_F(DitherFixture, CheckValuesAreWithinStrength)
{
    // This relies on knowing that the internal dither buffer is
    // 16k long.
    static constexpr size_t kDitherBufferCheckLength = 8192;

    for (uint8_t strength = 1; strength <= kValidDitherStrength; ++strength) {
        const uint16_t* entropy = ldppDitherGetBuffer(&slice, kDitherBufferCheckLength);
        const int32_t minimumValue = -strength;
        const int32_t maximumValue = strength;

        for (size_t j = 0; j < kDitherBufferCheckLength; j++) {
            int32_t result = 0;
            ldppDitherApply(&result, &entropy, 0, strength);
            EXPECT_GE(result, minimumValue);
            EXPECT_LE(result, maximumValue);
        }
    }
}

TEST_F(DitherFixture, CheckSIMDAccuracy)
{
    static constexpr size_t kEntropyRange = 0x10000;
    std::array<uint16_t, 16> entropyValues{};
    std::array<int16_t, 16> simdResults{};

    for (uint8_t strength = 1; strength <= kValidDitherStrength; ++strength) {
        for (size_t simdIndex = 0; simdIndex < kEntropyRange; simdIndex += 16) {
            std::iota(entropyValues.begin(), entropyValues.end(), static_cast<uint16_t>(simdIndex));
            const uint16_t* pEntropySIMD = entropyValues.data();
            const uint16_t* pEntropyScalar = entropyValues.data();
            simdResults.fill(0);

#if VN_CORE_FEATURE(SSE)
            ldppDitherApplySSE((__m128i*)simdResults.data(), &pEntropySIMD, 0, strength);
#elif VN_CORE_FEATURE(NEON)
            // Explicit copy to/from result buffer for NEON
            int16x8x2_t neonResult = vld2q_s16(simdResults.data());
            ldppDitherApplyNEON(&neonResult, &pEntropySIMD, 0, strength);
            vst2q_s16(simdResults.data(), neonResult);
#endif

            // Compare SIMD result against scalar
            for (auto simdResult : simdResults) {
                int32_t scalarResult = 0;
                ldppDitherApply(&scalarResult, &pEntropyScalar, 0, strength);
                EXPECT_EQ(simdResult, scalarResult);
            }
        }
    }
}

// -----------------------------------------------------------------------------