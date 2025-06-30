/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include <gtest/gtest.h>
#include <range/v3/view.hpp>
#include <rng.h>

extern "C"
{
#include "dequant.h"
#include "log_utilities.h"
#include "transform.h"
}

extern "C"
{
// Helper function that exists inside "decode/transform.c" to perform the merged
// quant & transform functions.
void dequantScalar(const Dequant* dequant, TemporalSignal temporalSignal, int32_t numLayers,
                   const int16_t* coeffs, int16_t* dequantizedCoeffs);
}

static inline int32_t transformTypeLayerCount(LdeTransformType type)
{
    return (type == TransformDD) ? 4 : 16;
}

namespace rg = ranges;
namespace rv = ranges::views;

// -----------------------------------------------------------------------------

enum class CoefficientValuesType
{
    Ones,
    OnesRandomSign,
    Incrementing,
    Overflow,
    Underflow,
    Random
};

static const char* coefficientValuesTypeToString(CoefficientValuesType type)
{
    switch (type) {
        case CoefficientValuesType::Ones: return "CoeffsOnes";
        case CoefficientValuesType::OnesRandomSign: return "CoeffsOneRandomSign";
        case CoefficientValuesType::Incrementing: return "CoeffsIncrementing";
        case CoefficientValuesType::Overflow: return "CoeffsOverflow";
        case CoefficientValuesType::Underflow: return "CoeffsUnderflow";
        case CoefficientValuesType::Random: return "CoeffsRandom";
    }
    return "error_unknown_coefficient_values_type";
}

static std::vector<int16_t> getCoefficientValues(CoefficientValuesType coeffsType, LdeTransformType transformType)
{
    const auto layerCount = transformTypeLayerCount(transformType);

    if (coeffsType == CoefficientValuesType::Ones) {
        return std::vector<int16_t>(layerCount, 1);
    }

    if (coeffsType == CoefficientValuesType::OnesRandomSign) {
        auto rng = lcevc_dec::utility::RNG(1);

        // clang-format off
        return rv::generate([&rng]() { return static_cast<int16_t>(1 * ((rng() == 0) ? -1 : 1)); }) |
               rv::take(layerCount) |
               rg::to_vector;
        // clang-format on
    }

    if (coeffsType == CoefficientValuesType::Incrementing) {
        // clang-format off
        return rv::iota(1) |
               rv::take(layerCount) |
               rv::transform([](auto value) { return static_cast<int16_t>(value); }) |
               rg::to_vector;
        // clang-format on
    }

    if (coeffsType == CoefficientValuesType::Overflow) {
        return std::vector<int16_t>(layerCount, std::numeric_limits<int16_t>::max() - 1);
    }

    if (coeffsType == CoefficientValuesType::Underflow) {
        return std::vector<int16_t>(layerCount, std::numeric_limits<int16_t>::min() + 1);
    }

    if (coeffsType == CoefficientValuesType::Random) {
        // do something to generate random
        auto rng = lcevc_dec::utility::RNG(std::numeric_limits<uint16_t>::max() - 1);
        static const auto offset = 32768;

        // clang-format off
        return rv::generate([&rng]()
                    {
                        return static_cast<int16_t>(static_cast<int32_t>(rng()) - offset);
                    }) |
               rv::take(layerCount) |
               rg::to_vector;
        // clang-format on
    }

    return {};
}

enum class DequantValuesType
{
    Basic,
    Overflow,
    Underflow
};

const char* dequantValuesTypeToString(DequantValuesType type)
{
    switch (type) {
        case DequantValuesType::Basic: return "DequantBasic";
        case DequantValuesType::Overflow: return "DequantOverflow";
        case DequantValuesType::Underflow: return "DequantUnderflow";
    }

    return "error_unknown_dequant_values_type";
}

