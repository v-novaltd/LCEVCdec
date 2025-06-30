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

#include "common/types.h"
#include "surface/sharpen_common.h"

#include <LCEVC/build_config.h>
#include <stddef.h>

#if VN_CORE_FEATURE(NEON)

#include "common/dither.h"
#include "common/memory.h"
#include "common/neon.h"
#include "surface/surface.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

static inline int32x4_t fromU16(int32x4_t a)
{
    return vshrq_n_s32(vaddq_s32(a, vdupq_n_s32(1 << 15)), 16);
}

static inline void sharpenNEON(const SharpenArgs_t* args, uint32_t pixelSize, int16_t clamp)
{
    const uint16_t strength = f32ToU16(args->strength);
    const size_t rowCopySize = (size_t)(args->src->width - 2) * pixelSize;
    const int32x4_t strengthS32 = vdupq_n_s32(strength);
    const int16x8_t clampS16 = vdupq_n_s16(clamp);

    for (uint32_t y = 0; y < args->count; ++y) {
        const uint32_t offset = y + args->offset;

        uint8_t* srcRows[3] = {
            surfaceGetLine(args->src, offset - 1),
            surfaceGetLine(args->src, offset),
            surfaceGetLine(args->src, offset + 1),
        };

        uint8_t* tmpRow = surfaceGetLine(args->tmpSurface, offset);

        const int8_t* ditherBuffer = NULL;

        if (ditherIsEnabled(args->dither)) {
            ditherBuffer = ditherGetBuffer(args->dither, args->tmpSurface->width + 16);
        }

        for (uint32_t x = 1; x < (args->tmpSurface->width - 1);) {
            const uint32_t pixelOffset = x * pixelSize;
            const uint32_t remainder = args->tmpSurface->width - x;
            const uint32_t count = minU32(remainder, 16);

            /* Load 16 pixels */
            const int16x8x2_t centerS16 = loadUNAsS16NEON(&srcRows[1][pixelOffset], count, pixelSize);
            const int16x8x2_t leftS16 =
                loadUNAsS16NEON(&srcRows[1][pixelOffset - pixelSize], count, pixelSize);
            const int16x8x2_t rightS16 =
                loadUNAsS16NEON(&srcRows[1][pixelOffset + pixelSize], count, pixelSize);
            const int16x8x2_t topS16 = loadUNAsS16NEON(&srcRows[0][pixelOffset], count, pixelSize);
            const int16x8x2_t bottomS16 = loadUNAsS16NEON(&srcRows[2][pixelOffset], count, pixelSize);

            /* Load 16 dither values */
            int16x8x2_t ditherS16;

            if (ditherBuffer) {
                ditherS16 = expandS8ToS16NEON(loadVectorS8NEON(ditherBuffer, 16));
                ditherBuffer += 16;
            }

            int16x8x2_t result;

            for (int32_t i = 0; i < 2; ++i) {
                /* (4 * center) - (left + right + top + bottom) */
                const int16x8_t weightS16 =
                    vsubq_s16(vshlq_n_s16(centerS16.val[i], 2),
                              vaddq_s16(vaddq_s16(topS16.val[i], bottomS16.val[i]),
                                        vaddq_s16(leftS16.val[i], rightS16.val[i])));

                /* Apply weight to center in S32. */
                const int32x4x2_t weightS32 = expandS16ToS32NEON(weightS16);
                int32x4x2_t centerS32 = expandS16ToS32NEON(centerS16.val[i]);

                for (int32_t j = 0; j < 2; ++j) {
                    /* weight * strength, strength is Q16 so convert back to starting domain */
                    const int32x4_t adjustment = fromU16(vmulq_s32(weightS32.val[j], strengthS32));

                    /* center + adjustment */
                    centerS32.val[j] = vaddq_s32(centerS32.val[j], adjustment);
                }

                /* clamp back to u16 */
                result.val[i] = vcombine_s16(vqmovn_s32(centerS32.val[0]), vqmovn_s32(centerS32.val[1]));

                if (ditherBuffer) {
                    result.val[i] = vqaddq_s16(result.val[i], ditherS16.val[i]);
                }
            }

            /* Output result. */
            writeS16AsUNSSE(&tmpRow[pixelOffset], result, count, pixelSize, clampS16);

            x += count;
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

void sharpenU8NEON(const SharpenArgs_t* args) { sharpenNEON(args, 1, 255); }
void sharpenU10NEON(const SharpenArgs_t* args) { sharpenNEON(args, 2, 1023); }
void sharpenU12NEON(const SharpenArgs_t* args) { sharpenNEON(args, 2, 4095); }
void sharpenU14NEON(const SharpenArgs_t* args) { sharpenNEON(args, 2, 16383); }

/*------------------------------------------------------------------------------*/

static const SharpenFunction_t kTable[FPCount] = {
    sharpenU8NEON,  /* U8*/
    sharpenU10NEON, /* U10 */
    sharpenU12NEON, /* U12 */
    sharpenU14NEON, /* U14 */
    NULL,           /* S8.7 */
    NULL,           /* S10.5 */
    NULL,           /* S12.3 */
    NULL,           /* S14.1 */
};

SharpenFunction_t surfaceSharpenGetFunctionNEON(FixedPoint_t dstFP) { return kTable[dstFP]; }

#else

SharpenFunction_t surfaceSharpenGetFunctionNEON(FixedPoint_t dstFP)
{
    VN_UNUSED(dstFP);
    return NULL;
}

#endif /* VN_CORE_FEATURE(NEON) */

/*------------------------------------------------------------------------------*/
