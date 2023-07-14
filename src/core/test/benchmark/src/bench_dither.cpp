/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"

extern "C"
{
#include "common/dither.h"
}

// -----------------------------------------------------------------------------

class DitherFixture : public lcevc_dec::Fixture
{
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final { Super::SetUp(state); }

    void TearDown(benchmark::State& state) final { Super::TearDown(state); }
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(DitherFixture, DitherRegenerate)(benchmark::State& state)
{
    uint8_t strength = 1;
    static const uint8_t strengthMax = 31;
    for (auto _ : state) {
        ditherRegenerate(ctx.dither, strength, DTUniform);

        if (++strength > strengthMax) {
            strength = 1;
        }
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(DitherFixture, DitherRegenerate)->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------