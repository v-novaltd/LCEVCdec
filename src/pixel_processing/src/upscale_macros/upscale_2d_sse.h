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

//
// SSE4.1 optimized upscaler - 8 source sample loop step
//

// Upscale a single plane by doubling with a 4 tap filter applied vertically then horizontally
//
// Destination plane dimensions need to be twice the input plane dimensions
//
// SSE4.1 Optimised version - processes 8 source pixels per iteration (16 outputs)
//
// Does not need to handle source widths less than 16
//
// Should exactly match UpscalePlaneReferenceS16
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
    // on the right edge from carried vertical filtered vectors. Indexed by how much right
    // edge overhangs the 16 byte aligned end of the horizontal body loop.
    //
    // If the overhang is less than 4, then the edge writes are moved back to recalculate
    // and overwrite the last 16 bytes of the body. This kills two birds with one stone:
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
    // PA rounding constant
    const __m128i twos = _mm_set1_epi32(2);
    const __m128i ones = _mm_set1_epi16(1);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m128i ditherOffset = _mm_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m128i ditherMul = _mm_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // Fixed shuffle pattern to extend left edge of plane - replicates left into first two samples,
    // and aligns the rest.
    const __m128i leftEdgeShuf = _mm_setr_epi8(0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);

    const int32_t bodyEnd = (srcWidth - 1) & ~7;
    const int32_t overhang = srcWidth - bodyEnd;
    // Move back if overhang is <= 4
    const int32_t rightEdgeOffset = (overhang > 4) ? bodyEnd : (bodyEnd - 4);

    // Lookup the righthand shuffles depending on overhang
    const __m128i rightEdgeShufA = _mm_loadu_si128((const __m128i*)rightEdgeControlA[overhang - 1]);
    const __m128i rightEdgeShufB = _mm_loadu_si128((const __m128i*)rightEdgeControlB[overhang - 1]);

    // Process each row pair (0,1), (2,3), (4,5), etc.
    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif

        const SSE_INPUT_TYPE* const srcRow0Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy - 2);
        const SSE_INPUT_TYPE* const srcRow1Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy - 1);
        const SSE_INPUT_TYPE* const srcRow2Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 0);
        const SSE_INPUT_TYPE* const srcRow3Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 1);
        const SSE_INPUT_TYPE* const srcRow4Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 2);

        // Clamped destination row indices - (0,1), (2,3), (4,5), etc.
        SSE_OUTPUT_TYPE* const destRow0Ptr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 0);
        SSE_OUTPUT_TYPE* const destRow1Ptr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 1);

        // Carry-forward vertical filter results (two full vectors: prev and curr)
        __m128i vfPrevEven, vfPrevOdd, vfCurrEven, vfCurrOdd;

        // Start of horizontal loop - load left hand edge, and use leftEdgeShuf to
        // deal with alignment and edge extension. Sets up initial Prev/Curr values
        // for body of main loop.
        {
            const __m128i srcRow0_0 = SSE_LOAD_INPUT(srcRow0Ptr);
            const __m128i srcRow1_0 = SSE_LOAD_INPUT(srcRow1Ptr);
            const __m128i srcRow2_0 = SSE_LOAD_INPUT(srcRow2Ptr);
            const __m128i srcRow3_0 = SSE_LOAD_INPUT(srcRow3Ptr);
            const __m128i srcRow4_0 = SSE_LOAD_INPUT(srcRow4Ptr);

            __m128i vf0Even, vf0Odd;
            VERTICAL_FILTER(srcRow3_0, srcRow2_0, srcRow1_0, srcRow0_0, vf0Even);
            VERTICAL_FILTER(srcRow1_0, srcRow2_0, srcRow3_0, srcRow4_0, vf0Odd);

            const __m128i srcRow0_8 = SSE_LOAD_INPUT(srcRow0Ptr + 8);
            const __m128i srcRow1_8 = SSE_LOAD_INPUT(srcRow1Ptr + 8);
            const __m128i srcRow2_8 = SSE_LOAD_INPUT(srcRow2Ptr + 8);
            const __m128i srcRow3_8 = SSE_LOAD_INPUT(srcRow3Ptr + 8);
            const __m128i srcRow4_8 = SSE_LOAD_INPUT(srcRow4Ptr + 8);

            __m128i vf8Even, vf8Odd;
            VERTICAL_FILTER(srcRow3_8, srcRow2_8, srcRow1_8, srcRow0_8, vf8Even);
            VERTICAL_FILTER(srcRow1_8, srcRow2_8, srcRow3_8, srcRow4_8, vf8Odd);

            // Block A: left edge — replicate left edge
            const __m128i hEvenA = _mm_shuffle_epi8(vf0Even, leftEdgeShuf);
            const __m128i hOddA = _mm_shuffle_epi8(vf0Odd, leftEdgeShuf);

            // Block B: interior window [2..9]
            const __m128i hEvenB = _mm_alignr_epi8(vf8Even, vf0Even, 4);
            const __m128i hOddB = _mm_alignr_epi8(vf8Odd, vf0Odd, 4);

            __m128i destRow0A, destRow1A, destRow0B, destRow1B;
            HORIZONTAL_FILTER(hEvenA, destRow0A);
            HORIZONTAL_FILTER(hOddA, destRow1A);
            HORIZONTAL_FILTER(hEvenB, destRow0B);
            HORIZONTAL_FILTER(hOddB, destRow1B);

#if APPLY_PA
            const __m128i base = SSE_LOAD_INPUT(srcRow2Ptr + 0);
            PREDICTED_AVERAGE_PAIR(destRow0A, destRow1A, destRow0B, destRow1B, base);
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            // Store
            SSE_STORE_OUTPUT(destRow0Ptr + 0, destRow0A);
            SSE_STORE_OUTPUT(destRow0Ptr + 8, destRow0B);
            SSE_STORE_OUTPUT(destRow1Ptr + 0, destRow1A);
            SSE_STORE_OUTPUT(destRow1Ptr + 8, destRow1B);

            // Set up carry forward for body loop
            vfPrevEven = vf0Even;
            vfPrevOdd = vf0Odd;
            vfCurrEven = vf8Even;
            vfCurrOdd = vf8Odd;
        }

        // Main 8-wide body loop with aligned loads and alignr window construction
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            // Load next aligned chunk of source samples
            const __m128i srcRow0 = SSE_LOAD_INPUT(srcRow0Ptr + sx + 8);
            const __m128i srcRow1 = SSE_LOAD_INPUT(srcRow1Ptr + sx + 8);
            const __m128i srcRow2 = SSE_LOAD_INPUT(srcRow2Ptr + sx + 8);
            const __m128i srcRow3 = SSE_LOAD_INPUT(srcRow3Ptr + sx + 8);
            const __m128i srcRow4 = SSE_LOAD_INPUT(srcRow4Ptr + sx + 8);

            __m128i vfNextEven, vfNextOdd;
            VERTICAL_FILTER(srcRow3, srcRow2, srcRow1, srcRow0, vfNextEven);
            VERTICAL_FILTER(srcRow1, srcRow2, srcRow3, srcRow4, vfNextOdd);

            // Extract the horizontal filter input windows from previous, and a little bit of next block
            const __m128i hEvenA = _mm_alignr_epi8(vfCurrEven, vfPrevEven, 12);
            const __m128i hOddA = _mm_alignr_epi8(vfCurrOdd, vfPrevOdd, 12);
            const __m128i hEvenB = _mm_alignr_epi8(vfNextEven, vfCurrEven, 4);
            const __m128i hOddB = _mm_alignr_epi8(vfNextOdd, vfCurrOdd, 4);

            // Move carry forward on
            vfPrevEven = vfCurrEven;
            vfPrevOdd = vfCurrOdd;
            vfCurrEven = vfNextEven;
            vfCurrOdd = vfNextOdd;

            __m128i destRow0A, destRow1A, destRow0B, destRow1B;
            HORIZONTAL_FILTER(hEvenA, destRow0A);
            HORIZONTAL_FILTER(hOddA, destRow1A);
            HORIZONTAL_FILTER(hEvenB, destRow0B);
            HORIZONTAL_FILTER(hOddB, destRow1B);

