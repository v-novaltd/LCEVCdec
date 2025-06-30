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

#include <assert.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/transform_unit.h>

bool ldeTuStateInitialize(TUState* const state, uint32_t width, uint32_t height, uint32_t xOffset,
                          uint32_t yOffset, uint8_t tuWidthShift)
{
    assert(state);
    assert(tuWidthShift);

    const uint32_t tuSize = 1 << tuWidthShift;

    /* Require width and height to be divisible by TU's width and height respectively. */
    if (width & (tuSize - 1)) {
        return false;
    }

    if (height & (tuSize - 1)) {
        return false;
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
    state->block.maxWholeBlockTu = (height >> BSTemporalShift) * state->block.tuPerRow;
    state->block.blocksPerRow = (width + BSTemporal - 1) >> BSTemporalShift;
    state->block.blocksPerCol = (height + BSTemporal - 1) >> BSTemporalShift;

    const uint32_t blockAlignedWidth = ((width + (BSTemporal - 1)) & (-BSTemporal));
    state->blockAligned.tuPerRow = (blockAlignedWidth >> tuWidthShift) << state->block.tuPerBlockDimsShift;
    state->blockAligned.maxWholeBlockY = (state->block.wholeBlocksPerCol << BSTemporalShift);

    return true;
}

TuStateReturn ldeTuCoordsSurfaceRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
{
    assert(state);
    assert(x);
    assert(y);

    if (tuIndex > state->tuTotal) {
        return TUError;
    }

    if (tuIndex == state->tuTotal) {
        return TUComplete;
    }

    *x = ((tuIndex % state->numAcross) << state->tuWidthShift) + state->xOffset;
    *y = ((tuIndex / state->numAcross) << state->tuWidthShift) + state->yOffset;

    return TUMore;
}

TuStateReturn ldeTuCoordsBlockRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
{
    assert(state);
    assert(x);
    assert(y);

    if (tuIndex > state->tuTotal) {
        return TUError;
    }

    if (tuIndex == state->tuTotal) {
        return TUComplete;
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

    return TUMore;
}

uint32_t ldeTuCoordsBlockAlignedIndex(const TUState* state, uint32_t x, uint32_t y)
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

uint32_t ldeTuIndexBlockAlignedIndex(const TUState* state, uint32_t tuIndex)
{
    assert(state);

    uint32_t res = tuIndex;

    if (state->block.tuPerBlockRowRightEdge > 0) {
        const uint32_t blockRowIndex = tuIndex / state->block.tuPerRow;
        res += (state->block.tuPerBlock -
                (state->block.tuPerBlockRowRightEdge * state->block.tuPerBlockDims)) *
               blockRowIndex;
        if ((tuIndex % state->block.tuPerRow) > (state->block.wholeBlocksPerRow * state->block.tuPerBlock)) {
            res += (((tuIndex % state->block.tuPerRow) % state->block.tuPerBlock) /
                    state->block.tuPerBlockRowRightEdge) *
                   (state->block.tuPerBlockDims - state->block.tuPerBlockRowRightEdge);
        }
    }

    if (state->block.tuPerBlockColBottomEdge > 0 && tuIndex > state->block.maxWholeBlockTu) {
        const uint32_t lastRowBlockIndex =
            (tuIndex - state->block.maxWholeBlockTu) /
            (state->block.tuPerBlockColBottomEdge * state->block.tuPerBlockDims);
        res += (state->block.tuPerBlock -
                (state->block.tuPerBlockColBottomEdge * state->block.tuPerBlockDims)) *
               lastRowBlockIndex;
        if (lastRowBlockIndex == state->block.blocksPerRow - 1 && state->block.tuPerBlockRowRightEdge > 0) {
            res += (((tuIndex - state->block.maxWholeBlockTu) %
                     (state->block.tuPerBlockColBottomEdge * state->block.tuPerBlockDims)) /
                    state->block.tuPerBlockRowRightEdge) *
                   (state->block.tuPerBlockDims - state->block.tuPerBlockRowRightEdge);
        }
    }

    assert(res <= state->blockAligned.tuPerRow * (state->block.wholeBlocksPerCol + 1));

    return res;
}

uint32_t ldeTuCoordsSurfaceIndex(const TUState* state, uint32_t x, uint32_t y)
{
    uint32_t res = (y >> state->tuWidthShift) * state->numAcross;
    res += x >> state->tuWidthShift;

    return res;
}

void ldeTuCoordsBlockAlignedRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y)
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
}

uint32_t ldeTuCoordsBlockTuCount(const TUState* state, uint32_t tuIndex)
{
    assert(state);

    const uint32_t rightLimit = state->block.wholeBlocksPerRow << state->block.tuPerBlockShift;

    const uint32_t tuWide =
        ((tuIndex % state->block.tuPerRow) >= rightLimit ? state->block.tuPerBlockRowRightEdge
                                                         : state->block.tuPerBlockDims);
    const uint32_t tuHigh = (tuIndex >= state->block.maxWholeBlockTu ? state->block.tuPerBlockColBottomEdge
                                                                     : state->block.tuPerBlockDims);

    return tuWide * tuHigh;
}

bool ldeTuIsBlockStart(const TUState* state, uint32_t tuIndex)
{
    if (tuIndex >= state->block.maxWholeBlockTu) {
        return (tuIndex - state->block.maxWholeBlockTu) % state->block.tuPerBlockBottomEdge == 0;
    }
    return (tuIndex % state->block.tuPerRow) % state->block.tuPerBlock == 0;
}

/*------------------------------------------------------------------------------*/