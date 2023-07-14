/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include <range/v3/view.hpp>

#include <algorithm>

extern "C"
{
#include "common/cmdbuffer.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/blit.h"
}

#include "unit_fixture.h"
#include "unit_utility.h"

extern "C"
{
ApplyCmdBufferFunction_t applyCmdBufferGetFunction(ApplyCmdBufferMode_t mode,
                                                   FixedPoint_t surfaceFP, TransformType_t transform,
                                                   CPUAccelerationFeatures_t preferredAccel);
}

namespace rg = ranges;
namespace rv = ranges::views;

// -----------------------------------------------------------------------------

struct ApplyCmdBufferSIMDTestParams
{
    ApplyCmdBufferMode_t mode;
    FixedPoint_t format;
    TransformType_t transform;
};

class ApplyCmdBufferSIMDTest : public FixtureWithParam<ApplyCmdBufferSIMDTestParams>
{};

TEST_P(ApplyCmdBufferSIMDTest, Test)
{
    static constexpr uint32_t kWidth = 500;
    static constexpr uint32_t kHeight = 400;
    static constexpr uint32_t kStride = 522;
    static constexpr double kBufferOccupancy = 0.4;

    const auto& params = GetParam();
    const bool applyTileClear = (params.mode == ACBM_Tiles);
    auto scalarFunction = applyCmdBufferGetFunction(params.mode, params.format, params.transform, CAFNone);
    auto simdFunction =
        applyCmdBufferGetFunction(params.mode, params.format, params.transform, simdFlag());

    if (scalarFunction == simdFunction) {
        GTEST_SKIP() << "Skipping SIMD comparison as there is no SIMD implementation for "
                        "these parameters";
    }

    Surface_t dstScalar{};
    Surface_t dstSIMD{};

    Context_t* ctx = context.get();

    surfaceIdle(ctx, &dstScalar);
    surfaceIdle(ctx, &dstSIMD);

    EXPECT_EQ(surfaceInitialise(ctx, &dstScalar, params.format, kWidth, kHeight, kStride, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstSIMD, params.format, kWidth, kHeight, kStride, ILNone), 0);

    fillSurfaceWithNoise(dstScalar);
    surfaceBlit(ctx, &dstScalar, &dstSIMD, BMCopy);

    CmdBuffer_t* cmdBuffer;
    EXPECT_TRUE(cmdBufferInitialise(ctx->memory, &cmdBuffer, applyTileClear ? CBTCoordinates : CBTResiduals));

    fillCmdBufferWithNoise(cmdBuffer, params.format, kWidth, kHeight,
                           applyTileClear ? 0 : transformTypeLayerCount(params.transform),
                           kBufferOccupancy);

    const auto cmdBufferData = cmdBufferGetData(cmdBuffer);

    ApplyCmdBufferArgs_t args{&dstScalar, cmdBufferData.coordinates, cmdBufferData.residuals,
                              cmdBufferData.count};
    scalarFunction(&args);

    args.surface = &dstSIMD;
    simdFunction(&args);

    const auto compareByteSize = fixedPointByteSize(params.format) * kStride * kHeight;
    EXPECT_EQ(memcmp(dstScalar.data, dstSIMD.data, compareByteSize), 0);

    cmdBufferFree(cmdBuffer);
    surfaceRelease(ctx, &dstScalar);
    surfaceRelease(ctx, &dstSIMD);
}

// -----------------------------------------------------------------------------

struct ApplyCmdBufferIntraCorrectnessTestParams
{
    CPUAccelerationFeatures_t accel;
    TransformType_t transform;
};

class ApplyCmdBufferIntraCorrectnessTest : public FixtureWithParam<ApplyCmdBufferIntraCorrectnessTestParams>
{};

