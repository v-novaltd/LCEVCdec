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

#include "test_transform.h"

#include <gtest/gtest.h>
#include <LCEVC/common/acceleration.h>
#include <range/v3/view.hpp>

namespace rg = ranges;
namespace rv = ranges::views;

// -----------------------------------------------------------------------------

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

    if (coefficients.size() != layerCount) {
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
    for (uint32_t i = 0; i < layerCount; ++i) {
        std::cout << scalarResiduals[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "SIMD:   ";
    for (uint32_t i = 0; i < layerCount; ++i) {
        std::cout << simdResiduals[i] << ", ";
    }
    std::cout << std::endl;
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
