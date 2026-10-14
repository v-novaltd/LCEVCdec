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

#ifndef VN_LCEVC_ENHANCEMENT_TEST_TRANSFORM_H
#define VN_LCEVC_ENHANCEMENT_TEST_TRANSFORM_H

#include <gtest/gtest.h>
#include <rng.h>

extern "C"
{
#include "log_utilities.h"
#include "transform.h"
}

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

inline uint32_t transformTypeLayerCount(LdeTransformType type)
{
    return (type == TransformDD) ? RCLayerCountDD : RCLayerCountDDS;
}

enum class CoefficientValuesType
{
    Zeros,
    Ones,
    OnesRandomSign,
    Incrementing,
    Overflow,
    Underflow,
    Random
};

inline const char* coefficientValuesTypeToString(CoefficientValuesType type)
{
    switch (type) {
        case CoefficientValuesType::Zeros: return "CoeffsZeros";
        case CoefficientValuesType::Ones: return "CoeffsOnes";
        case CoefficientValuesType::OnesRandomSign: return "CoeffsOneRandomSign";
        case CoefficientValuesType::Incrementing: return "CoeffsIncrementing";
        case CoefficientValuesType::Overflow: return "CoeffsOverflow";
        case CoefficientValuesType::Underflow: return "CoeffsUnderflow";
        case CoefficientValuesType::Random: return "CoeffsRandom";
    }
    return "error_unknown_coefficient_values_type";
}

inline std::vector<int16_t> getCoefficientValues(CoefficientValuesType coeffsType, LdeTransformType transformType)
{
    const auto layerCount = transformTypeLayerCount(transformType);
    std::vector<int16_t> coefficients(layerCount);

    switch (coeffsType) {
        case CoefficientValuesType::Zeros: break;
        case CoefficientValuesType::Ones:
            std::fill(coefficients.begin(), coefficients.end(), 1);
            break;
        case CoefficientValuesType::OnesRandomSign: {
            auto rng = lcevc_dec::utility::RNG(1);
            for (auto& coefficient : coefficients) {
                coefficient = static_cast<int16_t>((rng() == 0) ? -1 : 1);
            }
            break;
        }
        case CoefficientValuesType::Incrementing:
            for (uint32_t layer = 0; layer < layerCount; ++layer) {
                coefficients[layer] = static_cast<int16_t>(layer + 1);
            }
            break;
        case CoefficientValuesType::Overflow:
            std::fill(coefficients.begin(), coefficients.end(), std::numeric_limits<int16_t>::max() - 1);
            break;
        case CoefficientValuesType::Underflow:
            std::fill(coefficients.begin(), coefficients.end(), std::numeric_limits<int16_t>::min() + 1);
            break;
        case CoefficientValuesType::Random: {
            auto rng = lcevc_dec::utility::RNG(std::numeric_limits<uint16_t>::max() - 1);
            constexpr int32_t kSignedOffset = 32768;
            for (auto& coefficient : coefficients) {
                coefficient = static_cast<int16_t>(static_cast<int32_t>(rng()) - kSignedOffset);
            }
            break;
        }
    }

    return coefficients;
}

struct TransformTestParams
{
    CoefficientValuesType coeffsValues;
    LdeTransformType transform;
    LdeScalingMode scaling;
};

inline std::string transformTestToString(const testing::TestParamInfo<TransformTestParams>& value)
{
    std::stringstream ss;
    ss << coefficientValuesTypeToString(value.param.coeffsValues) << "_"
       << transformTypeToString(value.param.transform) << "_"
       << scalingModeToString(value.param.scaling);
    return ss.str();
}

#endif // VN_LCEVC_ENHANCEMENT_TEST_TRANSFORM_H
