/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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
#include "find_assets_dir.h"
#include "LCEVC/utility/bin_reader.h"
#include "LCEVC/utility/filesystem.h"

extern "C"
{
#include "common/cmdbuffer.h"
#include "common/simd.h"
#include "common/tile.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "decode/decode_serial.h"
#include "decode/transform_unit.h"
#include "surface/surface.h"
}

using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets = findAssetsDir("src/core/test/assets");

std::map<int, std::string> kContentMap = {
    {0, "Tunnel_360x200_DDS_8bit_3f.bin"},
    {1, "Tunnel_360x200_DD_8bit_3f.bin"},
    {2, "Venice1_3840x2160_DDS_10bit_3f.bin"},
    {3, "Venice1_3840x2160_DD_10bit_3f.bin"},
};

// -----------------------------------------------------------------------------

class ApplyCmdBufferFixture : public lcevc_dec::Fixture
{
    using Super = lcevc_dec::Fixture;

    void SetUp(benchmark::State& state) final
    {
        Super::SetUp(state);

        surfaceIdle(&surface);
        surfaceInitialise(ctx.memory, &surface, surfaceFP, 3840, 2160, 3840, ILNone);
    }

    void TearDown(benchmark::State& state) final
    {
        surfaceRelease(ctx.memory, &surface);
        for (uint32_t loq = 0; loq < LOQEnhancedCount; loq++) {
            for (uint32_t planeIdx = 0; planeIdx < 3; planeIdx++) {
                for (uint32_t tileIdx = 0;
                     tileIdx < decodeSerialGetTileCount(ctx.decodeSerial[loq], planeIdx); tileIdx++) {
                    cmdBufferFree(tiles[planeIdx][tileIdx].cmdBuffer);
                }
            }
        }

        Super::TearDown(state);
    }

public:
    const Highlight_t* noHighlight = {};
    Surface_t surface{};
    CPUAccelerationFeatures_t accel = CAFSSE;
    FixedPoint_t surfaceFP = FPU8;
    TileState_t* tiles[3] = {};

    void getFrame(benchmark::State& state)
    {
        int contentId = static_cast<int>(state.range(0));
        accel = simdFlag(static_cast<CPUAccelerationFeatures_t>(state.range(1)));
        surfaceFP = static_cast<FixedPoint_t>(state.range(2));

        std::unique_ptr<BinReader> reader =
            createBinReader((kTestAssets / kContentMap[contentId]).string());
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;
        reader->read(decodeIndex, presentationIndex, payload);

        DeserialisedData_t* deserialisedData = &ctx.deserialised;
        deserialise(ctx.memory, ctx.log, payload.data(), (uint32_t)payload.size(), deserialisedData,
                    &ctx, Parse_Full);
        DequantArgs_t dequantArgs = {0};
        initialiseDequantArgs(deserialisedData, &dequantArgs);
        dequantCalculate(&ctx.dequant, &dequantArgs);
        contextSetDepths(&ctx);

        decodeSerialInitialize(ctx.memory, ctx.decodeSerial);
        DecodeSerialArgs_t params = {};
        params.loq = LOQ0;
        params.memory = ctx.memory;
        params.log = ctx.log;
        decodeSerial(&ctx, &params);
        for (uint8_t planeIdx = 0; planeIdx < 3; planeIdx++) {
            tiles[planeIdx] = decodeSerialGetTile(ctx.decodeSerial[LOQ0], planeIdx);
        }
    }
};

// -----------------------------------------------------------------------------

BENCHMARK_DEFINE_F(ApplyCmdBufferFixture, ApplyCmdBuffer)(benchmark::State& state)
{
    getFrame(state);
    CmdBufferApplicator_t applicatorFunction = NULL;
    if (accel == CAFNEON && detectSupportedSIMDFeatures() == CAFNEON) {
        applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockNEON;
    } else if (accel == CAFSSE && detectSupportedSIMDFeatures() == CAFSSE) {
        applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockSSE;
    } else if (accel == CAFNone) {
        applicatorFunction = (CmdBufferApplicator_t)cmdBufferApplicatorBlockScalar;
    }

    if (!applicatorFunction) {
        state.SkipWithError("Cannot benchmark this SIMD mode on this platform");
        return;
    }

    for (auto _ : state) {
        for (uint8_t planeIdx = 0; planeIdx < 3; planeIdx++) {
            for (uint8_t tileIdx = 0;
                 tileIdx < decodeSerialGetTileCount(ctx.decodeSerial[LOQ0], planeIdx); tileIdx++) {
                applicatorFunction(&tiles[planeIdx][tileIdx], 0, &surface, noHighlight);
            }
        }
    }
}

