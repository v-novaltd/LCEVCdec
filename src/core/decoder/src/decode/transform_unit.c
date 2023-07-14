#include "decode/transform_unit.h"

#include "common/types.h"

#include <assert.h>

int32_t tuStateInitialise(TUState_t* state, const TileState_t* tile, uint32_t tuSize)
{
    const uint32_t width = tile->width;
    const uint32_t height = tile->height;

    assert(state);
    assert(tuSize);

    if (width % tuSize) {
        return -1;
    }

    if (height % tuSize) {
        return -1;
    }

    state->tuSize = tuSize;
    state->numAcross = width / state->tuSize;
    state->tuTotal = state->numAcross * (height / state->tuSize);
    state->xOffset = tile->x;
    state->yOffset = tile->y;
    state->block.tuPerBlockDims = BSTemporal / state->tuSize;
    state->block.tuPerBlock = state->block.tuPerBlockDims * state->block.tuPerBlockDims;
    state->block.tuPerBlockRowRightEdge = ((width % BSTemporal) / state->tuSize);
    state->block.tuPerBlockColBottomEdge = ((height % BSTemporal) / state->tuSize);
    state->block.tuPerBlockBottomEdge = state->block.tuPerBlockColBottomEdge * state->block.tuPerBlockDims;
    state->block.tuPerRow = state->numAcross * state->block.tuPerBlockDims;
    state->block.wholeBlocksPerRow = width / BSTemporal;
    state->block.wholeBlocksPerCol = height / BSTemporal;
    state->block.blocksPerRow = (width + BSTemporal - 1) / BSTemporal;
    state->block.blocksPerCol = (height + BSTemporal - 1) / BSTemporal;

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

    *x = ((tuIndex % state->numAcross) * state->tuSize) + state->xOffset;
    *y = ((tuIndex / state->numAcross) * state->tuSize) + state->yOffset;

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

    /* Determine row of blocks this TU falls into */
    const uint32_t blockRowIndex =
        tuIndex / state->block.tuPerRow; /* Index of the block row this TU falls into. */
    const uint32_t rowTUIndex = tuIndex % state->block.tuPerRow; /* TU index of the within the block row. */

    /* Determine column of blocks this TU falls into */
    uint32_t blockColIndex = 0;
    uint32_t blockTUIndex = 0;

    if (blockRowIndex >= state->block.wholeBlocksPerCol) {
        /* Handle bottom edge case where each block will contain less TUs */
        assert(blockRowIndex == state->block.wholeBlocksPerCol);
        blockColIndex = rowTUIndex / state->block.tuPerBlockBottomEdge;
        blockTUIndex = rowTUIndex % state->block.tuPerBlockBottomEdge;
    } else {
        blockColIndex = rowTUIndex / state->block.tuPerBlock;
        blockTUIndex = rowTUIndex % state->block.tuPerBlock;
    }

    /* Determine coord of TU inside block */
    uint32_t tuXCoord = 0;
    uint32_t tuYCoord = 0;

    if (blockColIndex >= state->block.wholeBlocksPerRow) {
        assert(blockColIndex == state->block.wholeBlocksPerRow);
        tuYCoord = blockTUIndex / state->block.tuPerBlockRowRightEdge;
        tuXCoord = blockTUIndex % state->block.tuPerBlockRowRightEdge;
    } else {
        tuYCoord = blockTUIndex / state->block.tuPerBlockDims;
        tuXCoord = blockTUIndex % state->block.tuPerBlockDims;
    }

    /* Offset TU coord to full surface */
    tuXCoord += state->block.tuPerBlockDims * blockColIndex;
    tuYCoord += state->block.tuPerBlockDims * blockRowIndex;

    /* Convert TU coord to pixel position. */
    *x = (tuXCoord * state->tuSize) + state->xOffset;
    *y = (tuYCoord * state->tuSize) + state->yOffset;

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

    uint32_t tuWide = 0;
    uint32_t tuHigh = 0;

    if (xPos >= rightLimit) {
        if (yPos >= bottomLimit) {
            tuWide = state->block.tuPerBlockRowRightEdge;
            tuHigh = state->block.tuPerBlockColBottomEdge;
        } else {
            tuWide = state->block.tuPerBlockRowRightEdge;
            tuHigh = state->block.tuPerBlockDims;
        }
    } else if (yPos >= bottomLimit) {
        tuWide = state->block.tuPerBlockDims;
        tuHigh = state->block.tuPerBlockColBottomEdge;
    } else {
        tuWide = state->block.tuPerBlockDims;
        tuHigh = state->block.tuPerBlockDims;
    }

    *tuCount = tuWide * tuHigh;
    *blockWidth = tuWide * state->tuSize;
    *blockHeight = tuHigh * state->tuSize;
}

/*------------------------------------------------------------------------------*/