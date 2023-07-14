/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "surface/upscale_common.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

void upscaleHorizontalGetCoords(uint32_t width, uint32_t xStart, uint32_t xEnd, uint32_t kernelSize,
                                uint32_t middleAlignment, UpscaleHorizontalCoords_t* coords)
{
    /* This is the size required for left/right edge processing when xStart & xEnd
     * are near surface bounds, aligned to ensure the margins are working on a minimum
     * of 2-pixels */
    const uint32_t edgeMargin = alignU32(kernelSize >> 1, 2);

    coords->leftStart = xStart;
    coords->leftEnd = (((int32_t)xStart - (int32_t)edgeMargin) < 0) ? edgeMargin : xStart;

    coords->rightStart = (xEnd > (width - edgeMargin)) ? alignU32((xEnd - edgeMargin - 1), 2) : xEnd;
    coords->rightEnd = xEnd;

    /* We've handled out-of-bounds read, now middle needs alignment for SIMD, so
     * extend right edge by the amount of overflow. */
    if (middleAlignment) {
        coords->rightStart -= (coords->rightStart - coords->leftEnd) % middleAlignment;
    }

    /* Update middle coords. */
    coords->start = coords->leftEnd;
    coords->end = coords->rightStart;

    /* Verification checks for PA application. */
    assert(((coords->leftEnd - coords->leftStart) % 2) == 0); /* Verify left edge width is aligned to 2. */
    assert(((coords->end - coords->start) % 2) == 0); /* Verify middle width is aligned to 2. */
    assert((coords->rightStart % 2) ==
           0); /* Verify right start is aligned to 2 (implied by the 2 previous checks) */
}

bool upscaleHorizontalCoordsIsLeftValid(const UpscaleHorizontalCoords_t* coords)
{
    return ((int32_t)coords->leftEnd - (int32_t)coords->leftStart) > 0;
}

bool upscaleHorizontalCoordsIsRightValid(const UpscaleHorizontalCoords_t* coords)
{
    return ((int32_t)coords->rightEnd - (int32_t)coords->rightStart) > 0;
}

/*------------------------------------------------------------------------------*/