TEST_P(ApplyCmdBufferIntraCorrectnessTest, Test)
{
    static constexpr uint32_t kFullTransformCount = 3;

    const auto& params = GetParam();

    auto function = applyCmdBufferGetFunction(ACBM_Intra, FPS8, params.transform, simdFlag(params.accel));

    // @todo: some of this can be replaced.
    const int16_t residualsZero[RCLayerMaxCount] = {0};
    int16_t residualsMin[RCLayerMaxCount];
    int16_t residualsMax[RCLayerMaxCount];
    int16_t residualsHalfPositive[RCLayerMaxCount];
    const int32_t residualMaxValue = fixedPointMaxValue(FPS8);
    std::fill_n(residualsMin, RCLayerMaxCount, fixedPointMinValue(FPS8));
    std::fill_n(residualsMax, RCLayerMaxCount, residualMaxValue);
    std::fill_n(residualsHalfPositive, RCLayerMaxCount, residualMaxValue);

    const auto transformSize = static_cast<uint32_t>(transformTypeDimensions(params.transform));

    // Ensure surface has a partial transform on the edge.
    const auto surfaceWidth = transformSize * kFullTransformCount + (transformSize >> 1);
    const auto surfaceHeight = transformSize + (transformSize >> 1);

    Surface_t dst{};

    Context_t* ctx = context.get();
    surfaceIdle(ctx, &dst);

    EXPECT_EQ(surfaceInitialise(ctx, &dst, FPS8, surfaceWidth, surfaceHeight, surfaceWidth, ILNone), 0);
    surfaceZero(ctx, &dst);

    CmdBuffer_t* cmdBuffer;
    EXPECT_TRUE(cmdBufferInitialise(ctx->memory, &cmdBuffer, CBTResiduals));
    EXPECT_TRUE(cmdBufferReset(cmdBuffer, transformTypeLayerCount(params.transform)));

    // Transform 0 = write 0
    cmdBufferAppend(cmdBuffer, 0, 0, residualsZero);
    // Transform 1 = write min
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize), 0, residualsMin);
    // Transform 2 = write max
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize * 2), 0, residualsMax);
    // Transform 3 = write half-positive (partial transform on right edge).
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize * 3), 0, residualsHalfPositive);
    // Transform 4,5,6,7 = write several bottom partials
    // This is designed to ensure out of bounds memory could occur.
    cmdBufferAppend(cmdBuffer, 0, static_cast<int16_t>(transformSize), residualsMax);
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize),
                    static_cast<int16_t>(transformSize), residualsMax);
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize * 2),
                    static_cast<int16_t>(transformSize), residualsMax);
    cmdBufferAppend(cmdBuffer, static_cast<int16_t>(transformSize * 3),
                    static_cast<int16_t>(transformSize), residualsMax);

    const auto cmdBufferData = cmdBufferGetData(cmdBuffer);

    ApplyCmdBufferArgs_t args{&dst, cmdBufferData.coordinates, cmdBufferData.residuals,
                              cmdBufferData.count};
    function(&args);

    const size_t byteCompareSize = sizeof(int16_t) * transformSize;
    const size_t partialByteCompareSize = sizeof(int16_t) * (transformSize >> 1);

    for (uint32_t y = 0; y < transformSize; ++y) {
        const auto* line = reinterpret_cast<const int16_t*>(surfaceGetLine(&dst, y));
        EXPECT_EQ(memcmp(line, residualsZero, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize], residualsMin, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize * 2], residualsMax, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize * 3], residualsHalfPositive, partialByteCompareSize), 0);
    }

    // Bottom edge partial transform.
    for (uint32_t y = transformSize; y < surfaceHeight; ++y) {
        const auto* line = reinterpret_cast<const int16_t*>(surfaceGetLine(&dst, y));
        EXPECT_EQ(memcmp(line, residualsMax, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize], residualsMax, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize * 2], residualsMax, byteCompareSize), 0);
        EXPECT_EQ(memcmp(&line[transformSize * 3], residualsMax, partialByteCompareSize), 0);
    }

    cmdBufferFree(cmdBuffer);
    surfaceRelease(ctx, &dst);
}

// -----------------------------------------------------------------------------

struct ApplyCmdBufferInterCorrectnessTestParams
{
    CPUAccelerationFeatures_t accel;
    FixedPoint_t format;
    TransformType_t transform;
};

