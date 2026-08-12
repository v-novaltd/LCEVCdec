/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C"
{
#include "config_parser_types.h"

#include <LCEVC/enhancement/approximate_pa.h>
#include <LCEVC/enhancement/config_parser.h>
}

class ApproximatePA : public testing::Test
{
public:
    void SetUp() override
    {
        std::vector<int16_t> coeffs;
        std::string name = testing::UnitTest::GetInstance()->current_test_info()->name();
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);

        if (name == "Cubic") {
            globalConfig.upscale = USCubic;
        } else if (name == "ModifiedCubic") {
            globalConfig.upscale = USModifiedCubic;
        } else if (name == "Linear") {
            globalConfig.upscale = USLinear;
        } else if (name == "Nearest") {
            globalConfig.upscale = USNearest;
        } else {
            FAIL() << "Invalid upscale type";
        }
        globalConfig.kernel = kKernels[globalConfig.upscale];
        globalConfig.initialized = true;
        globalConfig.predictedAverageEnabled = true;
    }
    LdeGlobalConfig globalConfig = {};
};

TEST_F(ApproximatePA, Cubic)
{
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_TRUE(globalConfig.kernel.approximatedPA);
    EXPECT_EQ(globalConfig.kernel.length, 4);
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({2662, 16384, 2662, 0}));
}

TEST_F(ApproximatePA, ModifiedCubic)
{
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_TRUE(globalConfig.kernel.approximatedPA);
    EXPECT_EQ(globalConfig.kernel.length, 4);
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({3262, 16384, 3262, 0}));
}

TEST_F(ApproximatePA, Linear)
{
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_TRUE(globalConfig.kernel.approximatedPA);
    EXPECT_EQ(globalConfig.kernel.length, 4);
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({2048, 16384, 2048, 0}));

    // Re-approximate and the kernel should remain as-is
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_TRUE(globalConfig.kernel.approximatedPA);
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({2048, 16384, 2048, 0}));
}

TEST_F(ApproximatePA, Nearest)
{
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_FALSE(globalConfig.kernel.approximatedPA);
    EXPECT_EQ(globalConfig.kernel.length, 2);
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({0, 16384, 0, 0}));
}

TEST(ApproximatePAWide, PreservesUnsignedMagnitudes)
{
    LdeGlobalConfig globalConfig = {};
    ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &globalConfig);
    globalConfig.upscale = USAdaptiveCubic;
    globalConfig.kernel = {{49151, 65535, 65535, 65535}, 4, false};
    globalConfig.initialized = true;
    globalConfig.predictedAverageEnabled = true;

    EXPECT_EQ(ldeKernelGetCoefficient(&globalConfig.kernel, 0), -49151);
    EXPECT_EQ(ldeKernelGetCoefficient(&globalConfig.kernel, 1), 65535);
    EXPECT_TRUE(ldeApproximatePA(&globalConfig));
    EXPECT_THAT(globalConfig.kernel.coeffs, ::testing::ElementsAreArray({57343, 16384, 57343, 0}));
    EXPECT_EQ(ldeKernelGetCoefficient(&globalConfig.kernel, 0), -57343);
}