// Populates Dequant_t with values that will result in producing dequantized
// coefficients that will exercise the behavior named in `DequantValuesType`.
//
// Noting that it is exected that these values are applied to coefficient values
// all containing 1.
//
// This does not populate Dequant_t with values that perform the behavior named
// in `DequantValuesType` during dequantization - dequantization is expected
// to always be within stable numeric ranges.
static Dequant getDequantValues(DequantValuesType type, LdeTransformType transformType)
{
    const auto layerCount = transformTypeLayerCount(transformType);
    Dequant dequant = {};

    int16_t stepWidth = 0;
    int16_t offset = 0;

    if (type == DequantValuesType::Basic) {
        stepWidth = 100;
        offset = 50;
    }

    if (type == DequantValuesType::Overflow) {
        const auto maxValue = std::numeric_limits<int16_t>::max() - (2 * layerCount * TSCount) - 2;
        stepWidth = static_cast<int16_t>(maxValue);
        offset = static_cast<int16_t>(layerCount * TSCount);
    }

    if (type == DequantValuesType::Underflow) {
        const auto minValue = std::numeric_limits<int16_t>::min() + (2 * layerCount * TSCount) + 2;
        stepWidth = static_cast<int16_t>(minValue);
        offset = 1;
    }

    // Fill out sw/offsets.
    for (auto temporal = 0; temporal < TSCount; ++temporal) {
        for (auto i = 0; i < layerCount; ++i) {
            dequant.stepWidth[temporal][i] = stepWidth++;
            dequant.offset[temporal][i] = offset--;
        }
    }

    // Load up SIMD registers
#if VN_CORE_FEATURE(SSE)
    for (auto temporal = 0; temporal < TSCount; ++temporal) {
        dequant.stepWidthVector[temporal][0] =
            _mm_load_si128((const __m128i*)&dequant.stepWidth[temporal][0]);
        dequant.stepWidthVector[temporal][1] =
            _mm_load_si128((const __m128i*)&dequant.stepWidth[temporal][8]);
        dequant.offsetVector[temporal][0] = _mm_load_si128((const __m128i*)&dequant.offset[temporal][0]);
        dequant.offsetVector[temporal][1] = _mm_load_si128((const __m128i*)&dequant.offset[temporal][8]);
    }
#elif VN_CORE_FEATURE(NEON)
    for (auto temporal = 0; temporal < TSCount; ++temporal) {
        dequant.stepWidthVector[temporal][0] = vld1q_s16(&dequant.stepWidth[temporal][0]);
        dequant.stepWidthVector[temporal][1] = vld1q_s16(&dequant.stepWidth[temporal][8]);
        dequant.offsetVector[temporal][0] = vld1q_s16(&dequant.offset[temporal][0]);
        dequant.offsetVector[temporal][1] = vld1q_s16(&dequant.offset[temporal][8]);
    }
#endif

    return dequant;
}

static const char* temporalSignalToString(TemporalSignal signal)
{
    switch (signal) {
        case TSInter: return "inter";
        case TSIntra: return "intra";
        case TSCount: break;
    }

    return "error_unknown_temporal_signal_value";
}

// -----------------------------------------------------------------------------

struct TransformTestParams
{
    CoefficientValuesType coeffsValues;
    LdeTransformType transform;
    LdeScalingMode scaling;
};

class TransformTest : public testing::TestWithParam<TransformTestParams>
{};

