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

#if defined(VN_UPSCALE_LOOP)
#define IN_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT(VN_UPSCALE_LOOP)
#define OUT_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT(VN_UPSCALE_LOOP)
#define APPLY_PA VN_UPSCALE_LOOP_CONFIG_APPLY_PA(VN_UPSCALE_LOOP)
#define DITHER VN_UPSCALE_LOOP_CONFIG_DITHER(VN_UPSCALE_LOOP)
#endif

#if !defined(IN_FIXED_POINT) || !defined(OUT_FIXED_POINT) || !defined(APPLY_PA) || !defined(DITHER)
#error "Define VN_UPSCALE_LOOP before including this header"
#endif
{
    const __m128i kRound = _mm_set1_epi32(0x2000);
    const __m128i kMin16 = _mm_set1_epi16(-0x4000);
    const __m128i kMax16 = _mm_set1_epi16(0x4000 - 1);

#if IN_FIXED_POINT == FP_U8
    const __m128i kMidpointS16 = _mm_set1_epi16(0x4000);
#elif IN_FIXED_POINT == FP_U16
    const __m128i kMidpointS16 = _mm_set1_epi16(0x4000);
    const __m128i kLeftShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif

    // Load fixed point conversion constants to registers
#if OUT_FIXED_POINT == FP_U8
    const __m128i kOffset = _mm_set1_epi16(64);
    const __m128i kMidpoint = _mm_set1_epi16(128);
#elif OUT_FIXED_POINT == FP_U16
    const __m128i kOffset = _mm_set1_epi16((int16_t)params.offset);
    const __m128i kMidpoint = _mm_set1_epi16((int16_t)params.midpoint);
    const __m128i kRightShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif

#if APPLY_PA == 1
    const __m128i kPAOnes = _mm_set1_epi16(1);
    const __m128i kPATwos = _mm_set1_epi32(2);
#endif

    const int32_t dstHeight = (int32_t)(srcHeight << 1);

    const __m128i kLeftEdgeShuffle = _mm_set_epi8(11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 0, 1, 0);
    const __m128i kRightEdgeShuffle =
        _mm_set_epi8(15, 14, 15, 14, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4);

    // Kernel coefficients broadcast to vectors
    const __m128i k01 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[1] << 16) | (uint16_t)params.kernel[0]));
    const __m128i k23 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[3] << 16) | (uint16_t)params.kernel[2]));
    const __m128i k32 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[2] << 16) | (uint16_t)params.kernel[3]));
    const __m128i k10 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[0] << 16) | (uint16_t)params.kernel[1]));

    const __m128i kHEven =
        _mm_set_epi16(params.kernel[3], params.kernel[2], params.kernel[1], params.kernel[0],
                      params.kernel[3], params.kernel[2], params.kernel[1], params.kernel[0]);
    const __m128i kHOdd =
        _mm_set_epi16(params.kernel[0], params.kernel[1], params.kernel[2], params.kernel[3],
                      params.kernel[0], params.kernel[1], params.kernel[2], params.kernel[3]);

    // Body end for 4-wide kernel (used for edge handling)
    const int32_t bodyEnd = (int32_t)srcWidth - 6;

    // Process each row
    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; ++srcY) {
#if DITHER == 1
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
        const __m128i ditherOffset = _mm_set1_epi16((int16_t)dither->strength);
        const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift); // Shift when < 16bit
        const __m128i ditherMul = _mm_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#else
        VNUnused(dither);
#endif

        // Get clamped source row indices
        const int32_t srcY0 = clampInt(srcY - 2, 0, (int32_t)srcHeight - 1);
        const int32_t srcY1 = clampInt(srcY - 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY2 = clampInt(srcY - 0, 0, (int32_t)srcHeight - 1);
        const int32_t srcY3 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY4 = clampInt(srcY + 2, 0, (int32_t)srcHeight - 1);

#if IN_FIXED_POINT == FP_U8
        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);
        const uint8_t* const srcRow2 = getRowU8(src, srcStride, srcY2);
        const uint8_t* const srcRow3 = getRowU8(src, srcStride, srcY3);
        const uint8_t* const srcRow4 = getRowU8(src, srcStride, srcY4);
#else // N16
        const int16_t* const srcRow0 = getRowS16(src, srcStride, srcY0);
        const int16_t* const srcRow1 = getRowS16(src, srcStride, srcY1);
        const int16_t* const srcRow2 = getRowS16(src, srcStride, srcY2);
        const int16_t* const srcRow3 = getRowS16(src, srcStride, srcY3);
        const int16_t* const srcRow4 = getRowS16(src, srcStride, srcY4);
#endif

        // Get clamped dest row indices and pointers
        const int32_t dstY0 = clampInt((srcY * 2) + 0, 0, dstHeight - 1);
        const int32_t dstY1 = clampInt((srcY * 2) + 1, 0, dstHeight - 1);

#if OUT_FIXED_POINT == FP_U8
        uint8_t* const dstRow0 = getRowU8(dst, dstStride, dstY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, dstY1);
#else // N16
        int16_t* const dstRow0 = getRowS16(dst, dstStride, dstY0);
        int16_t* const dstRow1 = getRowS16(dst, dstStride, dstY1);
#endif

        // Start: handle left edge using 4-wide kernel (destX = 0, outputs 0-7)
#define KERNEL_VARIANT 1
#include "sse_2d_kernel_edge.h"

        // Main 8-wide body loop (outputs 8 to bodyEnd8*2+15)
        for (int32_t srcX = 4; srcX < bodyEnd; srcX += 8) {
#include "sse_2d_kernel_middle.h"
        }

        // Cover any remaining columns between 8-wide body and right edge
        // Run 4-wide body up to srcWidth-8 (may overlap with the right edge block).
        if ((srcWidth & 15) != 0) {
            int32_t srcX = bodyEnd;
#define KERNEL_VARIANT 0
#include "sse_2d_kernel_edge.h"
        }

        // Final 4 input pixels on the right edge
#define KERNEL_VARIANT 2
#include "sse_2d_kernel_edge.h"
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