class ApplyCmdBufferInterCorrectnessTest : public FixtureWithParam<ApplyCmdBufferInterCorrectnessTestParams>
{};

TEST_P(ApplyCmdBufferInterCorrectnessTest, Test)
{
    static constexpr uint32_t kFullTransformCount = 4;

    const auto& params = GetParam();

    auto function =
        applyCmdBufferGetFunction(ACBM_Inter, params.format, params.transform, simdFlag(params.accel));

    const auto transformSize = static_cast<uint32_t>(transformTypeDimensions(params.transform));

    const auto surfaceWidth = transformSize * kFullTransformCount + (transformSize >> 1);
    const auto surfaceHeight = transformSize + (transformSize >> 1);

    Surface_t dst{};
    Surface_t dstExpected{};

    Context_t* ctx = context.get();
    surfaceIdle(ctx, &dst);
    surfaceIdle(ctx, &dstExpected);

    EXPECT_EQ(surfaceInitialise(ctx, &dst, params.format, surfaceWidth, surfaceHeight, surfaceWidth, ILNone),
              0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstExpected, params.format, surfaceWidth, surfaceHeight,
                                surfaceWidth, ILNone),
              0);
    surfaceZero(ctx, &dst);
    surfaceZero(ctx, &dstExpected);

    CmdBuffer_t* cmdBuffer;
    EXPECT_TRUE(cmdBufferInitialise(ctx->memory, &cmdBuffer, CBTResiduals));
    EXPECT_TRUE(cmdBufferReset(cmdBuffer, transformTypeLayerCount(params.transform)));

    const auto writeTransformData = [&params, &transformSize, &cmdBuffer, &dst,
                                     &dstExpected](uint32_t transformX, uint32_t transformY,
                                                   int32_t sourceValue, int32_t addAmount,
                                                   int32_t expectedValue, bool bExpectedMax = false) {
        // Calculate the residual.
        const auto fpLow = fixedPointLowPrecision(params.format);
        const int32_t sourceHP = calculateFixedPointHighPrecisionValue(fpLow, sourceValue);
        const int32_t targetValue = sourceValue + addAmount;
        const int32_t destinationHP = calculateFixedPointHighPrecisionValue(fpLow, targetValue);
        const int32_t expectedHP = calculateFixedPointHighPrecisionValue(fpLow, expectedValue);

        const int32_t residual = destinationHP - sourceHP;

        // Calculate pixel coords.
        const int16_t pixelX = static_cast<int16_t>(transformX * transformSize);
        const int16_t pixelY = static_cast<int16_t>(transformY * transformSize);

        // Write command buffer.
        std::vector<int16_t> residualData(RCLayerMaxCount, static_cast<int16_t>(residual));
        cmdBufferAppend(cmdBuffer, pixelX, pixelY, residualData.data());

        const bool bIsSigned = fixedPointIsSigned(params.format);

        const int32_t dstValue = bIsSigned ? sourceHP : sourceValue;

        // bExpectedMax is a bit of a hack for the signed code-path, as synthesizing
        // the correct maximum value is not possible in the current code.
        const int32_t dstExpectedValue = bIsSigned ? (bExpectedMax ? 32767 : expectedHP) : expectedValue;

        // Write initial value in dst.
        fillSurfaceRegionWithValue(dst, static_cast<uint32_t>(pixelX), static_cast<uint32_t>(pixelY),
                                   transformSize, transformSize, dstValue);

        // Write expected value in dstExpected.
        fillSurfaceRegionWithValue(dstExpected, static_cast<uint32_t>(pixelX),
                                   static_cast<uint32_t>(pixelY), transformSize, transformSize,
                                   dstExpectedValue);
    };

    // Add - dst = 20, residual = 20, result = 40
    writeTransformData(0, 0, 20, 20, 40);

    // Subtract - dst = 60, residual = -20, result = 40
    writeTransformData(1, 0, 60, -20, 40);

    // Calculate min and max for under/overflow calculations
    // For unsigned this is trivially, [0, 2 ** bitdepth].
    //
    // For signed this is the numbers used to calculate the
    // values are offset to excite the behaviour in a contrived way.
    int32_t dstMax = fixedPointMaxValue(fixedPointLowPrecision(params.format));
    int32_t dstMin = 0;

    if (fixedPointIsSigned(params.format)) {
        const auto fpHalf = (dstMax + 1) >> 1;
        dstMin = -fpHalf;
        dstMax += fpHalf;
    }

    // Underflow - dst = min + 5, residual = -10, result = 0
    writeTransformData(2, 0, dstMin + 5, -10, dstMin);

    // Overflow - dst = max - 5, residual = 10, result = max
    writeTransformData(3, 0, dstMax - 5, 10, dstMax, true);

    // Right edge partial transform
    writeTransformData(4, 0, 0, 10, 10);

    // Bottom edge partial transforms (with a bottom right edge too)
    // dst = 30, residual = -10, result = 20, progressively incrementing
    // the dst and result values across transforms. This ensures bottom
    // edge processing moves the residual data pointer for a whole transform.
    for (uint32_t i = 0; i < 5; ++i) {
        const auto transformOffset = i * 10;
        writeTransformData(i, 1, 30 + transformOffset, -10, 20 + transformOffset);
    }

    const auto cmdBufferData = cmdBufferGetData(cmdBuffer);

    ApplyCmdBufferArgs_t args{&dst, cmdBufferData.coordinates, cmdBufferData.residuals,
                              cmdBufferData.count};
    function(&args);

    ExpectEqSurfacesTiled(transformSize, dst, dstExpected);

    cmdBufferFree(cmdBuffer);
    surfaceRelease(ctx, &dst);
    surfaceRelease(ctx, &dstExpected);
}