// This test checks that the SIMD functions matche the equivalent
// scalar functions across a couple numeric conditions.
TEST_P(TransformTest, CompareSIMD)
{
    const auto& params = GetParam();

    auto scalarFunction = transformGetFunction(params.transform, params.scaling, true);
    auto simdFunction = transformGetFunction(params.transform, params.scaling, false);

    EXPECT_NE(scalarFunction, nullptr);
    EXPECT_NE(simdFunction, nullptr);

    if (scalarFunction == simdFunction) {
        GTEST_SKIP() << "Skipping SIMD comparison as there is no SIMD for these parameters";
    }

    const auto layerCount = transformTypeLayerCount(params.transform);
    const auto coefficients = getCoefficientValues(params.coeffsValues, params.transform);

    if (coefficients.size() != static_cast<uint32_t>(layerCount)) {
        GTEST_FAIL()
            << "Test error - coefficient values does not have the correct number of elements";
    }

    std::vector<int16_t> scalarResiduals(layerCount);
    std::vector<int16_t> simdResiduals(layerCount);

    scalarFunction(coefficients.data(), scalarResiduals.data());
    simdFunction(coefficients.data(), simdResiduals.data());

    const size_t compareSize = static_cast<size_t>(layerCount) * sizeof(int16_t);

    EXPECT_EQ(memcmp(scalarResiduals.data(), simdResiduals.data(), compareSize), 0);

    std::cout << "Scalar: ";
    for (auto i = 0; i < layerCount; ++i) {
        std::cout << scalarResiduals[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "SIMD:   ";
    for (auto i = 0; i < layerCount; ++i) {
        std::cout << simdResiduals[i] << ", ";
    }
    std::cout << std::endl;
}

// -----------------------------------------------------------------------------

struct DequantTransformTestParams
{
    DequantValuesType dequantType;
    CoefficientValuesType coeffsType;
    LdeTransformType transform;
    LdeScalingMode scaling;
    TemporalSignal temporalSignal;
};

class DequantTransformTest : public testing::TestWithParam<DequantTransformTestParams>
{};

TEST_P(DequantTransformTest, CompareSIMD)
{
    const auto& params = GetParam();

    auto scalarFunction = dequantTransformGetFunction(params.transform, params.scaling, true);
    auto simdFunction = dequantTransformGetFunction(params.transform, params.scaling, false);

    EXPECT_NE(scalarFunction, nullptr);
    EXPECT_NE(simdFunction, nullptr);

    if (scalarFunction == simdFunction) {
        GTEST_SKIP() << "Skipping SIMD comparison as there is no SIMD for these parameters";
    }

    const auto layerCount = transformTypeLayerCount(params.transform);
    const auto coefficients = getCoefficientValues(params.coeffsType, params.transform);

    if (coefficients.size() != static_cast<uint32_t>(layerCount)) {
        GTEST_FAIL()
            << "Test error - coefficient values does not have the correct number of elements";
    }

    const Dequant dequant = getDequantValues(params.dequantType, params.transform);

    std::vector<int16_t> scalarResiduals(layerCount);
    std::vector<int16_t> simdResiduals(layerCount);

    scalarFunction(&dequant, params.temporalSignal, coefficients.data(), scalarResiduals.data());
    simdFunction(&dequant, params.temporalSignal, coefficients.data(), simdResiduals.data());

    const size_t compareSize = static_cast<size_t>(layerCount) * sizeof(int16_t);

    EXPECT_EQ(memcmp(scalarResiduals.data(), simdResiduals.data(), compareSize), 0);
}

TEST_P(DequantTransformTest, CheckMergedFunctionMatchesSeparateFunctions)
{
    const auto& params = GetParam();

    const auto layerCount = transformTypeLayerCount(params.transform);
    const auto coefficients = getCoefficientValues(params.coeffsType, params.transform);

    if (coefficients.size() != static_cast<uint32_t>(layerCount)) {
        GTEST_FAIL()
            << "Test error - coefficient values does not have the correct number of elements";
    }

    const auto combinedFunction = dequantTransformGetFunction(params.transform, params.scaling, false);
    const auto transformFunction = transformGetFunction(params.transform, params.scaling, false);

    EXPECT_NE(combinedFunction, nullptr);
    EXPECT_NE(transformFunction, nullptr);

    const Dequant dequant = getDequantValues(params.dequantType, params.transform);

    // Perform separate dequant + transform.
    std::vector<int16_t> dequantizedCoefficients(layerCount);
    dequantScalar(&dequant, params.temporalSignal, layerCount, coefficients.data(),
                  dequantizedCoefficients.data());

    std::vector<int16_t> seperateResiduals(layerCount);
    transformFunction(dequantizedCoefficients.data(), seperateResiduals.data());

    // Perform merged dequant + transform (scalar).
    std::vector<int16_t> combinedResiduals(layerCount);
    combinedFunction(&dequant, params.temporalSignal, coefficients.data(), combinedResiduals.data());

    // Check they match
    const size_t compareSize = static_cast<size_t>(layerCount) * sizeof(int16_t);
    EXPECT_EQ(memcmp(seperateResiduals.data(), combinedResiduals.data(), compareSize), 0);
}

// -----------------------------------------------------------------------------

std::string transformTestToString(const testing::TestParamInfo<TransformTestParams>& value)
{
    std::stringstream ss;
    ss << coefficientValuesTypeToString(value.param.coeffsValues) << "_"
       << transformTypeToString(value.param.transform) << "_"
       << scalingModeToString(value.param.scaling);
    return ss.str();
}

std::string dequantTransformTestToString(const testing::TestParamInfo<DequantTransformTestParams>& value)
{
    std::stringstream ss;
    ss << dequantValuesTypeToString(value.param.dequantType) << "_"
       << coefficientValuesTypeToString(value.param.coeffsType) << "_"
       << transformTypeToString(value.param.transform) << "_" << scalingModeToString(value.param.scaling)
       << "_" << temporalSignalToString(value.param.temporalSignal);
    return ss.str();
}

// -----------------------------------------------------------------------------

const std::vector<CoefficientValuesType> kCoeffValuesAll = {
    CoefficientValuesType::Ones,         CoefficientValuesType::OnesRandomSign,
    CoefficientValuesType::Incrementing, CoefficientValuesType::Overflow,
    CoefficientValuesType::Underflow,    CoefficientValuesType::Random};
const std::vector<LdeTransformType> kTransformAll = {TransformDD, TransformDDS};
const std::vector<LdeScalingMode> kScaling1D = {Scale1D, Scale2D};
const std::vector<TemporalSignal> kTemporalSignalAll = {TSInter, TSIntra};
const std::vector<CoefficientValuesType> kDequantCoeffsLimited = {
    CoefficientValuesType::Ones, CoefficientValuesType::OnesRandomSign};
const std::vector<DequantValuesType> kDequantValuesAll = {
    DequantValuesType::Basic, DequantValuesType::Overflow, DequantValuesType::Underflow};

// clang-format off
const auto kTransformTestParams =   rv::cartesian_product(kCoeffValuesAll, kTransformAll, kScaling1D) |
                                    rv::transform([](auto value) {
                                        return TransformTestParams{std::get<0>(value), std::get<1>(value), std::get<2>(value)};
                                    }) |
                                    rg::to_vector;

// Dequant tests don't stress
const auto kDequantTransformTestParams =    rv::cartesian_product(kDequantValuesAll, kDequantCoeffsLimited, kTransformAll, kScaling1D, kTemporalSignalAll) |
                                            rv::transform([](auto value) {
                                                return DequantTransformTestParams{std::get<0>(value), std::get<1>(value), std::get<2>(value), std::get<3>(value), std::get<4>(value)};
                                            }) |
                                            rg::to_vector;
// clang-format on

INSTANTIATE_TEST_SUITE_P(TransformTests, TransformTest, testing::ValuesIn(kTransformTestParams),
                         transformTestToString);

INSTANTIATE_TEST_SUITE_P(TransformTests, DequantTransformTest,
                         testing::ValuesIn(kDequantTransformTestParams), dequantTransformTestToString);

// -----------------------------------------------------------------------------
