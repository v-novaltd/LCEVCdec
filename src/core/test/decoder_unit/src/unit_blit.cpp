/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include <range/v3/view.hpp>

#include <functional>
#include <random>
#include <sstream>

extern "C"
{
#include "surface/blit.h"
#include "surface/blit_common.h"
}

#include "unit_fixture.h"
#include "unit_utility.h"

extern "C"
{
BlitFunction_t surfaceBlitGetFunction(FixedPoint_t srcFP, FixedPoint_t dstFP,
                                      Interleaving_t interleaving, BlendingMode_t blending,
                                      CPUAccelerationFeatures_t preferredAccel);
}

namespace rg = ranges;
namespace rv = ranges::views;

// -----------------------------------------------------------------------------

constexpr uint32_t kWidth = 500;
constexpr uint32_t kHeight = 400;
constexpr uint32_t kDstStride = 512;

// -----------------------------------------------------------------------------

struct BlitTestParams
{
    FixedPoint_t srcFP;
    FixedPoint_t dstFP;
};

class CopyTest : public FixtureWithParam<BlitTestParams>
{};

TEST_P(CopyTest, CompareSIMD)
{
    const auto& params = GetParam();
    auto scalarFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMCopy, CAFNone);
    auto simdFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMCopy, simdFlag());

    Surface_t src{};
    Surface_t dstScalar{};
    Surface_t dstSIMD{};

    Context_t* ctx = context.get();

    surfaceIdle(ctx, &src);
    surfaceIdle(ctx, &dstScalar);
    surfaceIdle(ctx, &dstSIMD);

    EXPECT_EQ(surfaceInitialise(ctx, &src, params.srcFP, kWidth, kHeight, kWidth, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstScalar, params.dstFP, kWidth, kHeight, kDstStride, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstSIMD, params.dstFP, kWidth, kHeight, kDstStride, ILNone), 0);

    fillSurfaceWithNoise(src);

    BlitArgs_t args;
    args.src = &src;
    args.dst = &dstScalar;
    args.offset = 0;
    args.count = kHeight;

    scalarFunction(&args);

    args.dst = &dstSIMD;
    simdFunction(&args);

    const auto compareByteSize = fixedPointByteSize(params.dstFP) * kDstStride * kHeight;
    EXPECT_EQ(memcmp(dstScalar.data, dstSIMD.data, compareByteSize), 0);

    surfaceRelease(ctx, &src);
    surfaceRelease(ctx, &dstScalar);
    surfaceRelease(ctx, &dstSIMD);
}

// -----------------------------------------------------------------------------

class AddTest : public FixtureWithParam<BlitTestParams>
{};

TEST_P(AddTest, CompareSIMD)
{
    const auto& params = GetParam();
    auto scalarFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMAdd, CAFNone);
    auto simdFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMAdd, simdFlag());

    Surface_t src{};
    Surface_t dstScalar{};
    Surface_t dstSIMD{};

    Context_t* ctx = context.get();

    surfaceIdle(ctx, &src);
    surfaceIdle(ctx, &dstScalar);
    surfaceIdle(ctx, &dstSIMD);

    EXPECT_EQ(surfaceInitialise(ctx, &src, params.srcFP, kWidth, kHeight, kWidth, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstScalar, params.dstFP, kWidth, kHeight, kDstStride, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstSIMD, params.dstFP, kWidth, kHeight, kDstStride, ILNone), 0);

    fillSurfaceWithNoise(src);
    fillSurfaceWithNoise(dstScalar);

    // Copy scalar destination over to simd destination as we are testing additive
    // blits its useful to have plenty of random noise in both src and dst.
    auto copyFunction = surfaceBlitGetFunction(params.dstFP, params.dstFP, ILNone, BMCopy, simdFlag());
    BlitArgs_t copyArgs;
    copyArgs.src = &dstScalar;
    copyArgs.dst = &dstSIMD;
    copyArgs.offset = 0;
    copyArgs.count = kHeight;
    copyFunction(&copyArgs);

    BlitArgs_t args;
    args.src = &src;
    args.dst = &dstScalar;
    args.offset = 0;
    args.count = kHeight;

    scalarFunction(&args);

    args.dst = &dstSIMD;
    simdFunction(&args);

    const auto compareByteSize = fixedPointByteSize(params.dstFP) * kDstStride * kHeight;
    EXPECT_EQ(memcmp(dstScalar.data, dstSIMD.data, compareByteSize), 0);

    surfaceRelease(ctx, &src);
    surfaceRelease(ctx, &dstScalar);
    surfaceRelease(ctx, &dstSIMD);
}

// -----------------------------------------------------------------------------

// Helper for printing a meaningful name for the test parameter
std::string CopyToString(const testing::TestParamInfo<BlitTestParams>& value)
{
    const FixedPoint_t srcFP = value.param.srcFP;
    const FixedPoint_t dstFP = value.param.dstFP;

    std::stringstream ss;
    ss << fixedPointToString(srcFP) << "_to_" << fixedPointToString(dstFP);
    return ss.str();
}
//
std::string BlitToString(const testing::TestParamInfo<BlitTestParams>& value)
{
    const FixedPoint_t srcFP = value.param.srcFP;
    const FixedPoint_t dstFP = value.param.dstFP;

    std::stringstream ss;
    ss << fixedPointToString(srcFP) << "_on_" << fixedPointToString(dstFP);
    return ss.str();
}

// -----------------------------------------------------------------------------

const std::vector<FixedPoint_t> kFixedPointUnsigned = {FPU8, FPU10, FPU12, FPU14};

const std::vector<FixedPoint_t> kFixedPointAll = {FPU8, FPU10, FPU12, FPU14,
                                                  FPS8, FPS10, FPS12, FPS14};

// -----------------------------------------------------------------------------

const auto kCopyParams = rv::cartesian_product(kFixedPointAll, kFixedPointAll) |
                         rv::filter([](auto value) {
                             const FixedPoint_t srcFP = std::get<0>(value);
                             const FixedPoint_t dstFP = std::get<1>(value);
                             const bool srcSigned = fixedPointIsSigned(srcFP);
                             const bool dstSigned = fixedPointIsSigned(dstFP);
                             const auto srcDepth = bitdepthFromFixedPoint(srcFP);
                             const auto dstDepth = bitdepthFromFixedPoint(dstFP);

                             // Both signed is an identity copy, so omit them from perms
                             // since identities will be checked anyway.
                             const bool areBothSigned = srcSigned && dstSigned;

                             // Only perform tests for copies where the bit-depth is
                             // promoted, this is because we currently do not support
                             // bit-depth demotion.
                             const bool isDepthPromotion = dstDepth >= srcDepth;

                             return isDepthPromotion && !areBothSigned;
                         }) |
                         rv::transform([](auto value) {
                             return BlitTestParams{std::get<0>(value), std::get<1>(value)};
                         }) |
                         rg::to_vector;

INSTANTIATE_TEST_SUITE_P(BlitTests, CopyTest, testing::ValuesIn(kCopyParams), CopyToString);

// -----------------------------------------------------------------------------

const auto kBlitParams = kFixedPointAll | rv::transform([](auto value) {
                             return BlitTestParams{fixedPointHighPrecision(value), value};
                         }) |
                         rg::to_vector;

INSTANTIATE_TEST_SUITE_P(BlitTests, AddTest, testing::ValuesIn(kBlitParams), BlitToString);

// -----------------------------------------------------------------------------