#if APPLY_PA
            const __m128i base = SSE_LOAD_INPUT(srcRow2Ptr + sx);
            PREDICTED_AVERAGE_PAIR(destRow0A, destRow1A, destRow0B, destRow1B, base);
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            // Store
            const int32_t destX = sx * 2;
            SSE_STORE_OUTPUT(destRow0Ptr + destX, destRow0A);
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 8, destRow0B);
            SSE_STORE_OUTPUT(destRow1Ptr + destX, destRow1A);
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 8, destRow1B);
        }
        // End of loop
        {
            // Construct the two horizontal filter input windows from last carry forward
            const __m128i hEvenA = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufA);
            const __m128i hOddA = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufA);
            const __m128i hEvenB = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufB);
            const __m128i hOddB = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufB);

            __m128i destRow0A, destRow1A, destRow0B, destRow1B;
            HORIZONTAL_FILTER(hEvenA, destRow0A);
            HORIZONTAL_FILTER(hOddA, destRow1A);
            HORIZONTAL_FILTER(hEvenB, destRow0B);
            HORIZONTAL_FILTER(hOddB, destRow1B);

#if APPLY_PA
            const __m128i base = SSE_LOAD_INPUT(srcRow2Ptr + rightEdgeOffset);
            PREDICTED_AVERAGE_PAIR(destRow0A, destRow1A, destRow0B, destRow1B, base);
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            // Store (which may overlap last 16 bytes of body when overhang==1)
            const int32_t destX = rightEdgeOffset * 2;
            SSE_STORE_OUTPUT(destRow0Ptr + destX, destRow0A);
            SSE_STORE_OUTPUT(destRow1Ptr + destX, destRow1A);
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 8, destRow0B);
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 8, destRow1B);
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
