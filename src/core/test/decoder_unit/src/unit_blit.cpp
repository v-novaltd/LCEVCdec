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

class BlitTest : public FixtureWithParam<BlitTestParams>
{
    using BaseClass = FixtureWithParam<BlitTestParams>;

public:
    void SetUp() override
    {
        BaseClass::SetUp();

        const auto& params = GetParam();
        m_scalarFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMCopy, CAFNone);
        m_simdFunction = surfaceBlitGetFunction(params.srcFP, params.dstFP, ILNone, BMCopy, simdFlag());

        surfaceIdle(&m_src);
        surfaceIdle(&m_dstScalar);
        surfaceIdle(&m_dstSIMD);

        Memory_t memory = memoryWrapper.get();
        EXPECT_EQ(surfaceInitialise(memory, &m_src, params.srcFP, kWidth, kHeight, kWidth, ILNone), 0);
        EXPECT_EQ(surfaceInitialise(memory, &m_dstScalar, params.dstFP, kWidth, kHeight, kDstStride, ILNone),
                  0);
        EXPECT_EQ(surfaceInitialise(memory, &m_dstSIMD, params.dstFP, kWidth, kHeight, kDstStride, ILNone),
                  0);
    }

    void TearDown() override
    {
        Memory_t memory = memoryWrapper.get();
        surfaceRelease(memory, &m_src);
        surfaceRelease(memory, &m_dstScalar);
        surfaceRelease(memory, &m_dstSIMD);
    }

protected:
    Surface_t m_src{};
    Surface_t m_dstScalar{};
    Surface_t m_dstSIMD{};

    BlitFunction_t m_scalarFunction{};
    BlitFunction_t m_simdFunction{};
};

// -----------------------------------------------------------------------------

class CopyTest : public BlitTest
{};

TEST_P(CopyTest, CompareSIMD)
{
    fillSurfaceWithNoise(m_src);

    BlitArgs_t args;
    args.src = &m_src;
    args.dst = &m_dstScalar;
    args.offset = 0;
    args.count = kHeight;
    m_scalarFunction(&args);

    args.dst = &m_dstSIMD;
    m_simdFunction(&args);

    const auto& params = GetParam();
    const auto compareByteSize = fixedPointByteSize(params.dstFP) * kDstStride * kHeight;
    EXPECT_EQ(memcmp(m_dstScalar.data, m_dstSIMD.data, compareByteSize), 0);
}

// -----------------------------------------------------------------------------

class AddTest : public BlitTest
{};

TEST_P(AddTest, CompareSIMD)
{
    fillSurfaceWithNoise(m_src);
    fillSurfaceWithNoise(m_dstScalar);

    // Copy scalar destination over to simd destination. As we are testing additive
    // blits, it's useful to have plenty of random noise in both m_src and dst.
    const auto& params = GetParam();
    auto copyFunction = surfaceBlitGetFunction(params.dstFP, params.dstFP, ILNone, BMCopy, simdFlag());
    BlitArgs_t copyArgs;
    copyArgs.src = &m_dstScalar;
    copyArgs.dst = &m_dstSIMD;
    copyArgs.offset = 0;
    copyArgs.count = kHeight;
    copyFunction(&copyArgs);

    BlitArgs_t args;
    args.src = &m_src;
    args.dst = &m_dstScalar;
    args.offset = 0;
    args.count = kHeight;
    m_scalarFunction(&args);

    args.dst = &m_dstSIMD;
    m_simdFunction(&args);

    const auto compareByteSize = fixedPointByteSize(params.dstFP) * kDstStride * kHeight;
    EXPECT_EQ(memcmp(m_dstScalar.data, m_dstSIMD.data, compareByteSize), 0);
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
