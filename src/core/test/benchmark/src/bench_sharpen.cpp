/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"
#include "bench_utility.h"

extern "C"
{
#include "surface/sharpen_common.h"
#include "surface/surface.h"

SharpenFunction_t surfaceSharpenGetFunction(FixedPoint_t dstFP, CPUAccelerationFeatures_t preferredAccel);
}

// -----------------------------------------------------------------------------

class SharpenFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        accel = simdFlag(static_cast<CPUAccelerationFeatures_t>(state.range(0)));
        fp = static_cast<FixedPoint_t>(state.range(1));
        dimensions = Resolution::getDimensions(static_cast<Resolution::Enum>(state.range(2)));

        surfaceIdle(&ctx, &surfDst);
        if (surfaceInitialise(&ctx, &surfDst, fp, dimensions.width, dimensions.height,
                              dimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialize sharpen surface");
            return;
        }

        surfaceIdle(&ctx, &surfTmp);
        if (surfaceInitialise(&ctx, &surfTmp, fp, dimensions.width, dimensions.height,
                              dimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialize sharpen surface");
            return;
        }
    }

    void TearDown(benchmark::State& state) final
    {
        surfaceRelease(&ctx, &surfTmp);
        surfaceRelease(&ctx, &surfDst);
        Super::TearDown(state);
    }

    Surface_t surfDst;
    Surface_t surfTmp;
    CPUAccelerationFeatures_t accel = CAFSSE;
    FixedPoint_t fp;
    Dimensions dimensions;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(SharpenFixture, Sharpen)(benchmark::State& state)
{
    SharpenFunction_t function = surfaceSharpenGetFunction(fp, accel);

    if (!function) {
        state.SkipWithError("Failed to find sharpen function to benchmark");
        return;
    }

    SharpenArgs_t args{};
    args.src = &surfDst;
    args.tmpSurface = &surfTmp;
    args.dither = nullptr;
    args.strength = 0.5f;
    args.offset = 0;
    args.count = dimensions.height - 1;

    for (auto _ : state) {
        function(&args);
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(SharpenFixture, Sharpen)
    ->ArgNames({"SIMD", "FP", "Resolution"})

    ->Args({CAFNone, FPU8, Resolution::e4320p})
    ->Args({CAFSSE, FPU8, Resolution::e4320p})
    ->Args({CAFNone, FPU10, Resolution::e4320p})
    ->Args({CAFSSE, FPU10, Resolution::e4320p})

    ->Args({CAFNone, FPU8, Resolution::e1080p})
    ->Args({CAFSSE, FPU8, Resolution::e1080p})
    ->Args({CAFNone, FPU10, Resolution::e1080p})
    ->Args({CAFSSE, FPU10, Resolution::e1080p})

    ->Args({CAFNone, FPU8, Resolution::e720p})
    ->Args({CAFSSE, FPU8, Resolution::e720p})
    ->Args({CAFNone, FPU10, Resolution::e720p})
    ->Args({CAFSSE, FPU10, Resolution::e720p})

    ->Args({CAFNone, FPU8, Resolution::e540p})
    ->Args({CAFSSE, FPU8, Resolution::e540p})
    ->Args({CAFNone, FPU10, Resolution::e540p})
    ->Args({CAFSSE, FPU10, Resolution::e540p})

    ->Args({CAFNone, FPU8, Resolution::e360p})
    ->Args({CAFSSE, FPU8, Resolution::e360p})
    ->Args({CAFNone, FPU10, Resolution::e360p})
    ->Args({CAFSSE, FPU10, Resolution::e360p})

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------