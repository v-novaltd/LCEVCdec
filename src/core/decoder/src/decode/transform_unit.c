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

#include "decode/transform_unit.h"

#include "common/types.h"

#include <assert.h>

int32_t tuStateInitialise(TUState_t* const state, uint32_t width, uint32_t height, uint32_t xOffset,
                          uint32_t yOffset, uint8_t tuWidthShift)
{
    assert(state);
    assert(tuWidthShift);

    const uint32_t tuSize = 1 << tuWidthShift;

    /* Require width and height to be divisible by TU's width and height respectively. */
    if (width & (tuSize - 1)) {
        return -1;
    }

    if (height & (tuSize - 1)) {
        return -1;
    }

    state->tuWidthShift = tuWidthShift;
    state->numAcross = width >> tuWidthShift;
    state->tuTotal = state->numAcross * (height >> tuWidthShift);
    state->xOffset = xOffset;
    state->yOffset = yOffset;

    state->block.tuPerBlockDimsShift = ((tuWidthShift == 1) ? 4 : 3);
    state->block.tuPerBlockDims = (uint8_t)(1 << state->block.tuPerBlockDimsShift);
    state->block.tuPerBlockShift = (uint8_t)(state->block.tuPerBlockDimsShift << 1);
    state->block.tuPerBlock = (uint16_t)(1 << state->block.tuPerBlockShift);
    state->block.tuPerBlockRowRightEdge = ((width & (BSTemporal - 1)) >> tuWidthShift);
    state->block.tuPerBlockColBottomEdge = ((height & (BSTemporal - 1)) >> tuWidthShift);
    state->block.tuPerBlockBottomEdge = state->block.tuPerBlockColBottomEdge
                                        << state->block.tuPerBlockDimsShift;
    state->block.tuPerRow = state->numAcross << state->block.tuPerBlockDimsShift;
    state->block.wholeBlocksPerRow = width >> BSTemporalShift;
    state->block.wholeBlocksPerCol = height >> BSTemporalShift;
    state->block.blocksPerRow = (width + BSTemporal - 1) >> BSTemporalShift;
    state->block.blocksPerCol = (height + BSTemporal - 1) >> BSTemporalShift;

    const uint32_t blockAlignedWidth = ((width + (BSTemporal - 1)) & (-BSTemporal));
    state->blockAligned.tuPerRow = (blockAlignedWidth >> tuWidthShift) << state->block.tuPerBlockDimsShift;
    state->blockAligned.maxWholeBlockY = (state->block.wholeBlocksPerCol << BSTemporalShift);

    return 0;
}

