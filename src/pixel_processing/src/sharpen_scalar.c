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

#include "fp_types.h"
#include "sharpen_common.h"

#include <LCEVC/common/limit.h>
#include <LCEVC/pixel_processing/dither.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*------------------------------------------------------------------------------*/

/* Converts from u16 back to the starting domain */
static inline int32_t fromU16(int32_t val) { return (val + (1 << 15)) >> 16; }

static inline void sharpenKernelU8(int32_t strength, uint32_t x, uint8_t* src[3], uint8_t* dst)
{
    const uint8_t center = src[1][x];
    const uint8_t left = src[1][x - 1];
    const uint8_t right = src[1][x + 1];
    const uint8_t top = src[0][x];
    const uint8_t bottom = src[2][x];
    const int32_t weight = ((int32_t)center << 2) - (left + right + top + bottom);
    const int32_t coeff = fromU16(strength * weight);
    dst[x - 1] = (uint8_t)clampS32(center + coeff, 0, UINT8_MAX);
}

static inline void sharpenKernelU16(int32_t strength, uint32_t x, uint16_t* src[3], uint16_t* dst,
                                    int32_t resultClamp)
{
    const uint16_t center = src[1][x];
    const uint16_t left = src[1][x - 1];
    const uint16_t right = src[1][x + 1];
    const uint16_t top = src[0][x];
    const uint16_t bottom = src[2][x];
    const int32_t weight = ((int32_t)center << 2) - (left + right + top + bottom);
    const int32_t coeff = fromU16(strength * weight);
    dst[x - 1] = (uint16_t)clampS32(center + coeff, 0, resultClamp);
}

static inline void sharpenU8(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                             float strength, LdppDitherSlice* dither)
{
    if (width < 3 || height < 3) {
        return;
    }

    const int32_t strengthInt = f32ToU16(strength);

    const uint32_t tmpSize = width - 2;
    uint8_t* tmpRow[2] = {alloca(tmpSize), alloca(tmpSize)};

    memcpy(tmpRow[0], getRowU8(src, stride, 0) + 1, tmpSize);
    for (uint32_t y = 1; y < height - 1; ++y) {
        uint8_t* srcRows[3] = {
            getRowU8(src, stride, y - 1),
            getRowU8(src, stride, y),
            getRowU8(src, stride, y + 1),
        };

        const uint16_t* ditherBuffer = NULL;
        if (dither) {
            ditherBuffer = ldppDitherGetBuffer(dither, tmpSize);
        }

        /* Process inner loop, ignoring edge pixels. */
        for (uint32_t x = 1; x < width - 1; ++x) {
            sharpenKernelU8(strengthInt, x, srcRows, tmpRow[1]);

            if (ditherBuffer) {
                int32_t dithered = tmpRow[1][x - 1];
                ldppDitherApplyScalar(&dithered, &ditherBuffer, 0, dither->strength);
                tmpRow[1][x - 1] = (uint8_t)clampS32(dithered, 0, UINT8_MAX);
            }
        }

        memcpy(getRowU8(src, stride, y - 1) + 1, tmpRow[0], tmpSize);
        uint8_t* const swap = tmpRow[0];
        tmpRow[0] = tmpRow[1];
        tmpRow[1] = swap;
    }

    memcpy(getRowU8(src, stride, height - 2) + 1, tmpRow[0], tmpSize);
}

static inline void sharpenU16(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                              float strength, int32_t resultClamp, LdppDitherSlice* dither)
{
    if (width < 3 || height < 3) {
        return;
    }

    const int32_t strengthInt = f32ToU16(strength);

    const uint32_t tmpPixelCount = width - 2;
    const uint32_t tmpSize = tmpPixelCount * sizeof(uint16_t);
    uint16_t* tmpRow[2] = {(uint16_t*)alloca(tmpSize), (uint16_t*)alloca(tmpSize)};

    memcpy(tmpRow[0], ((uint16_t*)getRowU8(src, stride, 0)) + 1, tmpSize);
    for (uint32_t y = 1; y < height - 1; ++y) {
        uint16_t* srcRows[3] = {
            (uint16_t*)getRowU8(src, stride, y - 1),
            (uint16_t*)getRowU8(src, stride, y),
            (uint16_t*)getRowU8(src, stride, y + 1),
        };

        const uint16_t* ditherBuffer = NULL;
        if (dither) {
            ditherBuffer = ldppDitherGetBuffer(dither, tmpPixelCount);
        }

        /* Process inner loop, ignoring edge pixels. */
        for (uint32_t x = 1; x < width - 1; ++x) {
            sharpenKernelU16(strengthInt, x, srcRows, tmpRow[1], resultClamp);

            if (ditherBuffer) {
                int32_t dithered = tmpRow[1][x - 1];
                ldppDitherApplyScalar(&dithered, &ditherBuffer, 0, dither->strength);
                tmpRow[1][x - 1] = (uint16_t)clampS32(dithered, 0, resultClamp);
            }
        }

        memcpy(((uint16_t*)getRowU8(src, stride, y - 1)) + 1, tmpRow[0], tmpSize);
        uint16_t* const swap = tmpRow[0];
        tmpRow[0] = tmpRow[1];
        tmpRow[1] = swap;
    }

    memcpy(((uint16_t*)getRowU8(src, stride, height - 2)) + 1, tmpRow[0], tmpSize);
}

static inline void sharpenU10(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                              float strength, LdppDitherSlice* dither)
{
    sharpenU16(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU10), dither);
}

static inline void sharpenU12(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                              float strength, LdppDitherSlice* dither)
{
    sharpenU16(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU12), dither);
}

static inline void sharpenU14(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                              float strength, LdppDitherSlice* dither)
{
    sharpenU16(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU14), dither);
}

/*------------------------------------------------------------------------------*/

static const SharpenFunction kTable[LdpFPCount] = {
    sharpenU8,  /* U8 */
    sharpenU10, /* U10 */
    sharpenU12, /* U12 */
    sharpenU14, /* U14 */
    NULL,       /* S8.7 */
    NULL,       /* S10.5 */
    NULL,       /* S12.3 */
    NULL,       /* S14.1 */
};

SharpenFunction sharpenGetFunctionScalar(LdpFixedPoint fixedPoint) { return kTable[fixedPoint]; }

/*------------------------------------------------------------------------------*/
