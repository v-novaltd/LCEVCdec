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

#include "bench_fixture.h"
#include "bench_utility.h"

extern "C"
{
#include "common/simd.h"
#include "surface/surface.h"
#include "surface/upscale_common.h"
}

// -----------------------------------------------------------------------------

extern "C"
{
UpscaleHorizontal_t getHorizontalFunction(Context_t* ctx, FixedPoint_t srcFP, FixedPoint_t dstFP,
                                          FixedPoint_t baseFP, Interleaving_t interleaving,
                                          CPUAccelerationFeatures_t preferredAccel);

UpscaleVertical_t getVerticalFunction(Context_t* ctx, FixedPoint_t srcFP, FixedPoint_t dstFP,
                                      CPUAccelerationFeatures_t preferredAccel, uint32_t* xStep);
}

// -----------------------------------------------------------------------------
// This benchmark is performing 1080p to 2160p upscaling for both horizontal and
// vertical directions.
////
// This test tries to simulate something like real world usage as though it was being
// called on actual surfaces at run time, with this mode of operation we will walk
// across the entire input surface (relevant to the operation)
//
//      This will either be performing:
//          Horizontal: 960x540 -> 1920x540
//          Vertical:   960x540 -> 960x1080
//
// The absolute numbers here must be taken with caution, the more interesting values
// are the relative configurations to observe what things cost on the platform
// this test is being run.

class UpscaleFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) override
    {
        Super::SetUp(state);

        simd = static_cast<bool>(state.range(0));
        type = static_cast<UpscaleType_t>(state.range(1));
        srcFP = static_cast<FixedPoint_t>(state.range(2));
        dstFP = static_cast<FixedPoint_t>(state.range(3));

        const Dimensions dstDimensions = Resolution::getDimensions(Resolution::e2160p);
        const Dimensions srcDimensions = dstDimensions.Downscale();

        if (!upscaleGetKernel(ctx.log, &ctx, type, &kernel)) {
            state.SkipWithError("Failed to query upscale kernel");
            return;
        }

        surfaceIdle(&src);
        if (surfaceInitialise(ctx.memory, &src, srcFP, srcDimensions.width, srcDimensions.height,
                              srcDimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialise source surface");
            return;
        }

        surfaceIdle(&dst);
        if (surfaceInitialise(ctx.memory, &dst, dstFP, dstDimensions.width, dstDimensions.height,
                              dstDimensions.stride, ILNone)) {
            state.SkipWithError("Failed to initialise destination surface");
            return;
        }
    }

    void TearDown(benchmark::State& state) override
    {
        surfaceRelease(ctx.memory, &dst);
        surfaceRelease(ctx.memory, &src);

        Super::TearDown(state);
    }

    Surface_t src{};
    Surface_t dst{};
    bool simd = false;
    FixedPoint_t srcFP = FPU8;
    FixedPoint_t dstFP = FPU8;
    UpscaleType_t type;
    Kernel_t kernel;
};

class UpscaleHoriFixture : public UpscaleFixture
{
public:
    using Super = UpscaleFixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        dither = static_cast<bool>(state.range(4));
        pa = static_cast<int32_t>(state.range(5));

        const Dimensions dstDimensions = Resolution::getDimensions(Resolution::e2160p);
        const Dimensions srcDimensions = dstDimensions.Downscale();

        // Secondary surface for PA, this is just mirroring a real use-case
        // during horizontal scaling for 2D upscale.
        surfaceIdle(&base);
        if (surfaceInitialise(ctx.memory, &base, srcFP, srcDimensions.width, srcDimensions.height,
                              srcDimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialise source surface");
            return;
        }
    }

    void TearDown(benchmark::State& state) final
    {
        surfaceRelease(ctx.memory, &base);

        Super::TearDown(state);
    }

