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

// Interleaved (two-channel) SSE4.1 upscaler
// Processes 8 interleaved source samples (4 per channel) per iteration.
// Vertical filter runs on interleaved data; deinterleave is deferred to before the horizontal filter.
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
    assert((srcWidth % 2) == 0);
    assert(srcWidth >= 16);

    // shuffle2 control vectors for the right edge, indexed by overhang (1-4 UV pair columns).
    // Each entry selects from prev (bytes 0-15) and curr (bytes 16-31).
    //
    // When overhang <= 2, the edge writes are moved back 2 UV pairs to recalculate
    // and overwrite the last output block, avoiding corrupting the next row and
    // ensuring overhang==1 gets correct edge extension.
    //
    static const int8_t rightEdgeControlLo[4][16] = {
        /* 1 */ {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        /* 2 */ {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        /* 3 */ {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
        /* 4 */ {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
    };

    static const int8_t rightEdgeControlHi[4][16] = {
        /* 1 */ {16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19},
        /* 2 */ {16, 17, 18, 19, 20, 21, 22, 23, 20, 21, 22, 23, 20, 21, 22, 23},
        /* 3 */ {24, 25, 26, 27, 24, 25, 26, 27, 24, 25, 26, 27, 24, 25, 26, 27},
        /* 4 */ {24, 25, 26, 27, 28, 29, 30, 31, 28, 29, 30, 31, 28, 29, 30, 31},
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
    const __m128i paExtractU = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 13, 12, 9, 8, 5, 4, 1, 0);
    const __m128i paExtractV = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 15, 14, 11, 10, 7, 6, 3, 2);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m128i ditherOffset = _mm_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m128i ditherMul = _mm_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // Edge replication shuffle (applied on vertical filter results, 32-bit UV pair units)
    const __m128i leftEdgeShuf = _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0);

    // Deinterleave shuffle: within 128 bits, move even 16-bit words to low qword, odd to high qword
    const __m128i deintShuf = _mm_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0);

    //
    const int32_t nPairs = srcWidth / 2;
    const int32_t bodyEnd = ((nPairs - 1) & ~3) * 2;
    const int32_t overhang = nPairs - bodyEnd / 2;
    const int32_t rightEdgeOffset = (overhang > 2) ? bodyEnd : (bodyEnd - 4);

    // Lookup the right-edge shuffles depending on overhang
    const __m128i rightEdgeShufLo = _mm_loadu_si128((const __m128i*)rightEdgeControlLo[overhang - 1]);
    const __m128i rightEdgeShufHi = _mm_loadu_si128((const __m128i*)rightEdgeControlHi[overhang - 1]);

    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif
        const SSE_INPUT_TYPE* const srcRow0Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy - 2);
        const SSE_INPUT_TYPE* const srcRow1Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy - 1);
        const SSE_INPUT_TYPE* const srcRow2Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 0);
        const SSE_INPUT_TYPE* const srcRow3Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 1);
        const SSE_INPUT_TYPE* const srcRow4Ptr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy + 2);

        SSE_OUTPUT_TYPE* const destRow0Ptr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 0);
        SSE_OUTPUT_TYPE* const destRow1Ptr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 1);

        // Carry-forward vertical filter results (two full vectors: prev and curr)
        __m128i vfPrevEven, vfPrevOdd, vfCurrEven, vfCurrOdd;

        // Left edge: load two chunks, vertical filter both, apply edge replication on results
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

            // Lo window: left edge replicated from vf0
            const __m128i hEvenLo = _mm_shuffle_epi8(vf0Even, leftEdgeShuf);
            const __m128i hOddLo = _mm_shuffle_epi8(vf0Odd, leftEdgeShuf);

            // Hi window: interior [2..5] UV pairs
            const __m128i hEvenHi = _mm_alignr_epi8(vf8Even, vf0Even, 8);
            const __m128i hOddHi = _mm_alignr_epi8(vf8Odd, vf0Odd, 8);

            // Set up carry forward for body loop
            vfPrevEven = vf0Even;
            vfPrevOdd = vf0Odd;
            vfCurrEven = vf8Even;
            vfCurrOdd = vf8Odd;

            // Deinterleave vertical results to per-channel
            __m128i hEvenU, hEvenV, hOddU, hOddV;
            DEINTERLEAVE(hEvenLo, hEvenHi, hEvenU, hEvenV);
            DEINTERLEAVE(hOddLo, hOddHi, hOddU, hOddV);

            __m128i destRow0U, destRow1U;
            HORIZONTAL_FILTER(hEvenU, destRow0U);
            HORIZONTAL_FILTER(hOddU, destRow1U);

            __m128i destRow0V, destRow1V;
            HORIZONTAL_FILTER(hEvenV, destRow0V);
            HORIZONTAL_FILTER(hOddV, destRow1V);

