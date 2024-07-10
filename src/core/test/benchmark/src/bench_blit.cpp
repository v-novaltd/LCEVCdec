/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "bench_fixture.h"
#include "bench_utility.h"

extern "C"
{
#include "surface/blit.h"
#include "surface/blit_common.h"
#include "surface/surface.h"
}

// -----------------------------------------------------------------------------

extern "C"
{
BlitFunction_t surfaceBlitGetFunction(FixedPoint_t srcFP, FixedPoint_t dstFP,
                                      Interleaving_t interleaving, BlendingMode_t blending,
                                      CPUAccelerationFeatures_t preferredAccel);
}

// -----------------------------------------------------------------------------

class BlitFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        if (state.thread_index != 0) {
            return;
        }

        Super::SetUp(state);

        accel = simdFlag(static_cast<CPUAccelerationFeatures_t>(state.range(0)));
        srcFP = static_cast<FixedPoint_t>(state.range(1));
        dstFP = static_cast<FixedPoint_t>(state.range(2));
        dimensions = Resolution::getDimensions(static_cast<Resolution::Enum>(state.range(3)));

        surfaceIdle(&src);
        if (surfaceInitialise(ctx.memory, &src, srcFP, dimensions.width, dimensions.height,
                              dimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialise source surface");
            return;
        }

        surfaceIdle(&dst);
        if (surfaceInitialise(ctx.memory, &dst, dstFP, dimensions.width, dimensions.height,
                              dimensions.stride, ILNone)) {
            state.SkipWithError("Failed to initialise destination surface");
            return;
        }
    }

    void TearDown(benchmark::State& state) final
    {
        if (state.thread_index != 0) {
            return;
        }

        surfaceRelease(ctx.memory, &src);
        surfaceRelease(ctx.memory, &dst);

        Super::TearDown(state);
    }

    Surface_t src{};
    Surface_t dst{};
    CPUAccelerationFeatures_t accel = CAFSSE;
    FixedPoint_t srcFP = FPU8;
    FixedPoint_t dstFP = FPU8;
    Dimensions dimensions;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(BlitFixture, BlitAdd)(benchmark::State& state)
{
    BlitArgs_t args{};
    args.src = &src;
    args.dst = &dst;
    args.count = dimensions.height / state.threads;
    args.offset = args.count * state.thread_index;

    BlitFunction_t function = surfaceBlitGetFunction(srcFP, dstFP, ILNone, BMAdd, accel);

    if (!function) {
        state.SkipWithError("Failed to find blit function to benchmark");
        return;
    }

    if (state.thread_index == (state.threads - 1)) {
        // Last thread executes the remainder.
        args.count = dimensions.height - args.offset;
    }

    for (auto _ : state) {
        function(&args);
    }
}

BENCHMARK_DEFINE_F(BlitFixture, BlitCopy)(benchmark::State& state)
{
    BlitArgs_t args{};
    args.src = &src;
    args.dst = &dst;
    args.count = src.height;
    args.offset = 0;

    BlitFunction_t function = surfaceBlitGetFunction(srcFP, dstFP, ILNone, BMCopy, accel);

    if (!function) {
        state.SkipWithError("Failed to find copy function to benchmark");
        return;
    }

    for (auto _ : state) {
        function(&args);
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(BlitFixture, BlitAdd)
    ->ArgNames({"SIMD", "SrcFP", "DstFP", "Resolution"})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e4320p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e4320p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e4320p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e4320p})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e2160p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e2160p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e2160p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e2160p})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e1080p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e1080p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e1080p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e1080p})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e720p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e720p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e720p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e720p})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e540p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e540p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e540p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e540p})

    ->Args({CAFNone, FPS8, FPU8, Resolution::e360p})
    ->Args({CAFSSE, FPS8, FPU8, Resolution::e360p})
    ->Args({CAFNone, FPS10, FPU10, Resolution::e360p})
    ->Args({CAFSSE, FPS10, FPU10, Resolution::e360p})

    ->Unit(benchmark::kMillisecond)

    // ->ThreadRange(1, 8)
    ;

BENCHMARK_REGISTER_F(BlitFixture, BlitCopy)
    ->ArgNames({"SIMD", "SrcFP", "DstFP", "Resolution"})

    ->Args({false, FPS8, FPU8, Resolution::e4320p})
    ->Args({true, FPS8, FPU8, Resolution::e4320p})

    ->Args({false, FPU8, FPS8, Resolution::e4320p})
    ->Args({true, FPU8, FPS8, Resolution::e4320p})

    ->Args({false, FPU8, FPU8, Resolution::e4320p})
    ->Args({true, FPU8, FPU8, Resolution::e4320p})

    ->Args({false, FPS8, FPS8, Resolution::e4320p})
    ->Args({true, FPS8, FPS8, Resolution::e4320p})

    ->Args({false, FPU10, FPU8, Resolution::e4320p})
    ->Args({true, FPU10, FPU8, Resolution::e4320p})

    ->Args({false, FPU8, FPU10, Resolution::e4320p})
    ->Args({true, FPU8, FPU10, Resolution::e4320p})

    ->Unit(benchmark::kMillisecond)

    // ->ThreadRange(1, 8)
    ;

// -----------------------------------------------------------------------------
