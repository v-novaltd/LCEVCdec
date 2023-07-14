/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include <range/v3/view.hpp>

#include <functional>
#include <random>
#include <sstream>

extern "C"
{
#include "surface/blit.h"
#include "surface/sharpen.h"
#include "surface/sharpen_common.h"
}

#include "unit_fixture.h"
#include "unit_utility.h"

extern "C"
{
SharpenFunction_t surfaceSharpenGetFunction(FixedPoint_t dstFP, CPUAccelerationFeatures_t preferredAccel);
}

namespace rg = ranges;
namespace rv = ranges::views;

// -----------------------------------------------------------------------------

constexpr uint32_t kWidth = 500;
constexpr uint32_t kHeight = 400;

// -----------------------------------------------------------------------------

class SharpenTest : public FixtureWithParam<FixedPoint_t>
{};

TEST_P(SharpenTest, CompareSIMD)
{
    const auto& fp = GetParam();

    Context_t* ctx = context.get();
    Surface_t surfScalar{};
    Surface_t surfSIMD{};

    surfaceIdle(ctx, &surfScalar);
    surfaceIdle(ctx, &surfSIMD);

    EXPECT_EQ(surfaceInitialise(ctx, &surfScalar, fp, kWidth, kHeight, kWidth, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &surfSIMD, fp, kWidth, kHeight, kWidth, ILNone), 0);

    // Fill scalar surface and copy over.
    fillSurfaceWithNoise(surfScalar);
    surfaceBlit(ctx, &surfScalar, &surfSIMD, BMCopy);

    EXPECT_TRUE(surfaceSharpen(ctx->sharpen, &surfScalar, nullptr, CAFNone));
    EXPECT_TRUE(surfaceSharpen(ctx->sharpen, &surfSIMD, nullptr, simdFlag()));

    const auto compareByteSize = fixedPointByteSize(fp) * kWidth * kHeight;
    EXPECT_EQ(memcmp(surfScalar.data, surfSIMD.data, compareByteSize), 0);

    surfaceRelease(ctx, &surfScalar);
    surfaceRelease(ctx, &surfSIMD);
}

// -----------------------------------------------------------------------------

std::string SharpenToString(const testing::TestParamInfo<FixedPoint_t>& value)
{
    const FixedPoint_t fp = value.param;
    return fixedPointToString(fp);
}

const std::vector<FixedPoint_t> kFixedPointUnsigned = {FPU8, FPU10, FPU12, FPU14};

INSTANTIATE_TEST_SUITE_P(SharpenTests, SharpenTest, testing::ValuesIn(kFixedPointUnsigned), SharpenToString);