#if APPLY_PA
            const __m128i base8 = SSE_LOAD_INPUT(srcRow2Ptr + 0);
            const __m128i baseU = _mm_shuffle_epi8(base8, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(base8, paExtractV);
            PREDICTED_AVERAGE(destRow0U, destRow1U, baseU);
            PREDICTED_AVERAGE(destRow0V, destRow1V, baseV);
#endif

            DITHER_ADD(destRow0U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0V, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1V, ditherBuffer, ditherScale);

            SSE_STORE_OUTPUT(destRow0Ptr + 0, _mm_unpacklo_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow0Ptr + 8, _mm_unpackhi_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow1Ptr + 0, _mm_unpacklo_epi16(destRow1U, destRow1V));
            SSE_STORE_OUTPUT(destRow1Ptr + 8, _mm_unpackhi_epi16(destRow1U, destRow1V));
        }

        // Main body loop with alignr window construction
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

            // Construct shifted windows from carried VF results (shift by 2 UV pairs = 8 bytes)
            const __m128i hEvenLo = _mm_alignr_epi8(vfCurrEven, vfPrevEven, 8);
            const __m128i hOddLo = _mm_alignr_epi8(vfCurrOdd, vfPrevOdd, 8);
            const __m128i hEvenHi = _mm_alignr_epi8(vfNextEven, vfCurrEven, 8);
            const __m128i hOddHi = _mm_alignr_epi8(vfNextOdd, vfCurrOdd, 8);

            // Advance carry
            vfPrevEven = vfCurrEven;
            vfPrevOdd = vfCurrOdd;
            vfCurrEven = vfNextEven;
            vfCurrOdd = vfNextOdd;

            // Deinterleave vertical results to per-channel
            __m128i hEvenU, hEvenV, hOddU, hOddV;
            DEINTERLEAVE(hEvenLo, hEvenHi, hEvenU, hEvenV);
            DEINTERLEAVE(hOddLo, hOddHi, hOddU, hOddV);

            __m128i destRow0U, destRow1U;
            HORIZONTAL_FILTER(hEvenU, destRow0U);
            HORIZONTAL_FILTER(hOddU, destRow1U);

            __m128i destRow0V, destRow1V;
            HORIZONTAL_FILTER(hEvenV, destRow0V);
            HORIZONTAL_FILTER(hOddV, destRow1V);

#if APPLY_PA
            const __m128i base8 = SSE_LOAD_INPUT(srcRow2Ptr + sx);
            const __m128i baseU = _mm_shuffle_epi8(base8, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(base8, paExtractV);
            PREDICTED_AVERAGE(destRow0U, destRow1U, baseU);
            PREDICTED_AVERAGE(destRow0V, destRow1V, baseV);
#endif

            DITHER_ADD(destRow0U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0V, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1V, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 0, _mm_unpacklo_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 8, _mm_unpackhi_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 0, _mm_unpacklo_epi16(destRow1U, destRow1V));
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 8, _mm_unpackhi_epi16(destRow1U, destRow1V));
        }

        // Right edge: construct windows from last carried vertical filter results
        {
            const __m128i hEvenLo = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufLo);
            const __m128i hOddLo = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufLo);
            const __m128i hEvenHi = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufHi);
            const __m128i hOddHi = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufHi);

            // Deinterleave vertical results to per-channel
            __m128i hEvenU, hEvenV, hOddU, hOddV;
            DEINTERLEAVE(hEvenLo, hEvenHi, hEvenU, hEvenV);
            DEINTERLEAVE(hOddLo, hOddHi, hOddU, hOddV);

            __m128i destRow0U, destRow1U;
            HORIZONTAL_FILTER(hEvenU, destRow0U);
            HORIZONTAL_FILTER(hOddU, destRow1U);

            __m128i destRow0V, destRow1V;
            HORIZONTAL_FILTER(hEvenV, destRow0V);
            HORIZONTAL_FILTER(hOddV, destRow1V);

#if APPLY_PA
            const __m128i base8 = SSE_LOAD_INPUT(srcRow2Ptr + rightEdgeOffset);
            const __m128i baseU = _mm_shuffle_epi8(base8, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(base8, paExtractV);
            PREDICTED_AVERAGE(destRow0U, destRow1U, baseU);
            PREDICTED_AVERAGE(destRow0V, destRow1V, baseV);
#endif

            DITHER_ADD(destRow0U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0V, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1U, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1V, ditherBuffer, ditherScale);

            const int32_t destX = rightEdgeOffset * 2;
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 0, _mm_unpacklo_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow0Ptr + destX + 8, _mm_unpackhi_epi16(destRow0U, destRow0V));
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 0, _mm_unpacklo_epi16(destRow1U, destRow1V));
            SSE_STORE_OUTPUT(destRow1Ptr + destX + 8, _mm_unpackhi_epi16(destRow1U, destRow1V));
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
