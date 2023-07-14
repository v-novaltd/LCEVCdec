/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "unit_fixture.h"

extern "C"
{
#include "common/dither.h"
}

// -----------------------------------------------------------------------------

static constexpr uint8_t kValidDither = 128;
static constexpr uint8_t kInvalidDither = 129;
static constexpr size_t kInvalidBufferLength = 16385;

// -----------------------------------------------------------------------------

class DitherFixture : public Fixture
{
public:
    using Super = Fixture;

    void SetUp() final
    {
        Super::SetUp();

        ditherInitialize(context.get()->memory, &dither, 0, true);
    }

    void TearDown() final
    {
        ditherRelease(dither);
        Super::TearDown();
    }

    Dither_t dither;
};

// -----------------------------------------------------------------------------

TEST_F(DitherFixture, CheckInvalidStrength)
{
    EXPECT_FALSE(ditherRegenerate(dither, kInvalidDither, DTUniform));
    EXPECT_TRUE(ditherRegenerate(dither, kValidDither, DTUniform));
}

TEST_F(DitherFixture, CheckInvalidLength)
{
    EXPECT_TRUE(ditherRegenerate(dither, 5, DTUniform));
    EXPECT_EQ(ditherGetBuffer(dither, kInvalidBufferLength), nullptr);
}

TEST_F(DitherFixture, CheckValuesAreWithinStrength)
{
    // This relies on knowing that the internal dither buffer is
    // 16k long.
    static constexpr size_t kDitherBufferCheckLength = 8192;

    for (uint8_t i = 1; i <= kValidDither; ++i) {
        EXPECT_TRUE(ditherRegenerate(dither, i, DTUniform));
        const int8_t* values = ditherGetBuffer(dither, kDitherBufferCheckLength);
        const int32_t minimumValue = static_cast<int32_t>(-i);
        const int32_t maximumValue = static_cast<int32_t>(i);

        for (size_t j = 0; j < kDitherBufferCheckLength; ++j) {
            const int32_t value = static_cast<int32_t>(values[j]);
            EXPECT_GE(value, minimumValue);
            EXPECT_LE(value, maximumValue);
        }
    }
}

// -----------------------------------------------------------------------------