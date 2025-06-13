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
#include "LCEVC/build_config.h"
#include "surface/sharpen_common.h"

#include <stddef.h>

#if VN_CORE_FEATURE(SSE)

#include "common/dither.h"
#include "common/memory.h"
#include "common/sse.h"
#include "surface/surface.h"

#include <stdint.h>

/*------------------------------------------------------------------------------*/

/* Converts from u16 back to the starting domain */
static inline __m128i fromU16(__m128i a)
{
    return _mm_srai_epi32(_mm_add_epi32(a, _mm_set1_epi32(1 << 15)), 16);
}

static inline void sharpenSSE(const SharpenArgs_t* args, uint32_t pixelSize, int16_t clamp)
{
    const uint16_t strength = f32ToU16(args->strength);
    const size_t rowCopySize = (size_t)(args->src->width - 2) * pixelSize;
    const __m128i strengthS32 = _mm_set1_epi32(strength);
    const __m128i clampS16 = _mm_set1_epi16(clamp);

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

        /* Process inner loop, ignoring edge pixels. */
        for (uint32_t x = 1; x < (args->tmpSurface->width - 1);) {
            const uint32_t pixelOffset = x * pixelSize;
            const uint32_t remainder = args->tmpSurface->width - x;
            const uint32_t count = minU32(remainder, 16);

            /* Load 16 pixels */
            const Vector2_t centerS16 = loadVector2UNAsS16SSE(&srcRows[1][pixelOffset], count, pixelSize);
            const Vector2_t leftS16 =
                loadVector2UNAsS16SSE(&srcRows[1][pixelOffset - pixelSize], count, pixelSize);
            const Vector2_t rightS16 =
                loadVector2UNAsS16SSE(&srcRows[1][pixelOffset + pixelSize], count, pixelSize);
            const Vector2_t topS16 = loadVector2UNAsS16SSE(&srcRows[0][pixelOffset], count, pixelSize);
            const Vector2_t bottomS16 = loadVector2UNAsS16SSE(&srcRows[2][pixelOffset], count, pixelSize);

            /* Load 16 dither values */
            Vector2_t ditherS16;

            if (ditherBuffer) {
                ditherS16 = expandS8ToS16SSE(loadVectorU8SSE(ditherBuffer, 16));
                ditherBuffer += 16;
            }

            /* Process both halves. */
            Vector2_t result;

            for (uint32_t i = 0; i < 2; ++i) {
                /* (4 * center) - (left + right + top + bottom) */
                const __m128i weightS16 =
                    _mm_sub_epi16(_mm_slli_epi16(centerS16.val[i], 2),
                                  _mm_add_epi16(_mm_add_epi16(topS16.val[i], bottomS16.val[i]),
                                                _mm_add_epi16(leftS16.val[i], rightS16.val[i])));

                /* Apply weight to center in S32. */
                const Vector2_t weightS32 = expandS16ToS32SSE(weightS16);
                Vector2_t centerS32 = expandS16ToS32SSE(centerS16.val[i]);

                for (uint32_t j = 0; j < 2; ++j) {
                    /* weight * strength, strength is Q16 so convert back to starting domain */
                    const __m128i adjustment = fromU16(_mm_mullo_epi32(weightS32.val[j], strengthS32));

                    /* center + adjustment */
                    centerS32.val[j] = _mm_add_epi32(centerS32.val[j], adjustment);
                }

                /* Clamp back to u16 */
                result.val[i] = _mm_packs_epi32(centerS32.val[0], centerS32.val[1]);

                /* Apply dither (with clamp). */
                if (ditherBuffer) {
                    result.val[i] = _mm_adds_epi16(result.val[i], ditherS16.val[i]);
                }
            }

            /* Output result. */
            writeVector2S16AsUNSSE(&tmpRow[pixelOffset], &result, count, pixelSize, clampS16);

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

void sharpenU8SSE(const SharpenArgs_t* args) { sharpenSSE(args, 1, 255); }
void sharpenU10SSE(const SharpenArgs_t* args) { sharpenSSE(args, 2, 1023); }
void sharpenU12SSE(const SharpenArgs_t* args) { sharpenSSE(args, 2, 4095); }
void sharpenU14SSE(const SharpenArgs_t* args) { sharpenSSE(args, 2, 16383); }

/*------------------------------------------------------------------------------*/

static const SharpenFunction_t kTable[FPCount] = {
    sharpenU8SSE,  /* U8*/
    sharpenU10SSE, /* U10 */
    sharpenU12SSE, /* U12 */
    sharpenU14SSE, /* U14 */
    NULL,          /* S8.7 */
    NULL,          /* S10.5 */
    NULL,          /* S12.3 */
    NULL,          /* S14.1 */
};

SharpenFunction_t surfaceSharpenGetFunctionSSE(FixedPoint_t dstFP) { return kTable[dstFP]; }

#else

SharpenFunction_t surfaceSharpenGetFunctionSSE(FixedPoint_t dstFP)
{
    VN_UNUSED(dstFP);
    return NULL;
}

#endif /* VN_CORE_FEATURE(SSE) */

/*------------------------------------------------------------------------------*/
