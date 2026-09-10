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

// SSE4.1 optimized 1D upscaler - 8 source sample loop step
//

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
    assert(srcWidth >= 16);

    // shuffle2 control vectors that select the inputs for the two horizontal filters
    // on the right edge from carried source vectors. Indexed by how much right
    // edge overhangs the 8-aligned end of the horizontal body loop.
    //
    // If the overhang is less than 4, then the edge writes are moved back to recalculate
    // and overwrite the last 16 dest samples of the body. This handles two cases:
    //
    //   - If the overhang is 1, the last block of the body did not get the correct
    //     extended edge, and gets the wrong answer.
    //
    //   - If the overhang is <= 4, then the destination will only have space for
    //     one 16 byte write - so moving back avoids corrupting the next row.
    //
    static const int8_t rightEdgeControlA[8][16] = {
        // Replicated last sample in first slot
        /* 1 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 16, 17},
        /* 2 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
        /* 3 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
        /* 4 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},

        /* 5 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 24, 25},
        /* 6 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
        /* 7 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
        /* 8 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
    };

    static const int8_t rightEdgeControlB[8][16] = {
        /* 1 */ {12, 13, 14, 15, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17},
        /* 2 */ {12, 13, 14, 15, 16, 17, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19},
        /* 3 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 20, 21, 20, 21, 20, 21},
        /* 4 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 22, 23, 22, 23},

        /* 5 */ {20, 21, 22, 23, 24, 25, 24, 25, 24, 25, 24, 25, 24, 25, 24, 25},
        /* 6 */ {20, 21, 22, 23, 24, 25, 26, 27, 26, 27, 26, 27, 26, 27, 26, 27},
        /* 7 */ {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 28, 29, 28, 29, 28, 29},
        /* 8 */ {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 30, 31, 30, 31},
    };

    // Kernel coefficients as 16-bit pairs for madd
    const __m128i k01 =
        _mm_set_epi16(params.kernel[1], params.kernel[0], params.kernel[1], params.kernel[0],
                      params.kernel[1], params.kernel[0], params.kernel[1], params.kernel[0]);
    const __m128i k23 =
        _mm_set_epi16(params.kernel[3], params.kernel[2], params.kernel[3], params.kernel[2],
                      params.kernel[3], params.kernel[2], params.kernel[3], params.kernel[2]);

    // Rounding offset
    const __m128i round = _mm_set1_epi32(0x2000);

#if IN_FIXED_POINT != FP_S16
    const __m128i inputMidpoint = _mm_set1_epi16(16384);
#endif
#if IN_FIXED_POINT == FP_U16
    const __m128i inputShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif
#if OUT_FIXED_POINT == FP_U8 || OUT_FIXED_POINT == FP_NV12
    const __m128i outputOffset = _mm_set1_epi16(64);
    const __m128i outputMidpoint = _mm_set1_epi16(128);
#elif OUT_FIXED_POINT == FP_U16
    const __m128i outputOffset = _mm_set1_epi16((int16_t)params.offset);
    const __m128i outputMidpoint = _mm_set1_epi16((int16_t)params.midpoint);
    const __m128i outputShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif

    // Horizontal filter tap-pair shuffles (single pshufb builds each madd input)
    const __m128i hfShufOdd01 = _mm_setr_epi8(2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11);
    const __m128i hfShufOdd23 = _mm_setr_epi8(6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15);
    const __m128i hfShufEven01 = _mm_setr_epi8(6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 8, 9, 12, 13, 10, 11);
    const __m128i hfShufEven23 = _mm_setr_epi8(2, 3, 0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7);

