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

#include "surface/upscale_common.h"

#include "common/types.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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
