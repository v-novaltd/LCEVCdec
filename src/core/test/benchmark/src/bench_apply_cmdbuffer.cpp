/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"
#include "bench_utility.h"

extern "C"
{
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"
}

// -----------------------------------------------------------------------------

extern "C"
{
ApplyCmdBufferFunction_t applyCmdBufferGetFunction(ApplyCmdBufferMode_t mode,
                                                   FixedPoint_t surfaceFP, TransformType_t transform,
                                                   CPUAccelerationFeatures_t preferredAccel);
}

// -----------------------------------------------------------------------------

static constexpr auto kTileSize = 32;
static constexpr auto kTileClearCount = 2000;
static constexpr auto kTransformCount = 100000;

static CmdBufferType_t CmdBufferTypeFromApplyMode(ApplyCmdBufferMode_t mode)
{
    if (mode == ACBM_Tiles) {
        return CBTCoordinates;
    }

    return CBTResiduals;
}

// -----------------------------------------------------------------------------

class ApplyCmdBufferFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        accel = simdFlag(static_cast<CPUAccelerationFeatures_t>(state.range(0)));
        surfaceFP = static_cast<FixedPoint_t>(state.range(1));
        mode = static_cast<ApplyCmdBufferMode_t>(state.range(2));
        transform = static_cast<TransformType_t>(state.range(3));

        surfaceIdle(&ctx, &surface);

        auto dimensions = Resolution::getDimensions(Resolution::e2160p);
        if (surfaceInitialise(&ctx, &surface, surfaceFP, dimensions.width, dimensions.height,
                              dimensions.width, ILNone)) {
            state.SkipWithError("Failed to initialize surface");
            return;
        }

        if (!InitializeCommandBuffer(dimensions)) {
            state.SkipWithError("Failed to initialize command buffer");
            return;
        }
    }

    void TearDown(benchmark::State& state) final
    {
        surfaceRelease(&ctx, &surface);
        cmdBufferFree(cmdbuffer);

        Super::TearDown(state);
    }

    bool InitializeCommandBuffer(const Dimensions& dimensions)
    {
        const auto type = CmdBufferTypeFromApplyMode(mode);
        if (!cmdBufferInitialise(ctx.memory, &cmdbuffer, type)) {
            return false;
        }

        // populate command buffer.

        if (type == CBTCoordinates) {
            const auto tilesAcross = (dimensions.width + kTileSize - 1) / kTileSize;
            const auto tilesDown = (dimensions.height + kTileSize - 1) / kTileSize;
            const auto tilesTotal = tilesAcross * tilesDown;
            const auto tilesStep = tilesTotal / kTileClearCount;

            cmdBufferReset(cmdbuffer, 0);

            auto tileIndex = 0u;
            for (auto i = 0; i < kTileClearCount; ++i) {
                const auto y = tileIndex / tilesAcross;
                const auto x = tileIndex % tilesAcross;
                cmdBufferAppendCoord(cmdbuffer, static_cast<int16_t>(x), static_cast<int16_t>(y));
                tileIndex += tilesStep;
            }
        } else {
            const auto transformSize = transformTypeDimensions(transform);
            const auto tuAcross = (dimensions.width + transformSize - 1) / transformSize;
            const auto tuDown = (dimensions.height + transformSize - 1) / transformSize;
            const auto tuTotal = tuAcross * tuDown;
            const auto tuStep = tuTotal / kTransformCount;

            const int16_t data[16] = {0};

            cmdBufferReset(cmdbuffer, transformTypeLayerCount(transform));

            auto tuIndex = 0u;
            for (auto i = 0; i < kTransformCount; ++i) {
                const auto y = tuIndex / tuAcross;
                const auto x = tuIndex % tuAcross;
                cmdBufferAppend(cmdbuffer, static_cast<int16_t>(x), static_cast<int16_t>(y), data);
                tuIndex += tuStep;
            }
        }

        return true;
    }

    Surface_t surface{};
    CPUAccelerationFeatures_t accel = CAFSSE;
    FixedPoint_t surfaceFP = FPU8;
    CmdBuffer_t* cmdbuffer = nullptr;
    ApplyCmdBufferMode_t mode = ACBM_Inter;
    TransformType_t transform = TransformDD;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(ApplyCmdBufferFixture, ApplyCmdBuffer)(benchmark::State& state)
{
    ApplyCmdBufferFunction_t function = applyCmdBufferGetFunction(mode, surfaceFP, transform, accel);

    if (!function) {
        state.SkipWithError("Failed to find apply cmdbuffer function to benchmark");
        return;
    }

    auto data = cmdBufferGetData(cmdbuffer);

    const ApplyCmdBufferArgs_t args{
        &surface,
        data.coordinates,
        data.residuals,
        data.count,
    };

    for (auto _ : state) {
        function(&args);
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(ApplyCmdBufferFixture, ApplyCmdBuffer)
    ->ArgNames({"SIMD", "FixedPointType", "ApplyType", "Transform"})

    // Inter DD
    ->Args({CAFNone, FPU8, ACBM_Inter, TransformDD})
    ->Args({CAFSSE, FPU8, ACBM_Inter, TransformDD})
    ->Args({CAFNone, FPU10, ACBM_Inter, TransformDD})
    ->Args({CAFSSE, FPU10, ACBM_Inter, TransformDD})
    ->Args({CAFNone, FPU12, ACBM_Inter, TransformDD})
    ->Args({CAFSSE, FPU12, ACBM_Inter, TransformDD})
    ->Args({CAFNone, FPU14, ACBM_Inter, TransformDD})
    ->Args({CAFSSE, FPU14, ACBM_Inter, TransformDD})
    ->Args({CAFNone, FPS8, ACBM_Inter, TransformDD})
    ->Args({CAFSSE, FPS8, ACBM_Inter, TransformDD})

    // Inter DDS
    ->Args({CAFNone, FPU8, ACBM_Inter, TransformDDS})
    ->Args({CAFSSE, FPU8, ACBM_Inter, TransformDDS})
    ->Args({CAFNone, FPU10, ACBM_Inter, TransformDDS})
    ->Args({CAFSSE, FPU10, ACBM_Inter, TransformDDS})
    ->Args({CAFNone, FPU12, ACBM_Inter, TransformDDS})
    ->Args({CAFSSE, FPU12, ACBM_Inter, TransformDDS})
    ->Args({CAFNone, FPU14, ACBM_Inter, TransformDDS})
    ->Args({CAFSSE, FPU14, ACBM_Inter, TransformDDS})
    ->Args({CAFNone, FPS8, ACBM_Inter, TransformDDS})
    ->Args({CAFSSE, FPS8, ACBM_Inter, TransformDDS})

    // Intra
    ->Args({CAFNone, FPS8, ACBM_Intra, TransformDD})
    ->Args({CAFSSE, FPS8, ACBM_Intra, TransformDD})
    ->Args({CAFNone, FPS8, ACBM_Intra, TransformDDS})
    ->Args({CAFSSE, FPS8, ACBM_Intra, TransformDDS})

    // Tile Clear
    ->Args({CAFNone, FPS8, ACBM_Tiles, TransformDD})
    ->Args({CAFSSE, FPS8, ACBM_Tiles, TransformDD})

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------