#if APPLY_PA
    const __m128i ones = _mm_set1_epi16(1);
    const __m128i pa1d_ones = _mm_set1_epi32(1);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m128i ditherOffset = _mm_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m128i ditherMul = _mm_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // Fixed shuffle pattern to extend left edge of plane
    const __m128i leftEdgeShuf = _mm_setr_epi8(0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);

    const int32_t bodyEnd = (srcWidth - 1) & ~7;
    const int32_t overhang = srcWidth - bodyEnd;
    const int32_t rightEdgeOffset = (overhang > 4) ? bodyEnd : (bodyEnd - 4);

    // Lookup the righthand shuffles depending on overhang
    const __m128i rightEdgeShufA = _mm_loadu_si128((const __m128i*)rightEdgeControlA[overhang - 1]);
    const __m128i rightEdgeShufB = _mm_loadu_si128((const __m128i*)rightEdgeControlB[overhang - 1]);

    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif
        const SSE_INPUT_TYPE* const srcRowPtr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy);
        SSE_OUTPUT_TYPE* const destRowPtr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        __m128i srcPrev, srcCurr;

        // Left edge block: loads src[0..7] and src[8..15], applies left-edge replication
        // and processes source positions 0..7, writing dest[0..15].
        {
            const __m128i src0 = SSE_LOAD_INPUT(srcRowPtr);
            const __m128i src8 = SSE_LOAD_INPUT(srcRowPtr + 8);

            // hA: positions 0..3 — replicate src[0] into phantom src[-2] and src[-1]
            const __m128i hA = _mm_shuffle_epi8(src0, leftEdgeShuf);
            // hB: positions 4..7 — window src[2..9]
            const __m128i hB = _mm_alignr_epi8(src8, src0, 4);

            __m128i destA, destB;
            HORIZONTAL_FILTER(hA, destA);
            HORIZONTAL_FILTER(hB, destB);

#if APPLY_PA
            const __m128i baseA = SSE_LOAD_BASE4(srcRowPtr + 0);
            const __m128i baseB = SSE_LOAD_BASE4(srcRowPtr + 4);
            PREDICTED_AVERAGE_1D(destA, baseA);
            PREDICTED_AVERAGE_1D(destB, baseB);
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            SSE_STORE_OUTPUT(destRowPtr + 0, destA);
            SSE_STORE_OUTPUT(destRowPtr + 8, destB);

            srcPrev = src0;
            srcCurr = src8;
        }

        // Main body loop
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-BEGIN sse1d_body" ::: "memory");
#endif
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            const __m128i srcNext = SSE_LOAD_INPUT(srcRowPtr + sx + 8);

            // hA: positions sx+0..sx+3 — window src[sx-2..sx+5]
            const __m128i hA = _mm_alignr_epi8(srcCurr, srcPrev, 12);
            // hB: positions sx+4..sx+7 — window src[sx+2..sx+9]
            const __m128i hB = _mm_alignr_epi8(srcNext, srcCurr, 4);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            __m128i destA, destB;
            HORIZONTAL_FILTER(hA, destA);
            HORIZONTAL_FILTER(hB, destB);

#if APPLY_PA
            const __m128i baseA = SSE_LOAD_BASE4(srcRowPtr + sx);
            const __m128i baseB = SSE_LOAD_BASE4(srcRowPtr + sx + 4);
            PREDICTED_AVERAGE_1D(destA, baseA);
            PREDICTED_AVERAGE_1D(destB, baseB);
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            SSE_STORE_OUTPUT(destRowPtr + destX + 0, destA);
            SSE_STORE_OUTPUT(destRowPtr + destX + 8, destB);
        }
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-END sse1d_body" ::: "memory");
#endif

        // Right edge block
        {
            const __m128i hA = shuffle2(srcPrev, srcCurr, rightEdgeShufA);
            const __m128i hB = shuffle2(srcPrev, srcCurr, rightEdgeShufB);

            __m128i destA, destB;
            HORIZONTAL_FILTER(hA, destA);
            HORIZONTAL_FILTER(hB, destB);

#if APPLY_PA
            const __m128i baseA = SSE_LOAD_BASE4(srcRowPtr + rightEdgeOffset);
            const __m128i baseB = SSE_LOAD_BASE4(srcRowPtr + rightEdgeOffset + 4);
            PREDICTED_AVERAGE_1D(destA, baseA);
            PREDICTED_AVERAGE_1D(destB, baseB);
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = rightEdgeOffset * 2;
            SSE_STORE_OUTPUT(destRowPtr + destX + 0, destA);
            SSE_STORE_OUTPUT(destRowPtr + destX + 8, destB);
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
