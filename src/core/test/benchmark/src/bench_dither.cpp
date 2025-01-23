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
        ditherRegenerate(ctx.dither, strength, DTUniform, PPM_SPEED, Depth8);

        if (++strength > strengthMax) {
            strength = 1;
        }
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(DitherFixture, DitherRegenerate)->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------
