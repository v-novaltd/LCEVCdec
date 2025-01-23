/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

    Surface_t surfScalar{};
    Surface_t surfSIMD{};

    surfaceIdle(&surfScalar);
    surfaceIdle(&surfSIMD);

    Memory_t memory = memoryWrapper.get();
    EXPECT_EQ(surfaceInitialise(memory, &surfScalar, fp, kWidth, kHeight, kWidth, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(memory, &surfSIMD, fp, kWidth, kHeight, kWidth, ILNone), 0);

    // Fill scalar surface and copy over.
    Context_t* ctx = contextWrapper.get();
    fillSurfaceWithNoise(surfScalar);
    surfaceBlit(logWrapper.get(), &(ctx->threadManager), ctx->cpuFeatures, &surfScalar, &surfSIMD, BMCopy);

    EXPECT_TRUE(surfaceSharpen(ctx->sharpen, &surfScalar, nullptr, CAFNone));
    EXPECT_TRUE(surfaceSharpen(ctx->sharpen, &surfSIMD, nullptr, simdFlag()));

    const auto compareByteSize = fixedPointByteSize(fp) * kWidth * kHeight;
    EXPECT_EQ(memcmp(surfScalar.data, surfSIMD.data, compareByteSize), 0);

    surfaceRelease(memory, &surfScalar);
    surfaceRelease(memory, &surfSIMD);
}

// -----------------------------------------------------------------------------

std::string SharpenToString(const testing::TestParamInfo<FixedPoint_t>& value)
{
    const FixedPoint_t fp = value.param;
    return fixedPointToString(fp);
}

const std::vector<FixedPoint_t> kFixedPointUnsigned = {FPU8, FPU10, FPU12, FPU14};

INSTANTIATE_TEST_SUITE_P(SharpenTests, SharpenTest, testing::ValuesIn(kFixedPointUnsigned), SharpenToString);