// -----------------------------------------------------------------------------

struct ApplyCmdBufferTileClearCorrectnessTestParams
{
    CPUAccelerationFeatures_t accel;
    FixedPoint_t format;
};

class ApplyCmdBufferTileClearCorrectnessTest
    : public FixtureWithParam<ApplyCmdBufferTileClearCorrectnessTestParams>
{};

TEST_P(ApplyCmdBufferTileClearCorrectnessTest, Test)
{
    static constexpr uint32_t kTileSize = 32;
    static constexpr uint32_t kWidth = 120;
    static constexpr uint32_t kHeight = 120;
    static constexpr int16_t kFillValue = 35;

    const auto& params = GetParam();

    auto function =
        applyCmdBufferGetFunction(ACBM_Tiles, params.format, TransformDD, simdFlag(params.accel));

    Surface_t dst{};
    Surface_t dstExpected{};

    Context_t* ctx = context.get();
    surfaceIdle(ctx, &dst);
    surfaceIdle(ctx, &dstExpected);

    EXPECT_EQ(surfaceInitialise(ctx, &dst, params.format, kWidth, kHeight, kWidth, ILNone), 0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstExpected, params.format, kWidth, kHeight, kWidth, ILNone), 0);

    CmdBuffer_t* cmdBuffer;
    EXPECT_TRUE(cmdBufferInitialise(ctx->memory, &cmdBuffer, CBTCoordinates));

    fillSurfaceWithValue(dst, kFillValue);
    fillSurfaceWithValue(dstExpected, kFillValue);

    const auto writeTileClear = [&dstExpected, &cmdBuffer](uint32_t tileX, uint32_t tileY) {
        const uint32_t pixelX = tileX * kTileSize;
        const uint32_t pixelY = tileY * kTileSize;

        cmdBufferAppendCoord(cmdBuffer, static_cast<int16_t>(pixelX), static_cast<int16_t>(pixelY));
        fillSurfaceRegionWithValue(dstExpected, pixelX, pixelY, kTileSize, kTileSize, 0);
    };

    writeTileClear(1, 0); // Top line, no clipping.
    writeTileClear(3, 0); // Top line, right edge clipping.
    writeTileClear(1, 2); // 3rd line, no clipping.
    writeTileClear(2, 3); // Bottom line, bottom edge clipping.
    writeTileClear(3, 3); // Bottom line & right edge clipping

    const auto cmdBufferData = cmdBufferGetData(cmdBuffer);
    const ApplyCmdBufferArgs_t args{&dst, cmdBufferData.coordinates, nullptr, cmdBufferData.count};
    function(&args);

    // Check surface is exactly as expected.
    ExpectEqSurfacesTiled(kTileSize, dst, dstExpected);

    cmdBufferFree(cmdBuffer);
    surfaceRelease(ctx, &dst);
    surfaceRelease(ctx, &dstExpected);
}

