/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/dither.h"
#include "common/memory.h"
#include "common/types.h"
#include "surface/sharpen_common.h"
#include "surface/surface.h"

/*
 * @todo(bob): Investigate optimizations here, likely on a per-case basis.
 * Specifically, we can omit the need to copy back from the
 * temporary surface if we have allocated both surfaces on host
 * memory internally (or user supplied functions can provide
 * some guarantees) then we can just pointer swap surfaces. */

/*------------------------------------------------------------------------------*/

/* Converts from u16 back to the starting domain */
static inline int32_t fromU16(int32_t val) { return (val + (1 << 15)) >> 16; }

static inline void sharpenKernelU8(int32_t strength, uint32_t x, uint8_t* src[3], uint8_t* dst,
                                   const int8_t dither)
{
    const uint8_t center = src[1][x];
    const uint8_t left = src[1][x - 1];
    const uint8_t right = src[1][x + 1];
    const uint8_t top = src[0][x];
    const uint8_t bottom = src[2][x];
    const int32_t weight = ((int32_t)center << 2) - (left + right + top + bottom);
    const int32_t coeff = fromU16(strength * weight);
    dst[x] = (uint8_t)clampS32(center + coeff + dither, 0, UINT8_MAX);
}

static inline void sharpenKernelU16(int32_t strength, uint32_t x, uint8_t* src[3], uint8_t* dst,
                                    const int8_t dither, int32_t resultClamp)
{
    const uint16_t center = ((const uint16_t*)src[1])[x];
    const uint16_t left = ((const uint16_t*)src[1])[x - 1];
    const uint16_t right = ((const uint16_t*)src[1])[x + 1];
    const uint16_t top = ((const uint16_t*)src[0])[x];
    const uint16_t bottom = ((const uint16_t*)src[2])[x];
    const int32_t weight = ((int32_t)center << 2) - (left + right + top + bottom);
    const int32_t coeff = fromU16(strength * weight);
    ((uint16_t*)dst)[x] = (uint16_t)clampS32(center + coeff + dither, 0, resultClamp);
}

static inline void sharpen(const SharpenArgs_t* args, uint32_t pixelSize, int32_t resultClamp)
{
    const int32_t strength = f32ToU16(args->strength);
    const size_t rowCopySize = (size_t)(args->src->width - 2) * pixelSize;

    for (uint32_t y = 0; y < args->count; ++y) {
        const uint32_t offset = y + args->offset;

        uint8_t* srcRows[3] = {
            surfaceGetLine(args->src, offset - 1),
            surfaceGetLine(args->src, offset),
            surfaceGetLine(args->src, offset + 1),
        };

        uint8_t* tmpRow = surfaceGetLine(args->tmpSurface, offset);

        const int8_t* ditherBuffer = NULL;
        int8_t ditherValue = 0;

        if (ditherIsEnabled(args->dither)) {
            ditherBuffer = ditherGetBuffer(args->dither, args->tmpSurface->width);
            ditherValue = *ditherBuffer++;
        }

        /* Process inner loop, ignoring edge pixels. */
        for (uint32_t x = 1; x < (args->tmpSurface->width - 1); ++x) {
            if (pixelSize == sizeof(uint16_t)) {
                sharpenKernelU16(strength, x, srcRows, tmpRow, ditherValue, resultClamp);
            } else {
                sharpenKernelU8(strength, x, srcRows, tmpRow, ditherValue);
            }

            if (ditherBuffer) {
                ditherValue = *ditherBuffer++;
            }
        }

        /* Safe now to copy back from the intermediate surface to the destination now that
         * we know we're not going to read from the previous line any more, and ensure that
         * left and right columns are not overwritten. */
        if (y > 1) {
            const uint8_t* sharpenedSrc = surfaceGetLine(args->tmpSurface, offset - 1);
            memoryCopy(srcRows[0] + pixelSize, sharpenedSrc + pixelSize, rowCopySize);
        }
    }
}

void sharpenU8(const SharpenArgs_t* args) { sharpen(args, sizeof(uint8_t), 255); }
void sharpenU10(const SharpenArgs_t* args) { sharpen(args, sizeof(uint16_t), 1023); }
void sharpenU12(const SharpenArgs_t* args) { sharpen(args, sizeof(uint16_t), 4095); }
void sharpenU14(const SharpenArgs_t* args) { sharpen(args, sizeof(uint16_t), 16383); }

/*------------------------------------------------------------------------------*/

static const SharpenFunction_t kTable[FPCount] = {
    sharpenU8,  /* U8*/
    sharpenU10, /* U10 */
    sharpenU12, /* U12 */
    sharpenU14, /* U14 */
    NULL,       /* S8.7 */
    NULL,       /* S10.5 */
    NULL,       /* S12.3 */
    NULL,       /* S14.1 */
};

SharpenFunction_t surfaceSharpenGetFunctionScalar(FixedPoint_t dstFP) { return kTable[dstFP]; }

/*------------------------------------------------------------------------------*/