// -----------------------------------------------------------------------------

BENCHMARK_REGISTER_F(ApplyCmdBufferFixture, ApplyCmdBuffer)
    ->ArgNames({"Content", "SIMD", "FixedPointType"})

    /* Scalar */
    ->Args({0, CAFNone, FPU8})
    ->Args({0, CAFNone, FPU10})
    ->Args({0, CAFNone, FPU12})
    ->Args({0, CAFNone, FPU14})
    ->Args({0, CAFNone, FPS8})

    ->Args({1, CAFNone, FPU8})
    ->Args({1, CAFNone, FPU10})
    ->Args({1, CAFNone, FPU12})
    ->Args({1, CAFNone, FPU14})
    ->Args({1, CAFNone, FPS8})

    ->Args({2, CAFNone, FPU8})
    ->Args({2, CAFNone, FPU10})
    ->Args({2, CAFNone, FPU12})
    ->Args({2, CAFNone, FPU14})
    ->Args({2, CAFNone, FPS10})

    ->Args({3, CAFNone, FPU8})
    ->Args({3, CAFNone, FPU10})
    ->Args({3, CAFNone, FPU12})
    ->Args({3, CAFNone, FPU14})
    ->Args({3, CAFNone, FPS10})

    /* NEON */
    ->Args({0, CAFNEON, FPU8})
    ->Args({0, CAFNEON, FPU10})
    ->Args({0, CAFNEON, FPU12})
    ->Args({0, CAFNEON, FPU14})
    ->Args({0, CAFNEON, FPS8})

    ->Args({1, CAFNEON, FPU8})
    ->Args({1, CAFNEON, FPU10})
    ->Args({1, CAFNEON, FPU12})
    ->Args({1, CAFNEON, FPU14})
    ->Args({1, CAFNEON, FPS8})

    ->Args({2, CAFNEON, FPU8})
    ->Args({2, CAFNEON, FPU10})
    ->Args({2, CAFNEON, FPU12})
    ->Args({2, CAFNEON, FPU14})
    ->Args({2, CAFNEON, FPS10})

    ->Args({3, CAFNEON, FPU8})
    ->Args({3, CAFNEON, FPU10})
    ->Args({3, CAFNEON, FPU12})
    ->Args({3, CAFNEON, FPU14})
    ->Args({3, CAFNEON, FPS10})

    /* SSE */
    ->Args({0, CAFSSE, FPU8})
    ->Args({0, CAFSSE, FPU10})
    ->Args({0, CAFSSE, FPU12})
    ->Args({0, CAFSSE, FPU14})
    ->Args({0, CAFSSE, FPS8})

    ->Args({1, CAFSSE, FPU8})
    ->Args({1, CAFSSE, FPU10})
    ->Args({1, CAFSSE, FPU12})
    ->Args({1, CAFSSE, FPU14})
    ->Args({1, CAFSSE, FPS8})

    ->Args({2, CAFSSE, FPU8})
    ->Args({2, CAFSSE, FPU10})
    ->Args({2, CAFSSE, FPU12})
    ->Args({2, CAFSSE, FPU14})
    ->Args({2, CAFSSE, FPS10})

    ->Args({3, CAFSSE, FPU8})
    ->Args({3, CAFSSE, FPU10})
    ->Args({3, CAFSSE, FPU12})
    ->Args({3, CAFSSE, FPU14})
    ->Args({3, CAFSSE, FPS10})

    ->Unit(benchmark::kMillisecond);

// -----------------------------------------------------------------------------
