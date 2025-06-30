/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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
#include <LCEVC/enhancement/transform_unit.h>

typedef struct transformUnitTestInput
{
    uint8_t tuSizeShift;
    uint32_t width;
    uint32_t height;
} transformUnitTestInput;

class TransformUnit : public testing::TestWithParam<transformUnitTestInput>
{};

TEST_P(TransformUnit, TUIndexBlockAlignment)
{
    transformUnitTestInput params = GetParam();

    TUState state = {};
    EXPECT_TRUE(ldeTuStateInitialize(&state, params.width, params.height, 0, 0, params.tuSizeShift));
    uint32_t x = 0;
    uint32_t y = 0;
    for (uint32_t tuIndex = 0; tuIndex < state.tuTotal; tuIndex++) {
        EXPECT_EQ(ldeTuCoordsBlockRaster(&state, tuIndex, &x, &y), 0);
        uint32_t blockAlignedCoords = ldeTuCoordsBlockAlignedIndex(&state, x, y);
        uint32_t blockAlignedIndex = ldeTuIndexBlockAlignedIndex(&state, tuIndex);
        EXPECT_EQ(blockAlignedCoords, blockAlignedIndex);
    }
}

INSTANTIATE_TEST_SUITE_P(
    transformUnitTestValues, TransformUnit,
    testing::Values(transformUnitTestInput{1, 180, 100}, transformUnitTestInput{2, 180, 100},
                    transformUnitTestInput{1, 292, 192}, transformUnitTestInput{2, 292, 192},
                    transformUnitTestInput{1, 96, 64}, transformUnitTestInput{2, 96, 64}));
