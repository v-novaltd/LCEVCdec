/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"
#include "bench_utility.h"

extern "C"
{
#include "decode/transform.h"
}

// -----------------------------------------------------------------------------

class TransformFixture : public lcevc_dec::Fixture
{
public:
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        accel = simdFlag(static_cast<CPUAccelerationFeatures_t>(state.range(0)));
        scaling = static_cast<ScalingMode_t>(state.range(1));
        transform = static_cast<TransformType_t>(state.range(2));
    }

    CPUAccelerationFeatures_t accel;
    ScalingMode_t scaling;
    TransformType_t transform;
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(TransformFixture, Transform)(benchmark::State& state)
{
    TransformFunction_t function = transformGetFunction(transform, scaling, accel);

    if (!function) {
        state.SkipWithError("Failed to find transform function to benchmark");
        return;
    }

    const auto layerCount = transformTypeLayerCount(transform);

    std::vector<int16_t> coeffs(layerCount, 1);
    std::vector<int16_t> residuals(layerCount);

    for (auto _ : state) {
        function(coeffs.data(), residuals.data());
    }
}

BENCHMARK_DEFINE_F(TransformFixture, DequantTransform)(benchmark::State& state)
{
    DequantTransformFunction_t function = dequantTransformGetFunction(transform, scaling, accel);

    if (!function) {
        state.SkipWithError("Failed to find transform function to benchmark");
        return;
    }

    const auto layerCount = transformTypeLayerCount(transform);

    std::vector<int16_t> coeffs(layerCount, 1);
    std::vector<int16_t> residuals(layerCount);

    Dequant_t dequant = {};

    for (auto i = 0; i < layerCount; ++i) {
        dequant.stepWidth[TSInter][i] = 2;
        dequant.offset[TSInter][i] = 4;
    }

    // Load up SIMD registers
#if VN_CORE_FEATURE(SSE)
    dequant.stepWidthVector[TSInter][0] = _mm_load_si128((const __m128i*)&dequant.stepWidth[TSInter][0]);
    dequant.stepWidthVector[TSInter][1] = _mm_load_si128((const __m128i*)&dequant.stepWidth[TSInter][8]);
    dequant.offsetVector[TSInter][0] = _mm_load_si128((const __m128i*)&dequant.offset[TSInter][0]);
    dequant.offsetVector[TSInter][1] = _mm_load_si128((const __m128i*)&dequant.offset[TSInter][8]);
#elif VN_CORE_FEATURE(NEON)
    dequant.stepWidthVector[TSInter][0] = vld1q_s16(&dequant.stepWidth[TSInter][0]);
    dequant.stepWidthVector[TSInter][1] = vld1q_s16(&dequant.stepWidth[TSInter][8]);
    dequant.offsetVector[TSInter][0] = vld1q_s16(&dequant.offset[TSInter][0]);
    dequant.offsetVector[TSInter][1] = vld1q_s16(&dequant.offset[TSInter][8]);
#endif

    for (auto _ : state) {
        function(&dequant, TSInter, coeffs.data(), residuals.data());
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(TransformFixture, Transform)
    ->ArgNames({"SIMD", "Scaling", "Transform"})

    // DD
    ->Args({CAFNone, Scale2D, TransformDD})
    ->Args({CAFSSE, Scale2D, TransformDD})
    ->Args({CAFNone, Scale1D, TransformDD})
    ->Args({CAFSSE, Scale1D, TransformDD})

    // DDS
    ->Args({CAFNone, Scale2D, TransformDDS})
    ->Args({CAFSSE, Scale2D, TransformDDS})
    ->Args({CAFNone, Scale1D, TransformDDS})
    ->Args({CAFSSE, Scale1D, TransformDDS})

    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(TransformFixture, DequantTransform)
    ->ArgNames({"SIMD", "Scaling", "Transform"})

    // DD
    ->Args({CAFNone, Scale2D, TransformDD})
    ->Args({CAFSSE, Scale2D, TransformDD})
    ->Args({CAFNone, Scale1D, TransformDD})
    ->Args({CAFSSE, Scale1D, TransformDD})

    // DDS
    ->Args({CAFNone, Scale2D, TransformDDS})
    ->Args({CAFSSE, Scale2D, TransformDDS})
    ->Args({CAFNone, Scale1D, TransformDDS})
    ->Args({CAFSSE, Scale1D, TransformDDS})

    ->Unit(benchmark::kNanosecond);

// -----------------------------------------------------------------------------