    Surface_t base{};
    bool dither = false;
    int32_t pa = 0;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(UpscaleHoriFixture, UpscaleHorizontal)(benchmark::State& state)
{
    const CPUAccelerationFeatures_t cpuFeatures = simd ? detectSupportedSIMDFeatures() : CAFNone;
    UpscaleHorizontal_t function = getHorizontalFunction(&ctx, srcFP, dstFP, srcFP, ILNone, cpuFeatures);

    if (!function) {
        state.SkipWithError("Failed to find horizontal function to benchmark");
        return;
    }

    for (auto _ : state) {
        for (uint32_t y = 0; y < src.height; y += 2) {
            const uint8_t* srcRows[2] = {surfaceGetLine(&src, y), surfaceGetLine(&src, y + 1)};
            uint8_t* dstRows[2] = {surfaceGetLine(&dst, y), surfaceGetLine(&dst, y + 1)};
            const uint8_t* baseRows[2] = {nullptr, nullptr};

            // The pointer validity implies the behaviour of PA.
            if (pa >= 1) {
                baseRows[0] = surfaceGetLine(&base, y);
                if (pa == 1) {
                    baseRows[1] = surfaceGetLine(&base, y + 1);
                }
            }

            function(ctx.dither, srcRows, dstRows, baseRows, src.width, 0, src.width, &kernel);
        }
    }
}

BENCHMARK_DEFINE_F(UpscaleFixture, UpscaleVertical)(benchmark::State& state)
{
    const CPUAccelerationFeatures_t cpuFeatures = simd ? detectSupportedSIMDFeatures() : CAFNone;

    uint32_t xStep = 1;
    UpscaleVertical_t function = getVerticalFunction(&ctx, srcFP, dstFP, cpuFeatures, &xStep);

    // @todo: How does the decoder handle the edge-case?
    // UpscaleVertical_t functionScalar = getVerticalFunction(&ctx, srcFP, dstFP, CAFNone, &xStep);

    if (!function) {
        state.SkipWithError("Failed to find vertical function to benchmark");
        return;
    }

    const uint32_t srcStep = xStep * ldlFixedPointByteSize(src.type);
    const uint32_t dstStep = xStep * ldlFixedPointByteSize(dst.type);

    for (auto _ : state) {
        const uint8_t* srcPtr = surfaceGetLine(&src, 0);
        uint8_t* dstPtr = surfaceGetLine(&dst, 0);

        for (uint32_t x = 0; x < src.width; x += xStep, srcPtr += srcStep, dstPtr += dstStep) {
            function(srcPtr, src.stride, dstPtr, dst.stride, 0, src.height, src.height, &kernel);
        }
    }
}

// -----------------------------------------------------------------------------

#define UPSCALE_REGISTER_HELPER_ENTRY(UPSCALE, DITHER, PA) \
    ->Args({false, UPSCALE, FPU8, FPU8, DITHER, PA})       \
        ->Args({true, UPSCALE, FPU8, FPU8, DITHER, PA})    \
        ->Args({false, UPSCALE, FPS8, FPS8, DITHER, PA})   \
        ->Args({true, UPSCALE, FPS8, FPS8, DITHER, PA})

#define UPSCALE_REGISTER_HELPER_PA(UPSCALE, DITHER) \
    UPSCALE_REGISTER_HELPER_ENTRY(UPSCALE, DITHER, 0)
/*    UPSCALE_REGISTER_HELPER_ENTRY(UPSCALE, DITHER, 1) \
      UPSCALE_REGISTER_HELPER_ENTRY(UPSCALE, DITHER, 2)   */

#define UPSCALE_REGISTER_HELPER_UPSCALE(UPSCALE) \
    UPSCALE_REGISTER_HELPER_PA(UPSCALE, false)   \
    UPSCALE_REGISTER_HELPER_PA(UPSCALE, true)

// UPSCALE_REGISTER_HELPER_PA(UPSCALE, true)

#define UPSCALE_REGISTER_HELPER()             \
    UPSCALE_REGISTER_HELPER_UPSCALE(USLinear) \
    UPSCALE_REGISTER_HELPER_UPSCALE(USCubic)

BENCHMARK_REGISTER_F(UpscaleHoriFixture, UpscaleHorizontal)
    ->ArgNames({"SIMD", "Type", "SrcFP", "DstFP", "Dither", "PA"})

        UPSCALE_REGISTER_HELPER()

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------

#define UPSCALE_REGISTER_VERT_HELPER_ENTRY(UPSCALE) \
    ->Args({false, UPSCALE, FPU8, FPU8})            \
        ->Args({true, UPSCALE, FPU8, FPU8})         \
        ->Args({false, UPSCALE, FPS8, FPS8})        \
        ->Args({true, UPSCALE, FPS8, FPS8})

#define UPSCALE_REGISTER_VERT_HELPER()           \
    UPSCALE_REGISTER_VERT_HELPER_ENTRY(USLinear) \
    UPSCALE_REGISTER_VERT_HELPER_ENTRY(USCubic)

BENCHMARK_REGISTER_F(UpscaleFixture, UpscaleVertical)
    ->ArgNames({"SIMD", "Type", "SrcFP", "DstFP"})

        UPSCALE_REGISTER_VERT_HELPER()

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------
