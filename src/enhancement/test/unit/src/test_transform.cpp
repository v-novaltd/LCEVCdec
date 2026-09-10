/* Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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
#include <LCEVC/common/acceleration.h>
#include <range/v3/view.hpp>
#include <rng.h>

extern "C"
{
#include "log_utilities.h"
#include "transform.h"
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

// -----------------------------------------------------------------------------

struct TransformTestParams
{
    CoefficientValuesType coeffsValues;
    LdeTransformType transform;
    LdeScalingMode scaling;
};

class TransformTest : public testing::TestWithParam<TransformTestParams>
{};

// This test checks that the SIMD functions match the equivalent
// scalar functions across a couple numeric conditions.
TEST_P(TransformTest, CompareSIMD)
{
    const auto& params = GetParam();

    ldcAccelerationInitialize(false);
    auto scalarFunction = transformGetFunction(params.transform, params.scaling);

    ldcAccelerationInitialize(true);
    auto simdFunction = transformGetFunction(params.transform, params.scaling);

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

std::string transformTestToString(const testing::TestParamInfo<TransformTestParams>& value)
{
    std::stringstream ss;
    ss << coefficientValuesTypeToString(value.param.coeffsValues) << "_"
       << transformTypeToString(value.param.transform) << "_"
       << scalingModeToString(value.param.scaling);
    return ss.str();
}

// -----------------------------------------------------------------------------

const std::vector<CoefficientValuesType> kCoeffValuesAll = {
    CoefficientValuesType::Ones,         CoefficientValuesType::OnesRandomSign,
    CoefficientValuesType::Incrementing, CoefficientValuesType::Overflow,
    CoefficientValuesType::Underflow,    CoefficientValuesType::Random};
const std::vector<LdeTransformType> kTransformAll = {TransformDD, TransformDDS};
const std::vector<LdeScalingMode> kScaling1D = {Scale1D, Scale2D};

// clang-format off
const auto kTransformTestParams =   rv::cartesian_product(kCoeffValuesAll, kTransformAll, kScaling1D) |
                                    rv::transform([](auto value) {
                                        return TransformTestParams{std::get<0>(value), std::get<1>(value), std::get<2>(value)};
                                    }) |
                                    rg::to_vector;
// clang-format on

INSTANTIATE_TEST_SUITE_P(TransformTests, TransformTest, testing::ValuesIn(kTransformTestParams),
                         transformTestToString);

// -----------------------------------------------------------------------------