// -----------------------------------------------------------------------------

struct ApplyCmdBufferHighlightCorrectnessTestParams
{
    CPUAccelerationFeatures_t accel;
    FixedPoint_t format;
    TransformType_t transform;
    uint16_t highlightValue;
};

class ApplyCmdBufferHighlightCorrectnessTest
    : public FixtureWithParam<ApplyCmdBufferHighlightCorrectnessTestParams>
{};

TEST_P(ApplyCmdBufferHighlightCorrectnessTest, Test)
{
    static constexpr uint32_t kFullTransformCount = 4;

    const auto& params = GetParam();

    auto function = applyCmdBufferGetFunction(ACBM_Highlight, params.format, params.transform,
                                              simdFlag(params.accel));
    auto simdFunction =
        applyCmdBufferGetFunction(ACBM_Highlight, params.format, params.transform, simdFlag());
    if ((params.accel != CAFNone) && (function == simdFunction)) {
        GTEST_SKIP() << "Skipping SIMD check as there is no SIMD implementation for "
                        "these parameters";
    }

    EXPECT_NE(function, nullptr);

    const auto transformSize = static_cast<uint32_t>(transformTypeDimensions(params.transform));
    const auto surfaceWidth = transformSize * kFullTransformCount + (transformSize >> 1);
    const auto surfaceHeight = transformSize + (transformSize >> 1);

    Surface_t dst{};
    Surface_t dstExpected{};

    Context_t* ctx = context.get();
    surfaceIdle(ctx, &dst);
    surfaceIdle(ctx, &dstExpected);

    EXPECT_EQ(surfaceInitialise(ctx, &dst, params.format, surfaceWidth, surfaceHeight, surfaceWidth, ILNone),
              0);
    EXPECT_EQ(surfaceInitialise(ctx, &dstExpected, params.format, surfaceWidth, surfaceHeight,
                                surfaceWidth, ILNone),
              0);
    surfaceZero(ctx, &dst);
    surfaceZero(ctx, &dstExpected);

    CmdBuffer_t* cmdBuffer;
    EXPECT_TRUE(cmdBufferInitialise(ctx->memory, &cmdBuffer, CBTResiduals));
    EXPECT_TRUE(cmdBufferReset(cmdBuffer, transformTypeLayerCount(params.transform)));

    Highlight_t highlight = {};
    highlightSetValue(&highlight, bitdepthFromFixedPoint(params.format), params.highlightValue);
    const int32_t highlightValue =
        fixedPointIsSigned(params.format) ? highlight.valSigned : highlight.valUnsigned;

    const auto writeHighlightData = [&transformSize, &cmdBuffer, &dstExpected,
                                     &highlightValue](uint32_t transformX, uint32_t transformY) {
        // Calculate pixel coords.
        const int16_t pixelX = static_cast<int16_t>(transformX * transformSize);
        const int16_t pixelY = static_cast<int16_t>(transformY * transformSize);

        // Write command buffer (residual data doesn't matter as we're highlighting).
        std::vector<int16_t> residualData(RCLayerMaxCount, static_cast<int16_t>(0));
        cmdBufferAppend(cmdBuffer, pixelX, pixelY, residualData.data());

        // Write expected value in dstExpected.
        fillSurfaceRegionWithValue(dstExpected, static_cast<uint32_t>(pixelX),
                                   static_cast<uint32_t>(pixelY), transformSize, transformSize,
                                   highlightValue);
    };

    writeHighlightData(1, 0); // Top row whole TU
    writeHighlightData(4, 0); // Top right, right edge partial TU
    writeHighlightData(2, 1); // Middle wholeTU
    writeHighlightData(1, 3); // Bottom edge partial TU
    writeHighlightData(4, 3); // Bottom right corner partial TU

    const auto cmdBufferData = cmdBufferGetData(cmdBuffer);

    const ApplyCmdBufferArgs_t args{&dst, cmdBufferData.coordinates, cmdBufferData.residuals,
                                    cmdBufferData.count, &highlight};
    function(&args);

    ExpectEqSurfacesTiled(transformSize, dst, dstExpected);

    cmdBufferFree(cmdBuffer);
    surfaceRelease(ctx, &dst);
    surfaceRelease(ctx, &dstExpected);
}