int32_t tuCoordsSurfaceRaster(const TUState_t* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
{
    assert(state);
    assert(x);
    assert(y);

    if (tuIndex > state->tuTotal) {
        return -1;
    }

    if (tuIndex == state->tuTotal) {
        return 1;
    }

    *x = ((tuIndex % state->numAcross) << state->tuWidthShift) + state->xOffset;
    *y = ((tuIndex / state->numAcross) << state->tuWidthShift) + state->yOffset;

    return 0;
}

int32_t tuCoordsBlockRaster(const TUState_t* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
{
    assert(state);
    assert(x);
    assert(y);

    if (tuIndex > state->tuTotal) {
        return -1;
    }

    if (tuIndex == state->tuTotal) {
        return 1;
    }

    /* Determine row of blocks that this TU falls into */
    const uint32_t blockRowIndex =
        tuIndex / state->block.tuPerRow; /* Index of the block row this TU falls into. */
    const uint32_t rowTUIndex =
        tuIndex - (blockRowIndex * state->block.tuPerRow); /* TU index of the within the block row. */

    /* Determine column of blocks that this TU falls into */
    uint32_t blockColIndex = 0;
    uint32_t blockTUIndex = 0;

    if (blockRowIndex >= state->block.wholeBlocksPerCol) {
        /* Handle bottom edge case where each block will contain less TUs */
        assert(blockRowIndex == state->block.wholeBlocksPerCol);
        blockColIndex = rowTUIndex / state->block.tuPerBlockBottomEdge;
        blockTUIndex = rowTUIndex % state->block.tuPerBlockBottomEdge;
    } else {
        blockColIndex = rowTUIndex >> state->block.tuPerBlockShift;
        blockTUIndex = rowTUIndex - (blockColIndex << state->block.tuPerBlockShift);
    }

    /* Determine coord of TU inside block */
    uint32_t tuXCoord = 0;
    uint32_t tuYCoord = 0;

    if (blockColIndex >= state->block.wholeBlocksPerRow) {
        assert(blockColIndex == state->block.wholeBlocksPerRow);
        tuYCoord = blockTUIndex / state->block.tuPerBlockRowRightEdge;
        tuXCoord = blockTUIndex % state->block.tuPerBlockRowRightEdge;
    } else {
        tuYCoord = blockTUIndex >> state->block.tuPerBlockDimsShift;
        tuXCoord = blockTUIndex - (tuYCoord << state->block.tuPerBlockDimsShift);
    }

    /* Offset TU coord to full surface */
    tuXCoord += blockColIndex << state->block.tuPerBlockDimsShift;
    tuYCoord += blockRowIndex << state->block.tuPerBlockDimsShift;

    /* Convert TU coord to pixel position. */
    *x = (tuXCoord << state->tuWidthShift) + state->xOffset;
    *y = (tuYCoord << state->tuWidthShift) + state->yOffset;

    return 0;
}

uint32_t tuCoordsBlockAlignedIndex(const TUState_t* state, uint32_t x, uint32_t y)
{
    assert(state);
    assert(x >= state->xOffset);
    assert(y >= state->yOffset);

    /* Determine row and column of blocks that this TU falls into. */
    x -= state->xOffset;
    y -= state->yOffset;
    const uint32_t blockIndexX = x >> BSTemporalShift;
    const uint32_t blockIndexY = y >> BSTemporalShift;

    /* Get the tuIndex for the top-left corner of this block. Note that tuPerRow is for a row of
     * blocks, not a row of pixels. */
    uint32_t res =
        (blockIndexY * state->blockAligned.tuPerRow) + (blockIndexX << state->block.tuPerBlockShift);

    /* Get the offset within the block. */
    res += (((y - (blockIndexY * BSTemporal)) >> state->tuWidthShift) << state->block.tuPerBlockDimsShift);
    res += ((x - (blockIndexX * BSTemporal)) >> state->tuWidthShift);

    return res;
}

uint32_t tuCoordsSurfaceIndex(const TUState_t* state, uint32_t x, uint32_t y)
{
    uint32_t res = (y >> state->tuWidthShift) * state->numAcross;
    res += x >> state->tuWidthShift;

    return res;
}

int32_t tuCoordsBlockAlignedRaster(const TUState_t* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
{
    assert(state && x && y);

    const uint32_t blockRowIndex = tuIndex / state->blockAligned.tuPerRow;
    const uint32_t rowTUIndex = tuIndex - (blockRowIndex * state->blockAligned.tuPerRow);

    uint32_t blockColIndex = rowTUIndex >> state->block.tuPerBlockShift;
    uint32_t blockTUIndex = rowTUIndex - (blockColIndex << state->block.tuPerBlockShift);
    uint32_t tuYCoord = blockTUIndex >> state->block.tuPerBlockDimsShift;
    uint32_t tuXCoord = blockTUIndex - (tuYCoord << state->block.tuPerBlockDimsShift);

    tuXCoord += blockColIndex << state->block.tuPerBlockDimsShift;
    tuYCoord += blockRowIndex << state->block.tuPerBlockDimsShift;

    *x = (tuXCoord << state->tuWidthShift) + state->xOffset;
    *y = (tuYCoord << state->tuWidthShift) + state->yOffset;

    return 0;
}

int32_t tuCoordsBlockIndex(const TUState_t* state, uint32_t x, uint32_t y, uint32_t* blockIndex)
{
    assert(state);
    assert(blockIndex);
    assert(x >= state->xOffset);
    assert(y >= state->yOffset);

    const uint32_t blockCount = state->block.blocksPerRow * state->block.blocksPerCol;
    const uint32_t blockIndexX = (x - state->xOffset) / BSTemporal;
    const uint32_t blockIndexY = (y - state->yOffset) / BSTemporal;
    const uint32_t res = (blockIndexY * state->block.blocksPerRow) + blockIndexX;

    if (res > blockCount) {
        return -1;
    }

    *blockIndex = res;

    return 0;
}

void tuCoordsBlockDetails(const TUState_t* state, uint32_t x, uint32_t y, uint32_t* blockWidth,
                          uint32_t* blockHeight, uint32_t* tuCount)
{
    assert(state);
    assert(blockWidth);
    assert(blockHeight);
    assert(tuCount);

    const uint32_t rightLimit = state->block.wholeBlocksPerRow * BSTemporal;
    const uint32_t bottomLimit = state->block.wholeBlocksPerCol * BSTemporal;
    const uint32_t xPos = x - state->xOffset;
    const uint32_t yPos = y - state->yOffset;

    const uint32_t tuWide =
        (xPos >= rightLimit ? state->block.tuPerBlockRowRightEdge : state->block.tuPerBlockDims);
    const uint32_t tuHigh =
        (yPos >= bottomLimit ? state->block.tuPerBlockColBottomEdge : state->block.tuPerBlockDims);

    *tuCount = tuWide * tuHigh;
    *blockWidth = tuWide << state->tuWidthShift;
    *blockHeight = tuHigh << state->tuWidthShift;
}

void tuBlockTuCount(const TUState_t* state, uint32_t x, uint32_t y, uint32_t* tuCount)
{
    assert(state && tuCount);

    const uint32_t rightLimit = state->block.wholeBlocksPerRow * BSTemporal;
    const uint32_t bottomLimit = state->block.wholeBlocksPerCol * BSTemporal;
    const uint32_t xPos = x - state->xOffset;
    const uint32_t yPos = y - state->yOffset;

    const uint32_t tuWide =
        (xPos >= rightLimit ? state->block.tuPerBlockRowRightEdge : state->block.tuPerBlockDims);
    const uint32_t tuHigh =
        (yPos >= bottomLimit ? state->block.tuPerBlockColBottomEdge : state->block.tuPerBlockDims);

    *tuCount = tuWide * tuHigh;
}

/*------------------------------------------------------------------------------*/
