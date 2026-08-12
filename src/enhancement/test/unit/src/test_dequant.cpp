/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

extern "C"
{
#include "dequant.h"
}

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

TEST(Dequant, DeadzoneSupportsFullStepWidthRange)
{
    struct TestCase
    {
        int32_t stepWidth;
        int16_t expectedOffset;
    };

    constexpr std::array<TestCase, 5> kTestCases = {{{16, -8},
                                                     {17, 0},
                                                     {12249, std::numeric_limits<int16_t>::max()},
                                                     {12250, std::numeric_limits<int16_t>::max()},
                                                     {QMaxStepWidth, std::numeric_limits<int16_t>::max()}}};

    LdeGlobalConfig globalConfig = {};
    globalConfig.numLayers = RCLayerCountDD;

    LdeFrameConfig frameConfig = {};
    frameConfig.dequantOffset = 0;

    for (const LdeDequantOffsetMode mode : {DQMDefault, DQMConstOffset}) {
        frameConfig.dequantOffsetMode = mode;

        for (const TestCase& testCase : kTestCases) {
            frameConfig.stepWidths[LOQ1] = testCase.stepWidth;

            Dequant dequant = {};
            ASSERT_TRUE(calculateDequant(&dequant, &globalConfig, &frameConfig, 0, LOQ1));

            for (uint8_t temporal = 0; temporal < TSCount; ++temporal) {
                for (uint8_t layer = 0; layer < globalConfig.numLayers; ++layer) {
                    EXPECT_EQ(dequant.stepWidth[temporal][layer], testCase.stepWidth);
                    EXPECT_EQ(dequant.offset[temporal][layer], testCase.expectedOffset);
                }
            }
        }
    }
}