// -----------------------------------------------------------------------------

const char* applyCmdBufferModeToString(ApplyCmdBufferMode_t mode)
{
    switch (mode) {
        case ACBM_Inter: return "inter";
        case ACBM_Intra: return "intra";
        case ACBM_Tiles: return "tiles";
        case ACBM_Highlight: return "highlight";
    }

    return "unknown";
}

std::string cpuAccelerationFeaturesToString(CPUAccelerationFeatures_t features)
{
    features = simdFlag(features);
    if (features == CAFNone) {
        return "Scalar";
    }
    if (features & CAFAVX2) {
        return "AVX2";
    }
    if (features & CAFSSE) {
        return "SSE";
    }
    if (features & CAFNEON) {
        return "NEON";
    }
    return "Unknown";
}

std::string applyCmdBufferTileClearTestToString(
    const testing::TestParamInfo<ApplyCmdBufferTileClearCorrectnessTestParams>& value)
{
    std::stringstream ss;

    ss << cpuAccelerationFeaturesToString(value.param.accel) << "_"
       << fixedPointToString(value.param.format);

    return ss.str();
}

std::string applyCmdBufferSIMDTestToString(const testing::TestParamInfo<ApplyCmdBufferSIMDTestParams>& value)
{
    std::stringstream ss;

    const auto mode = value.param.mode;
    ss << applyCmdBufferModeToString(mode) << fixedPointToString(value.param.format);

    if (mode != ACBM_Tiles) {
        ss << "_" << transformTypeToString(value.param.transform);
    }

    return ss.str();
}

std::string applyCmdBufferIntraCorrectnessTestToString(
    const testing::TestParamInfo<ApplyCmdBufferIntraCorrectnessTestParams>& value)
{
    std::stringstream ss;

    ss << cpuAccelerationFeaturesToString(value.param.accel) << "_"
       << transformTypeToString(value.param.transform);

    return ss.str();
}

std::string applyCmdBufferInterCorrectnessTestToString(
    const testing::TestParamInfo<ApplyCmdBufferInterCorrectnessTestParams>& value)
{
    std::stringstream ss;

    ss << cpuAccelerationFeaturesToString(value.param.accel) << "_"
       << fixedPointToString(value.param.format) << "_" << transformTypeToString(value.param.transform);

    return ss.str();
}

std::string applyCmdBufferHighlightCorrectnessTestToString(
    const testing::TestParamInfo<ApplyCmdBufferHighlightCorrectnessTestParams>& value)
{
    std::stringstream ss;

    ss << cpuAccelerationFeaturesToString(value.param.accel) << "_"
       << fixedPointToString(value.param.format) << "_"
       << transformTypeToString(value.param.transform) << "_" << value.param.highlightValue;

    return ss.str();
}

// -----------------------------------------------------------------------------

const std::vector<ApplyCmdBufferMode_t> kModesResiduals{ACBM_Inter, ACBM_Intra, ACBM_Tiles};
const std::vector<FixedPoint_t> kFixedPointAll = {FPU8, FPU10, FPU12, FPU14,
                                                  FPS8, FPS10, FPS12, FPS14};
