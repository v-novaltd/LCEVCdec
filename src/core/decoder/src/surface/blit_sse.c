/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "common/platform.h"
#include "common/simd.h"
#include "common/types.h"
#include "surface/blit.h"
#include "surface/blit_common.h"
#include "surface/surface.h"

#include <assert.h>

#if VN_CORE_FEATURE(SSE)

/*------------------------------------------------------------------------------*/

static const uint32_t kStep = 16;

static inline uint32_t simdAlignment(const uint32_t width) { return alignTruncU32(width, kStep); }

/*------------------------------------------------------------------------------*/

/*! \brief Performs an additive blit of an S16 input onto a U8 destination in SSE */
static void addU8_SSE(const BlitArgs_t* args)
{
    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i fractOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        uint8_t* dstPixel = dstRow;
        uint32_t x = 0;

        /* SIMD loop */
        for (; x < simdWidth; x += kStep, dstPixel += kStep, srcPixel0 += kStep, srcPixel1 += kStep) {
            /* Load 16-pixels */
            __m128i dstRight = _mm_loadu_si128((const __m128i*)dstPixel);
            const __m128i srcLeft = _mm_loadu_si128((const __m128i*)srcPixel0);
            const __m128i srcRight = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* Cast from u8 to u16 */
            __m128i dstLeft = _mm_unpacklo_epi8(dstRight, _mm_setzero_si128());
            dstRight = _mm_unpackhi_epi8(dstRight, _mm_setzero_si128());

            /* val <<= 7 */
            dstLeft = _mm_slli_epi16(dstLeft, kShift);
            dstRight = _mm_slli_epi16(dstRight, kShift);

            /* val -= 0x4000 */
            dstLeft = _mm_sub_epi16(dstLeft, usToSOffset);
            dstRight = _mm_sub_epi16(dstRight, usToSOffset);

            /* val += src */
            dstLeft = _mm_adds_epi16(dstLeft, srcLeft);
            dstRight = _mm_adds_epi16(dstRight, srcRight);

            /* val += 0x40*/
            dstLeft = _mm_adds_epi16(dstLeft, fractOffset);
            dstRight = _mm_adds_epi16(dstRight, fractOffset);

            /* val >>= 7 */
            dstLeft = _mm_srai_epi16(dstLeft, kShift);
            dstRight = _mm_srai_epi16(dstRight, kShift);

            /* val += 0x80 */
            dstLeft = _mm_add_epi16(dstLeft, signOffset);
            dstRight = _mm_add_epi16(dstRight, signOffset);

            /* Saturated cast back to u8 */
            const __m128i dstResult = _mm_packus_epi16(dstLeft, dstRight);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel, dstResult);
        }

        /* Remainder */
        for (; x < width; x += 1, dstPixel += 1, srcPixel0 += 1) {
            int32_t pel = fpU8ToS8(*dstPixel);
            pel += *srcPixel0;
            *dstPixel = fpS8ToU8(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S8 input onto a U8 destination in SSE */
static void addS7_U8_SSE(const BlitArgs_t* args)
{
    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i roundingOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    VN_BLIT_SIMD_BOILERPLATE(int8_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int8_t* srcPixel = srcRow;
        uint8_t* dstPixel = dstRow;
        uint32_t x = 0;

        /* SIMD loop*/
        for (; x < simdWidth; x += kStep, dstPixel += kStep, srcPixel += kStep) {
            /* Load 16-pixels */
            __m128i dst1 = _mm_loadu_si128((const __m128i*)dstPixel);
            __m128i src1 = _mm_loadu_si128((const __m128i*)srcPixel);

            /* Cast from u8 to u16 */
            __m128i dst0 = _mm_unpacklo_epi8(dst1, _mm_setzero_si128());
            dst1 = _mm_unpackhi_epi8(dst1, _mm_setzero_si128());

            /* Cast from i8 to i16 and shift left << 8 */
            __m128i src0 = _mm_unpacklo_epi8(_mm_setzero_si128(), src1);
            src1 = _mm_unpackhi_epi8(_mm_setzero_si128(), src1);

            /* val <<= 7 */
            dst0 = _mm_slli_epi16(dst0, kShift);
            dst1 = _mm_slli_epi16(dst1, kShift);

            /* val -= 0x4000 */
            dst0 = _mm_sub_epi16(dst0, usToSOffset);
            dst1 = _mm_sub_epi16(dst1, usToSOffset);

            /* val += src */
            dst0 = _mm_adds_epi16(dst0, src0);
            dst1 = _mm_adds_epi16(dst1, src1);

            /* val += 0x40*/
            dst0 = _mm_adds_epi16(dst0, roundingOffset);
            dst1 = _mm_adds_epi16(dst1, roundingOffset);

            /* val >>= 7 */
            dst0 = _mm_srai_epi16(dst0, kShift);
            dst1 = _mm_srai_epi16(dst1, kShift);

            /* val += 0x80 */
            dst0 = _mm_add_epi16(dst0, signOffset);
            dst1 = _mm_add_epi16(dst1, signOffset);

            /* Saturated cast back to u8 */
            const __m128i dstResult = _mm_packus_epi16(dst0, dst1);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel, dstResult);
        }

        /* Remainder */
        for (; x < width; x += 1, dstPixel += 1, srcPixel += 1) {
            int32_t pel = fpU8ToS8(*dstPixel);
            pel += ((int16_t)*srcPixel) << 8;
            *dstPixel = fpS8ToU8(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S16 input onto a U16 destination in SSE */
static void addUN_SSE(const BlitArgs_t* args, int32_t shift, int16_t roundingOffset,
                      int16_t signOffset, int16_t resultMax, FixedPoint_t unsignedFP)
{
    FixedPointPromotionFunction_t uToS = fixedPointGetPromotionFunction(unsignedFP);
    FixedPointDemotionFunction_t sToU = fixedPointGetDemotionFunction(unsignedFP);

    const __m128i usToSOffset = _mm_set1_epi16(16384);
    const __m128i roundingOffsetV = _mm_set1_epi16(roundingOffset);
    const __m128i signOffsetV = _mm_set1_epi16(signOffset);
    const __m128i minV = _mm_set1_epi16(0);
    const __m128i maxV = _mm_set1_epi16(resultMax);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, uint16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        uint16_t* dstPixel0 = dstRow;
        uint16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        /* SIMD loop*/
        for (; x < simdWidth; x += kStep, dstPixel0 += kStep, dstPixel1 += kStep,
                              srcPixel0 += kStep, srcPixel1 += kStep) {
            /* Load 16-pixels */
            __m128i dst0 = _mm_loadu_si128((const __m128i*)dstPixel0);
            __m128i dst1 = _mm_loadu_si128((const __m128i*)dstPixel1);
            const __m128i src0 = _mm_loadu_si128((const __m128i*)srcPixel0);
            const __m128i src1 = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val <<= shift */
            dst0 = _mm_slli_epi16(dst0, shift);
            dst1 = _mm_slli_epi16(dst1, shift);

            /* val -= 0x4000 */
            dst0 = _mm_sub_epi16(dst0, usToSOffset);
            dst1 = _mm_sub_epi16(dst1, usToSOffset);

            /* val += src */
            dst0 = _mm_adds_epi16(dst0, src0);
            dst1 = _mm_adds_epi16(dst1, src1);

            /* val += fract_half_offset */
            dst0 = _mm_adds_epi16(dst0, roundingOffsetV);
            dst1 = _mm_adds_epi16(dst1, roundingOffsetV);

            /* val >>= shift */
            dst0 = _mm_srai_epi16(dst0, shift);
            dst1 = _mm_srai_epi16(dst1, shift);

            /* val += sign_offset */
            dst0 = _mm_add_epi16(dst0, signOffsetV);
            dst1 = _mm_add_epi16(dst1, signOffsetV);

            /* clamp to unsigned range */
            dst0 = _mm_max_epi16(_mm_min_epi16(dst0, maxV), minV);
            dst1 = _mm_max_epi16(_mm_min_epi16(dst1, maxV), minV);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, dst0);
            _mm_storeu_si128((__m128i*)dstPixel1, dst1);
        }

        /* Remainder */
        for (; x < width; x += 1, dstPixel0 += 1, srcPixel0 += 1) {
            int32_t pel = uToS(*dstPixel0);
            pel += *srcPixel0;
            *dstPixel0 = sToU(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/*! \brief Performs an additive blit of an S16 input onto a S16 destination in SSE */
static void addS16_SSE(const BlitArgs_t* args)
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
            /* Load 16-pixels */
            __m128i dst0 = _mm_loadu_si128((const __m128i*)dstPixel0);
            __m128i dst1 = _mm_loadu_si128((const __m128i*)dstPixel1);
            const __m128i src0 = _mm_loadu_si128((const __m128i*)srcPixel0);
            const __m128i src1 = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val += src */
            dst0 = _mm_adds_epi16(dst0, src0);
            dst1 = _mm_adds_epi16(dst1, src1);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, dst0);
            _mm_storeu_si128((__m128i*)dstPixel1, dst1);
        }

        /* Remainder */
        for (; x < width; x += 1, dstPixel0 += 1, srcPixel0 += 1) {
            int32_t pel = *dstPixel0;
            pel += *srcPixel0;
            *dstPixel0 = saturateS16(pel);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

static void addU10_SSE(const BlitArgs_t* args) { addUN_SSE(args, 5, 16, 512, 1023, FPU10); }

static void addU12_SSE(const BlitArgs_t* args) { addUN_SSE(args, 3, 4, 2048, 4095, FPU12); }

static void addU14_SSE(const BlitArgs_t* args) { addUN_SSE(args, 1, 1, 8192, 16383, FPU14); }

/*------------------------------------------------------------------------------*/

/* Copy U8 to U16: val << shift  */
static void copyU8_U16_SSE(const BlitArgs_t* args, const int16_t shift)
{
    VN_BLIT_SIMD_BOILERPLATE(uint8_t, uint16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const uint8_t* srcPixel = srcRow;
        uint16_t* dstPixel0 = dstRow;
        uint16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        /* SIMD loop */
        for (; x < simdWidth; x += kStep, srcPixel += kStep, dstPixel0 += kStep, dstPixel1 += kStep) {
            /* Load 16-pixels & split */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel);
            __m128i right = _mm_srli_si128(left, 8);

            /* Convert to uint16_t */
            left = _mm_cvtepu8_epi16(left);
            right = _mm_cvtepu8_epi16(right);

            /* val <<= shift */
            left = _mm_slli_epi16(left, shift);
            right = _mm_slli_epi16(right, shift);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, left);
            _mm_storeu_si128((__m128i*)dstPixel1, right);
        }

        /* Remainder */
        for (; x < width; x++, srcPixel++, dstPixel0++) {
            *dstPixel0 = (uint16_t)(*srcPixel << shift);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/* Copy U8 to S16: (val << 7) - 0x4000 */
static void copyU8_S16_SSE(const BlitArgs_t* args)
{
    static const int16_t kShift = 7;
    const __m128i offset = _mm_set1_epi16(0x4000);

    VN_BLIT_SIMD_BOILERPLATE(uint8_t, int16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const uint8_t* srcPixel = srcRow;
        int16_t* dstPixel0 = dstRow;
        int16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        for (; x < simdWidth; x += kStep, srcPixel += kStep, dstPixel0 += kStep, dstPixel1 += kStep) {
            /* Load 16-pixels & split */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel);
            __m128i right = _mm_srli_si128(left, 8);

            /* Convert to int16_t */
            left = _mm_cvtepu8_epi16(left);
            right = _mm_cvtepu8_epi16(right);

            /* val <<= shift */
            left = _mm_slli_epi16(left, kShift);
            right = _mm_slli_epi16(right, kShift);

            /* val -= sign_offset */
            left = _mm_sub_epi16(left, offset);
            right = _mm_sub_epi16(right, offset);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, left);
            _mm_storeu_si128((__m128i*)dstPixel1, right);
        }

        for (; x < width; x++, srcPixel++, dstPixel0++) {
            *dstPixel0 = fpU16ToS16(*srcPixel, kShift);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/* Copy U16 to S16: (val << shift) - 0x4000 */
static void copyU16_S16_SSE(const BlitArgs_t* args, const int16_t shift)
{
    const __m128i offset = _mm_set1_epi16(0x4000);

    VN_BLIT_SIMD_BOILERPLATE(uint16_t, int16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const uint16_t* srcPixel0 = srcRow;
        const uint16_t* srcPixel1 = srcRow + 8;
        int16_t* dstPixel0 = dstRow;
        int16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        for (; x < simdWidth; x += kStep, srcPixel0 += kStep, srcPixel1 += kStep,
                              dstPixel0 += kStep, dstPixel1 += kStep) {
            /* Load 16-pixels */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel0);
            __m128i right = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val <<= shift */
            left = _mm_slli_epi16(left, shift);
            right = _mm_slli_epi16(right, shift);

            /* val -= signOffset */
            left = _mm_sub_epi16(left, offset);
            right = _mm_sub_epi16(right, offset);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, left);
            _mm_storeu_si128((__m128i*)dstPixel1, right);
        }

        for (; x < width; x++, srcPixel0++, dstPixel0++) {
            *dstPixel0 = fpU16ToS16(*srcPixel0, shift);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/* Copy U16 to U8: val >> shift */
static void copyU16_U8_SSE(const BlitArgs_t* args, const int16_t shift)
{
    VN_BLIT_SIMD_BOILERPLATE(uint16_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const uint16_t* srcPixel0 = srcRow;
        const uint16_t* srcPixel1 = srcRow + 8;
        uint8_t* dstPixel = dstRow;
        uint32_t x = 0;

        for (; x < simdWidth; x += kStep, srcPixel0 += kStep, srcPixel1 += kStep, dstPixel += kStep) {
            /* Load 16-pixels */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel0);
            __m128i right = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val >>= shift */
            left = _mm_srai_epi16(left, shift);
            right = _mm_srai_epi16(right, shift);

            /* clamp & store */
            _mm_storeu_si128((__m128i*)dstPixel, _mm_packus_epi16(left, right));
        }

        for (; x < width; x++, srcPixel0++, dstPixel++) {
            *dstPixel = saturateU8(*srcPixel0 >> shift);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/* Copy S8.7 to U8: ((val + 64) >> 7) + 128 */
static void copyS8_7_U8_SSE(const BlitArgs_t* args)
{
    const __m128i rounding = _mm_set1_epi16(0x40);
    const __m128i offset = _mm_set1_epi16(0x80);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, uint8_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        uint8_t* dstPixel = dstRow;
        uint32_t x = 0;

        for (; x < simdWidth; x += kStep, srcPixel0 += kStep, srcPixel1 += kStep, dstPixel += kStep) {
            /* Load 16-pixels */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel0);
            __m128i right = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val += rounding */
            left = _mm_adds_epi16(left, rounding);
            right = _mm_adds_epi16(right, rounding);

            /* val >>= shift */
            left = _mm_srai_epi16(left, 7);
            right = _mm_srai_epi16(right, 7);

            /* val += signed_offset */
            left = _mm_add_epi16(left, offset);
            right = _mm_add_epi16(right, offset);

            /* clamp & store */
            _mm_storeu_si128((__m128i*)dstPixel, _mm_packus_epi16(left, right));
        }

        for (; x < width; x++, srcPixel0++, dstPixel++) {
            *dstPixel = fpS8ToU8(*srcPixel0);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

/* Copy S16 to U16: clamped(0, maxValue, ((((val + rounding) >> shift) + signed_offset)) */
static void copyS16_U16_SSE(const BlitArgs_t* args, const int16_t shift, const int16_t signOffset,
                            const uint16_t maxValue)
{
    const int16_t roundingValue = (int16_t)(1 << (shift - 1));
    const __m128i rounding = _mm_set1_epi16(roundingValue);
    const __m128i offset = _mm_set1_epi16(signOffset);
    const __m128i minV = _mm_set1_epi16(0);
    const __m128i maxV = _mm_set1_epi16((int16_t)maxValue);

    VN_BLIT_SIMD_BOILERPLATE(int16_t, uint16_t);

    for (uint32_t y = 0; y < args->count; y++) {
        const int16_t* srcPixel0 = srcRow;
        const int16_t* srcPixel1 = srcRow + 8;
        uint16_t* dstPixel0 = dstRow;
        uint16_t* dstPixel1 = dstRow + 8;
        uint32_t x = 0;

        for (; x < simdWidth; x += kStep, srcPixel0 += kStep, srcPixel1 += kStep,
                              dstPixel0 += kStep, dstPixel1 += kStep) {
            /* Load 16-pixels */
            __m128i left = _mm_loadu_si128((const __m128i*)srcPixel0);
            __m128i right = _mm_loadu_si128((const __m128i*)srcPixel1);

            /* val += rounding */
            left = _mm_adds_epi16(left, rounding);
            right = _mm_adds_epi16(right, rounding);

            /* val >>= shift */
            left = _mm_srai_epi16(left, shift);
            right = _mm_srai_epi16(right, shift);

            /* val += signed_offset */
            left = _mm_add_epi16(left, offset);
            right = _mm_add_epi16(right, offset);

            /* clamp */
            left = _mm_max_epi16(_mm_min_epi16(left, maxV), minV);
            right = _mm_max_epi16(_mm_min_epi16(right, maxV), minV);

            /* Store 16-pixels */
            _mm_storeu_si128((__m128i*)dstPixel0, left);
            _mm_storeu_si128((__m128i*)dstPixel1, right);
        }

        for (; x < width; x++, srcPixel0++, dstPixel0++) {
            *dstPixel0 = fpS16ToU16(*srcPixel0, shift, roundingValue, signOffset, maxValue);
        }

        srcRow += src->stride;
        dstRow += dst->stride;
    }
}

static void copyU8_U10_SSE(const BlitArgs_t* args) { copyU8_U16_SSE(args, 2); }

static void copyU8_U12_SSE(const BlitArgs_t* args) { copyU8_U16_SSE(args, 4); }

static void copyU8_U14_SSE(const BlitArgs_t* args) { copyU8_U16_SSE(args, 6); }

static void copyU10_S16_SSE(const BlitArgs_t* args) { copyU16_S16_SSE(args, 5); }

static void copyU12_S16_SSE(const BlitArgs_t* args) { copyU16_S16_SSE(args, 3); }

static void copyU14_S16_SSE(const BlitArgs_t* args) { copyU16_S16_SSE(args, 1); }

static void copyU10_U8_SSE(const BlitArgs_t* args) { copyU16_U8_SSE(args, 2); }

static void copyU12_U8_SSE(const BlitArgs_t* args) { copyU16_U8_SSE(args, 4); }

static void copyU14_U8_SSE(const BlitArgs_t* args) { copyU16_U8_SSE(args, 6); }

static void copyS16_U10_SSE(const BlitArgs_t* args) { copyS16_U16_SSE(args, 5, 0x200, 1023); }

static void copyS16_U12_SSE(const BlitArgs_t* args) { copyS16_U16_SSE(args, 3, 0x800, 4095); }

static void copyS16_U14_SSE(const BlitArgs_t* args) { copyS16_U16_SSE(args, 1, 0x2000, 16383); }

/*------------------------------------------------------------------------------
 * Tables
 *------------------------------------------------------------------------------*/

/* clang-format off */

static const BlitFunction_t kAddTable[FPCount] = {
	&addU8_SSE,  /* FP_U8 */
	&addU10_SSE, /* FP_U10 */
	&addU12_SSE, /* FP_U12 */
	&addU14_SSE, /* FP_U14 */
	&addS16_SSE, /* FP_S8_7 */
	&addS16_SSE, /* FP_S10_5 */
	&addS16_SSE, /* FP_S12_3 */
	&addS16_SSE, /* FP_S14_1 */
};

static const BlitFunction_t kCopyTable[FPCount][FPCount] = {
	/* src/dst   U8                U10               U12               U14               S8.7             S10.5             S12.3             S14.1*/
	/* U8    */ {NULL,             &copyU8_U10_SSE,  &copyU8_U12_SSE,  &copyU8_U14_SSE,  &copyU8_S16_SSE, &copyU8_S16_SSE,  &copyU8_S16_SSE,  &copyU8_S16_SSE},
	/* U10   */ {&copyU10_U8_SSE,  NULL,             NULL,             NULL,             NULL,            &copyU10_S16_SSE, &copyU10_S16_SSE, &copyU10_S16_SSE},
	/* U12   */ {&copyU12_U8_SSE,  NULL,             NULL,             NULL,             NULL,            NULL,             &copyU12_S16_SSE, &copyU12_S16_SSE},
	/* U14   */ {&copyU14_U8_SSE,  NULL,             NULL,             NULL,             NULL,            NULL,             NULL,             &copyU14_S16_SSE},
	/* S8.7  */ {&copyS8_7_U8_SSE, &copyS16_U10_SSE, &copyS16_U12_SSE, &copyS16_U14_SSE, NULL,            NULL,             NULL,             NULL},
	/* S10.5 */ {NULL,             &copyS16_U10_SSE, &copyS16_U12_SSE, &copyS16_U14_SSE, NULL,            NULL,             NULL,             NULL},
	/* S12.3 */ {NULL,             NULL,             &copyS16_U12_SSE, &copyS16_U14_SSE, NULL,            NULL,             NULL,             NULL},
	/* S14.1 */ {NULL,             NULL,             NULL,             &copyS16_U14_SSE, NULL,            NULL,             NULL,             NULL},
};

/* clang-format on */

/*------------------------------------------------------------------------------*/

BlitFunction_t surfaceBlitGetFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending)
{
    if (blending == BMAdd) {
        /* Special case handling for a src of U8 (which is really an S7).
         * @todo(bob): Remove this. */
        if (dstFP == FPU8 && srcFP == FPU8) {
            return &addS7_U8_SSE;
        }

        /* Ensure formats match */
        assert(fixedPointIsValid(dstFP));
        assert(fixedPointHighPrecision(dstFP) == srcFP);

        return kAddTable[dstFP];
    }

    if (blending == BMCopy) {
        return kCopyTable[srcFP][dstFP];
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/

#else /* VN_CORE_FEATURE(SSE) */

BlitFunction_t surfaceBlitGetFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending)
{
    VN_UNUSED(srcFP);
    VN_UNUSED(dstFP);
    VN_UNUSED(blending);

    return NULL;
}

#endif /* VN_CORE_FEATURE(SSE) */
