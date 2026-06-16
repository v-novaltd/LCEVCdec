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

#if !defined(DIMENSION_1D)
#define DIMENSION_1D 1
#endif

#if !defined(DIMENSION_2D)
#define DIMENSION_2D 2
#endif

#if !defined(DIMENSION)
#error "DIMENSION must be defined before including this header"
#endif

#if DIMENSION != DIMENSION_1D && DIMENSION != DIMENSION_2D
#error "DIMENSION must be DIMENSION_1D or DIMENSION_2D"
#endif

#if IN_FIXED_POINT != FP_NV12 || OUT_FIXED_POINT != FP_NV12
#error "sse_nv12_loop.h is for FP_NV12 -> FP_NV12 only"
#endif

{
#if DITHER == 1
#error "Dithering not implemented for NV12 SSE"
#else
    VNUnused(dither);
#endif
    const __m128i kRound = _mm_set1_epi32(0x2000);
    const __m128i kMin16 = _mm_set1_epi16(-0x4000); /* see saturateS15 for explanation */
    const __m128i kMax16 = _mm_set1_epi16(0x4000 - 1);
    const __m128i kMidpointS16 = _mm_set1_epi16(0x4000);

    // Load fixed point conversion constants to registers
    const __m128i kOffset = _mm_set1_epi16(64);
    const __m128i kMidpoint = _mm_set1_epi16(128);

    // Build [UV0 UV0 UV0 UV1] for the left edge.
    const __m128i kStartShuffle =
        _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                     (char)0x80, (char)0x80, 3, 2, 1, 0, 1, 0, 1, 0);
    // Build [UV(w-2) UV(w-1) UV(w-1) UV(w-1)] for the right edge.
    const __m128i kEndShuffle =
        _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                     (char)0x80, (char)0x80, 7, 6, 7, 6, 7, 6, 5, 4);
    // Deinterleave U and V lanes from 8x16-bit UVUV... vectors.
    const __m128i kDeinterleaveU =
        _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                     (char)0x80, (char)0x80, 13, 12, 9, 8, 5, 4, 1, 0);
    const __m128i kDeinterleaveV =
        _mm_set_epi8((char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80, (char)0x80,
                     (char)0x80, (char)0x80, 15, 14, 11, 10, 7, 6, 3, 2);

    // Kernel coefficients broadcast to vectors
#if DIMENSION == DIMENSION_2D
    const __m128i k01 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[1] << 16) | (uint16_t)params.kernel[0]));
    const __m128i k23 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[3] << 16) | (uint16_t)params.kernel[2]));
    const __m128i k32 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[2] << 16) | (uint16_t)params.kernel[3]));
    const __m128i k10 =
        _mm_set1_epi32((int32_t)(((uint16_t)params.kernel[0] << 16) | (uint16_t)params.kernel[1]));
#endif

    const __m128i kHEven =
        _mm_set_epi16(params.kernel[3], params.kernel[2], params.kernel[1], params.kernel[0],
                      params.kernel[3], params.kernel[2], params.kernel[1], params.kernel[0]);
    const __m128i kHOdd =
        _mm_set_epi16(params.kernel[0], params.kernel[1], params.kernel[2], params.kernel[3],
                      params.kernel[0], params.kernel[1], params.kernel[2], params.kernel[3]);

#if DIMENSION == DIMENSION_2D
    const int32_t dstHeight = (int32_t)(srcHeight << 1);

    // Process each row
    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; ++srcY) {
#else
    // Process each pair of rows (1D)
    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; srcY += 2) {
#endif

#if DIMENSION == DIMENSION_2D
        // Get clamped source row indices
        const int32_t srcY0 = clampInt(srcY - 2, 0, (int32_t)srcHeight - 1);
        const int32_t srcY1 = clampInt(srcY - 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY2 = clampInt(srcY - 0, 0, (int32_t)srcHeight - 1);
        const int32_t srcY3 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY4 = clampInt(srcY + 2, 0, (int32_t)srcHeight - 1);

        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);
        const uint8_t* const srcRow2 = getRowU8(src, srcStride, srcY2);
        const uint8_t* const srcRow3 = getRowU8(src, srcStride, srcY3);
        const uint8_t* const srcRow4 = getRowU8(src, srcStride, srcY4);

        // Get clamped dest row indices and pointers
        const int32_t dstY0 = clampInt((srcY * 2) + 0, 0, dstHeight - 1);
        const int32_t dstY1 = clampInt((srcY * 2) + 1, 0, dstHeight - 1);
        uint8_t* const dstRow0 = getRowU8(dst, dstStride, dstY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, dstY1);
#else // Run two rows at a time for 1D
        const int32_t srcY0 = srcY;
        const int32_t srcY1 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);

        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);

        uint8_t* const dstRow0 = getRowU8(dst, dstStride, srcY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, srcY1);
#endif

        // Start: handle left edge using 8-wide kernel (destX = 0, outputs 0-15)
#define KERNEL_VARIANT 1
#include "sse_kernel_nv12.h"

        // Main 8-wide body loop
        for (int32_t srcX = 8; srcX <= ((int32_t)srcWidth) - 8; srcX += 8) {
#define KERNEL_VARIANT 0
#include "sse_kernel_nv12.h"
        }

        // Final 8 input pixels on the right edge
#define KERNEL_VARIANT 2
#include "sse_kernel_nv12.h"
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