const std::vector<FixedPoint_t> kFixedPointTileClear = {FPU8, FPS8};
const std::vector<TransformType_t> kTransformAll = {TransformDD, TransformDDS};
const std::vector<CPUAccelerationFeatures_t> kAccelAll = {CAFNone, CAFSSE};
const std::vector<uint16_t> kHighlightValuesAll = {0, 1};

// clang-format off
const auto kApplyResidualsSIMDParams =  rv::cartesian_product(kModesResiduals, kFixedPointAll, kTransformAll) |
                                        rv::filter([](auto value) {
                                            const auto mode = std::get<0>(value);
                                            const auto fpSigned = fixedPointIsSigned(std::get<1>(value));
                                            const auto isDD = std::get<2>(value) == TransformDD;

                                            // Apply intra is only supported for signed destination surfaces.
                                            if (mode == ACBM_Intra)
                                            {
                                                return fpSigned;
                                            }

                                            // Apply tiles should filter out the transform to reduce repeating tests
                                            // and is only supoported for signed destination surfaces.
                                            if (mode == ACBM_Tiles)
                                            {
                                                return fpSigned && isDD;
                                            }

                                            return true;
                                        }) |
                                        rv::transform([](auto value) {
                                            return ApplyCmdBufferSIMDTestParams{std::get<0>(value), std::get<1>(value), std::get<2>(value)};
                                        }) |
                                        rg::to_vector;

const auto kApplyIntraCorrectnessParams =   rv::cartesian_product(kAccelAll, kTransformAll) |
                                            rv::transform([](auto value) {
                                                return ApplyCmdBufferIntraCorrectnessTestParams{ std::get<0>(value), std::get<1>(value) };
                                            }) |
                                            rg::to_vector;

const auto kApplyInterCorrectnessParams =   rv::cartesian_product(kAccelAll, kFixedPointAll, kTransformAll) |
                                            rv::transform([](auto value) {
                                                return ApplyCmdBufferInterCorrectnessTestParams{ std::get<0>(value), std::get<1>(value), std::get<2>(value)};
                                            }) |
                                            rg::to_vector;

const auto kApplyTileClearParams =          rv::cartesian_product(kAccelAll, kFixedPointTileClear) |
                                            rv::transform([](auto value) {
                                                return ApplyCmdBufferTileClearCorrectnessTestParams{ std::get<0>(value), std::get<1>(value) };
                                            }) |
                                            rg::to_vector;

const auto kApplyHighlightParams =          rv::cartesian_product(kAccelAll, kFixedPointAll, kTransformAll, kHighlightValuesAll) |
                                            rv::transform([](auto value) {
                                                return ApplyCmdBufferHighlightCorrectnessTestParams{ std::get<0>(value), std::get<1>(value), std::get<2>(value), std::get<3>(value) };
                                            }) |
                                            rg::to_vector;
// clang-format on

INSTANTIATE_TEST_SUITE_P(CmdBufferTests, ApplyCmdBufferSIMDTest,
                         testing::ValuesIn(kApplyResidualsSIMDParams), applyCmdBufferSIMDTestToString);

INSTANTIATE_TEST_SUITE_P(CmdBufferTests, ApplyCmdBufferIntraCorrectnessTest,
                         testing::ValuesIn(kApplyIntraCorrectnessParams),
                         applyCmdBufferIntraCorrectnessTestToString);

INSTANTIATE_TEST_SUITE_P(CmdBufferTests, ApplyCmdBufferInterCorrectnessTest,
                         testing::ValuesIn(kApplyInterCorrectnessParams),
                         applyCmdBufferInterCorrectnessTestToString);

INSTANTIATE_TEST_SUITE_P(CmdBufferTests, ApplyCmdBufferTileClearCorrectnessTest,
                         testing::ValuesIn(kApplyTileClearParams), applyCmdBufferTileClearTestToString);

INSTANTIATE_TEST_SUITE_P(CmdBufferTests, ApplyCmdBufferHighlightCorrectnessTest,
                         testing::ValuesIn(kApplyHighlightParams),
                         applyCmdBufferHighlightCorrectnessTestToString);

// -----------------------------------------------------------------------------
