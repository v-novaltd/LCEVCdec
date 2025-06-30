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
#include "surface/blit.h"
#include "surface/blit_common.h"

#include <LCEVC/build_config.h>
#include <stddef.h>

#if VN_CORE_FEATURE(NEON)

#include "common/neon.h"
#include "surface/surface.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

/* Macros used for fixed values. This is a requirement for building with Clang
 * as it is more strict about passing in constants to the shift intrinsics. It
 * does not interpret the value on a static const uint32_t variable unfortunately. */
#define VN_SHIFT_U8() 7
#define VN_SHIFT_S7() 8

/*------------------------------------------------------------------------------*/

static const uint32_t kStep = 16;

/*! \brief Rounds width down to SIMD alignment requirements. */
static inline uint32_t simdAlignment(const uint32_t width)
{
    return ldlAlignTruncU32(width, kStep);
}

/*------------------------------------------------------------------------------*/

/*! \brief Performs an additive blit of an S16 input onto a U8 destination in NEON */
void ldlAddU8_NEON(const BlitArgs_t* args)
{
    const int16x8_t usToSOffset = vdupq_n_s16(16384);
    const int16x8_t fractOffset = vdupq_n_s16(64);
    const int16x8_t signOffset = vdupq_n_s16(128);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        uint8_t* dstPixel = dstRow;
        uint32_t x = 0;

        /* SIMD loop */
        for (; x < simdWidth; x += kStep, dstPixel += kStep, srcPixel0 += kStep, srcPixel1 += kStep) {
            /* Load 16-pixels */
            uint8x16_t dstResult = vld1q_u8(dstPixel);
            const int16x8_t src0 = vld1q_s16(srcPixel0);
            const int16x8_t src1 = vld1q_s16(srcPixel1);

            /* val <<= 7 and cast straight to s16. */
            int16x8_t dst0 = vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(dstResult), VN_SHIFT_U8()));
            int16x8_t dst1 = vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(dstResult), VN_SHIFT_U8()));

            /* val -= 0x4000 */
            dst0 = vsubq_s16(dst0, usToSOffset);
            dst1 = vsubq_s16(dst1, usToSOffset);

            /* val += src */
            dst0 = vqaddq_s16(dst0, src0);
            dst1 = vqaddq_s16(dst1, src1);

            /* val += 0x40 */
            dst0 = vqaddq_s16(dst0, fractOffset);
            dst1 = vqaddq_s16(dst1, fractOffset);

            /* val >>= 7 */
            dst0 = vshrq_n_s16(dst0, VN_SHIFT_U8());
            dst1 = vshrq_n_s16(dst1, VN_SHIFT_U8());

            /* val += 0x80 */
            dst0 = vaddq_s16(dst0, signOffset);
            dst1 = vaddq_s16(dst1, signOffset);

            /* Saturated cast back to u8 */
            dstResult = vcombine_u8(vqmovun_s16(dst0), vqmovun_s16(dst1));

            /* Store 16-pixels */
            vst1q_u8(dstPixel, dstResult);
        }

        /* Remainder */
        for (; x < dst->width; x += 1, dstPixel += 1, srcPixel0 += 1) {
            int32_t pel = fpU8ToS8(*dstPixel);
            pel += *srcPixel0;
            *dstPixel = fpS8ToU8(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S8 input onto a U8 destination in NEON */
void ldlAddS7_U8_NEON(const BlitArgs_t* args)
{
    const int16x8_t usToSOffset = vdupq_n_s16(16384);
    const int16x8_t fractOffset = vdupq_n_s16(64);
    const int16x8_t signOffset = vdupq_n_s16(128);

    VN_BLIT_SIMD_BOILERPLATE(int8_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        uint8_t* dstPixel = dstRow;
        const int8_t* srcPixel = srcRow;
        uint32_t x = 0;

        /* SIMD loop*/
        for (; x < simdWidth; x += kStep, dstPixel += kStep, srcPixel += kStep) {
            /* Load 16-pixels */
            uint8x16_t dstResult = vld1q_u8(dstPixel);
            int8x16_t srcFull = vld1q_s8(srcPixel);

            /* val <<= 7 and cast straight to s16. */
            int16x8_t dst0 = vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(dstResult), VN_SHIFT_U8()));
            int16x8_t dst1 = vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(dstResult), VN_SHIFT_U8()));

            /* Cast from i8 to i16 and shift left << 8 */
            int16x8_t src0 = vshll_n_s8(vget_low_s8(srcFull), VN_SHIFT_S7());
            int16x8_t src1 = vshll_n_s8(vget_high_s8(srcFull), VN_SHIFT_S7());

            /* val -= 0x4000 */
            dst0 = vsubq_s16(dst0, usToSOffset);
            dst1 = vsubq_s16(dst1, usToSOffset);

            /* val += src */
            dst0 = vqaddq_s16(dst0, src0);
            dst1 = vqaddq_s16(dst1, src1);

            /* val += 0x40 */
            dst0 = vqaddq_s16(dst0, fractOffset);
            dst1 = vqaddq_s16(dst1, fractOffset);

            /* val >>= 7 */
            dst0 = vshrq_n_s16(dst0, VN_SHIFT_U8());
            dst1 = vshrq_n_s16(dst1, VN_SHIFT_U8());

            /* val += 0x80 */
            dst0 = vaddq_s16(dst0, signOffset);
            dst1 = vaddq_s16(dst1, signOffset);

            /* Saturated conversion to u8 */
            dstResult = vcombine_u8(vqmovun_s16(dst0), vqmovun_s16(dst1));

            /* Store 16-pixels */
            vst1q_u8(dstPixel, dstResult);
        }

        /* Remainder */
        for (; x < dst->width; x += 1, dstPixel += 1, srcPixel += 1) {
            int32_t pel = fpU8ToS8(*dstPixel);
            pel += ((int16_t)*srcPixel) << VN_SHIFT_S7();
            *dstPixel = fpS8ToU8(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S16 input onto a U16 destination in NEON */
void ldlAddUN_NEON(const BlitArgs_t* args, int32_t shift, int16_t roundingOffset,
                   int16_t signOffset, int16_t resultMax, FixedPoint_t unsignedFP)
{
    FixedPointPromotionFunction_t uToS = ldlFixedPointGetPromotionFunction(unsignedFP);
    FixedPointDemotionFunction_t sToU = ldlFixedPointGetDemotionFunction(unsignedFP);

    const int16x8_t shiftUp = vdupq_n_s16(shift);
    const int16x8_t shiftDown = vdupq_n_s16(-shift);
    const int16x8_t usToSOffset = vdupq_n_s16(16384);
    const int16x8_t roundingOffsetV = vdupq_n_s16(roundingOffset);
    const int16x8_t signOffsetV = vdupq_n_s16(signOffset);
    const int16x8_t minV = vdupq_n_s16(0);
    const int16x8_t maxV = vdupq_n_s16(resultMax);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, int16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        int16_t* dstPixel0 = dstRow;
        int16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        /* SIMD loop*/
        for (; x < simdWidth; x += kStep, dstPixel0 += kStep, dstPixel1 += kStep,
                              srcPixel0 += kStep, srcPixel1 += kStep) {
            /* Load 16-pixels. Note: dst_X/Y are unsigned, but can load fine */
            int16x8_t dst0 = vld1q_s16(dstPixel0);
            int16x8_t dst1 = vld1q_s16(dstPixel1);
            const int16x8_t src0 = vld1q_s16(srcPixel0);
            const int16x8_t src1 = vld1q_s16(srcPixel1);

            /* val <<= shift */
            dst0 = vshlq_s16(dst0, shiftUp);
            dst1 = vshlq_s16(dst1, shiftUp);

            /* val -= 0x4000 */
            dst0 = vsubq_s16(dst0, usToSOffset);
            dst1 = vsubq_s16(dst1, usToSOffset);

            /* val += src */
            dst0 = vqaddq_s16(dst0, src0);
            dst1 = vqaddq_s16(dst1, src1);

            /* val += rounding */
            dst0 = vqaddq_s16(dst0, roundingOffsetV);
            dst1 = vqaddq_s16(dst1, roundingOffsetV);

            /* val >>= 5 */ // @todo(bob): This can be replaced with rounding shift and above removed?
            dst0 = vshlq_s16(dst0, shiftDown);
            dst1 = vshlq_s16(dst1, shiftDown);

            /* val += sign offset */
            dst0 = vaddq_s16(dst0, signOffsetV);
            dst1 = vaddq_s16(dst1, signOffsetV);

            /* clamp to unsigned range */
            dst0 = vmaxq_s16(vminq_s16(dst0, maxV), minV);
            dst1 = vmaxq_s16(vminq_s16(dst1, maxV), minV);

            /* Store 16-pixels */
            vst1q_s16(dstPixel0, dst0);
            vst1q_s16(dstPixel1, dst1);
        }

        /* Remainder */
        for (; x < dst->width; x += 1, dstPixel0 += 1, srcPixel0 += 1) {
            int32_t pel = uToS(*dstPixel0);
            pel += *srcPixel0;
            *dstPixel0 = sToU(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S16 input onto a S16 destination in NEON */
void ldlAddS16_NEON(const BlitArgs_t* args)
{
    VN_BLIT_SIMD_BOILERPLATE(int16_t, int16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        int16_t* dstPixel0 = dstRow;
        int16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        /* SIMD loop*/
        for (; x < simdWidth; x += kStep, dstPixel0 += kStep, dstPixel1 += kStep,
                              srcPixel0 += kStep, srcPixel1 += kStep) {
            /* Load 16-pixels. */
            int16x8_t dst0 = vld1q_s16(dstPixel0);
            int16x8_t dst1 = vld1q_s16(dstPixel1);
            const int16x8_t src0 = vld1q_s16(srcPixel0);
            const int16x8_t src1 = vld1q_s16(srcPixel1);

            /* val += src */
            dst0 = vqaddq_s16(dst0, src0);
            dst1 = vqaddq_s16(dst1, src1);

            /* Store 16-pixels */
            vst1q_s16(dstPixel0, dst0);
            vst1q_s16(dstPixel1, dst1);
        }

        /* Remainder */
        for (; x < dst->width; x += 1, dstPixel0 += 1, srcPixel0 += 1) {
            int32_t pel = *dstPixel0;
            pel += *srcPixel0;
            *dstPixel0 = saturateS16(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

static void ldlAddU10_NEON(const BlitArgs_t* args) { ldlAddUN_NEON(args, 5, 16, 512, 1023, FPU10); }

static void ldlAddU12_NEON(const BlitArgs_t* args) { ldlAddUN_NEON(args, 3, 4, 2048, 4095, FPU12); }

static void ldlAddU14_NEON(const BlitArgs_t* args)
{
    ldlAddUN_NEON(args, 1, 1, 8192, 16383, FPU14);
}

/*------------------------------------------------------------------------------*/

/* clang-format off */

static const BlitFunction_t kAddTable[FPCount] = {
	&ldlAddU8_NEON,  /* FP_U8 */
	&ldlAddU10_NEON, /* FP_U10 */
	&ldlAddU12_NEON, /* FP_U12 */
	&ldlAddU14_NEON, /* FP_U14 */
	&ldlAddS16_NEON, /* FP_S8_7 */
	&ldlAddS16_NEON, /* FP_S10_5 */
	&ldlAddS16_NEON, /* FP_S12_3 */
	&ldlAddS16_NEON, /* FP_S14_1 */
};

/* clang-format on */

/*------------------------------------------------------------------------------*/

BlitFunction_t surfaceBlitGetFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending)
{
    if (blending == BMAdd) {
        /* Special case handling for a src of U8 (which is really an S7).
         * @todo(bob): Remove this. */
        if (dstFP == FPU8 && srcFP == FPU8) {
            return &ldlAddS7_U8_NEON;
        }

        /* Ensure formats match */
        assert(ldlFixedPointIsValid(dstFP));
        assert(ldlFixedPointHighPrecision(dstFP) == srcFP);

        return kAddTable[dstFP];
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/

#else

BlitFunction_t surfaceBlitGetFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending)
{
    VN_UNUSED(dstFP);
    VN_UNUSED(srcFP);
    VN_UNUSED(blending);
    return NULL;
}

#endif
