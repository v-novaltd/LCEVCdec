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
// AVX2 optimized upscaler - 16 source sample loop step, 256-bit through the whole pipeline
//
// The 256-bit vertical filter output is regrouped so the even-phase result
// (destination row 0) sits in the low 128-bit lane and the odd-phase result
// (destination row 1) in the high lane. The horizontal filter, predicted
// average and dither then run once per row pair on 256-bit registers, and each
// destination row is written with one format-aware store per 8 source samples
// (32 bytes for S16/U16 and 16 bytes for U8/NV12).
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

    // shuffle2 control vectors for the right edge, indexed by overhang (1-8 samples).
    // Each entry selects from prev (bytes 0-15) and curr (bytes 16-31), and is
    // applied identically to both lanes (even/odd phase) by shuffle2_256.
    //
    // When overhang <= 4, the edge writes are moved back 4 samples to recalculate
    // and overwrite the last output block, avoiding corrupting the next row and
    // ensuring overhang==1 gets correct edge extension.
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

    // Kernel coefficients as 16-bit pairs for madd (128-bit needed by the drain
    // loop's vertical filter)
    const __m128i k01 =
        _mm_set_epi16(params.kernel[1], params.kernel[0], params.kernel[1], params.kernel[0],
                      params.kernel[1], params.kernel[0], params.kernel[1], params.kernel[0]);
    const __m128i k23 =
        _mm_set_epi16(params.kernel[3], params.kernel[2], params.kernel[3], params.kernel[2],
                      params.kernel[3], params.kernel[2], params.kernel[3], params.kernel[2]);

    // 256-bit kernel coefficients
    const __m256i k01_256 =
        _mm256_set1_epi32((int32_t)(((uint16_t)params.kernel[1] << 16) | (uint16_t)params.kernel[0]));
    const __m256i k23_256 =
        _mm256_set1_epi32((int32_t)(((uint16_t)params.kernel[3] << 16) | (uint16_t)params.kernel[2]));

    // Rounding offsets
    const __m128i round = _mm_set1_epi32(0x2000);
    const __m256i round256 = _mm256_set1_epi32(0x2000);

#if IN_FIXED_POINT != FP_S16
    const __m128i inputMidpoint = _mm_set1_epi16(16384);
    const __m256i inputMidpoint256 = _mm256_set1_epi16(16384);
#endif
#if IN_FIXED_POINT == FP_U16
    const __m128i inputShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif
#if OUT_FIXED_POINT == FP_U8 || OUT_FIXED_POINT == FP_NV12
    const __m256i outputOffset256 = _mm256_set1_epi16(64);
    const __m256i outputMidpoint256 = _mm256_set1_epi16(128);
#elif OUT_FIXED_POINT == FP_U16
    const __m256i outputOffset256 = _mm256_set1_epi16((int16_t)params.offset);
    const __m256i outputMidpoint256 = _mm256_set1_epi16((int16_t)params.midpoint);
    const __m128i outputShift = _mm_cvtsi32_si128((int32_t)params.shift);
#endif

    // Horizontal filter tap-pair shuffles (single pshufb builds each madd input)
    const __m256i hfShufOdd01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11));
    const __m256i hfShufOdd23_256 = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15));
    const __m256i hfShufEven01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 8, 9, 12, 13, 10, 11));
    const __m256i hfShufEven23_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7));

#if APPLY_PA
    // PA rounding constants
    const __m256i twos256 = _mm256_set1_epi32(2);
    const __m256i ones256 = _mm256_set1_epi16(1);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m256i ditherOffset256 = _mm256_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m256i ditherMul256 = _mm256_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // Fixed shuffle pattern to extend left edge of plane (same control in both lanes)
    const __m256i leftEdgeShuf256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));

    const int32_t bodyEnd = (srcWidth - 1) & ~7;
    const int32_t overhang = srcWidth - bodyEnd;
    const int32_t rightEdgeOffset = (overhang > 4) ? bodyEnd : (bodyEnd - 4);

    // Stream only when every store is 32-byte aligned and the right edge starts
    // after the body. If the edge moves back, it overlaps the last body block;
    // mixing a cached overwrite with an earlier weakly ordered NT store is unsafe.
#if OUT_FIXED_POINT == FP_U16 || OUT_FIXED_POINT == FP_S16
    const bool ntStores = ((((uintptr_t)dst | dstStride) & 31) == 0) && overhang > 4;
#else
    const bool ntStores = false;
#endif

    // Lookup the righthand shuffles depending on overhang
    const __m256i rightEdgeShufA256 =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)rightEdgeControlA[overhang - 1]));
    const __m256i rightEdgeShufB256 =
        _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)rightEdgeControlB[overhang - 1]));

    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif
        // Clamped source row indices - need 5 rows to align with PA
        const AVX_INPUT_TYPE* const srcRow0Ptr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy - 2);
        const AVX_INPUT_TYPE* const srcRow1Ptr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy - 1);
        const AVX_INPUT_TYPE* const srcRow2Ptr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy + 0);
        const AVX_INPUT_TYPE* const srcRow3Ptr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy + 1);
        const AVX_INPUT_TYPE* const srcRow4Ptr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy + 2);

        // Clamped destination row indices - (0,1), (2,3), (4,5), etc.
        AVX_OUTPUT_TYPE* const destRow0Ptr = AVX_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 0);
        AVX_OUTPUT_TYPE* const destRow1Ptr = AVX_OUTPUT_ROW(dst, dstStride, srcHeight * 2, sy * 2 + 1);

        // Carry-forward vertical filter results, even phase in the low lane, odd
        // phase in the high lane, 8 samples per lane
        __m256i vfPrev, vfCurr;

        // Left edge: 256-bit load, vertical filter, regroup, edge replication on results
        {
            const __m256i srcRow0_256 = AVX_LOAD_INPUT_256(srcRow0Ptr);
            const __m256i srcRow1_256 = AVX_LOAD_INPUT_256(srcRow1Ptr);
            const __m256i srcRow2_256 = AVX_LOAD_INPUT_256(srcRow2Ptr);
            const __m256i srcRow3_256 = AVX_LOAD_INPUT_256(srcRow3Ptr);
            const __m256i srcRow4_256 = AVX_LOAD_INPUT_256(srcRow4Ptr);

            __m256i vf256Even, vf256Odd;
            VERTICAL_FILTER_256(srcRow3_256, srcRow2_256, srcRow1_256, srcRow0_256, vf256Even);
            VERTICAL_FILTER_256(srcRow1_256, srcRow2_256, srcRow3_256, srcRow4_256, vf256Odd);

            // Regroup to [even | odd] per 8 source samples
            const __m256i vf0 = _mm256_permute2x128_si256(vf256Even, vf256Odd, 0x20);
            const __m256i vf8 = _mm256_permute2x128_si256(vf256Even, vf256Odd, 0x31);

            // Block A: left edge - replicate left edge. Block B: interior window [2..9]
            const __m256i hA = _mm256_shuffle_epi8(vf0, leftEdgeShuf256);
            const __m256i hB = _mm256_alignr_epi8(vf8, vf0, 4);

            __m256i destA, destB;
            HORIZONTAL_FILTER_256(hA, destA);
            HORIZONTAL_FILTER_256(hB, destB);

            const __m128i base = AVX_LOAD_BASE8(srcRow2Ptr + 0);
            PREDICTED_AVERAGE_PAIR_256(destA, destB, base);

            // Regroup before dither so the codec buffer is consumed in destination-row order.
            __m256i destRow0 = _mm256_permute2x128_si256(destA, destB, 0x20);
            __m256i destRow1 = _mm256_permute2x128_si256(destA, destB, 0x31);
            DITHER_ADD_256(destRow0, ditherBuffer, ditherScale256);
            DITHER_ADD_256(destRow1, ditherBuffer, ditherScale256);

            AVX_STORE_ROW(destRow0Ptr + 0, destRow0);
            AVX_STORE_ROW(destRow1Ptr + 0, destRow1);

            // Set up carry forward for body loop
            vfPrev = vf0;
            vfCurr = vf8;
        }

        // Main 16-wide body loop: 256-bit vertical and horizontal stages
        int32_t sx = 8;
        for (; sx + 16 <= bodyEnd; sx += 16) {
            // 256-bit loads for next 16 source samples (lookahead by 8)
            const __m256i srcRow0_256 = AVX_LOAD_INPUT_256(srcRow0Ptr + sx + 8);
            const __m256i srcRow1_256 = AVX_LOAD_INPUT_256(srcRow1Ptr + sx + 8);
            const __m256i srcRow2_256 = AVX_LOAD_INPUT_256(srcRow2Ptr + sx + 8);
            const __m256i srcRow3_256 = AVX_LOAD_INPUT_256(srcRow3Ptr + sx + 8);
            const __m256i srcRow4_256 = AVX_LOAD_INPUT_256(srcRow4Ptr + sx + 8);

            // 256-bit vertical filter: processes 16 samples at once
            __m256i vf256Even, vf256Odd;
            VERTICAL_FILTER_256(srcRow3_256, srcRow2_256, srcRow1_256, srcRow0_256, vf256Even);
            VERTICAL_FILTER_256(srcRow1_256, srcRow2_256, srcRow3_256, srcRow4_256, vf256Odd);

            // Regroup to [even | odd] per 8 source samples
            const __m256i vfNextA = _mm256_permute2x128_si256(vf256Even, vf256Odd, 0x20);
            const __m256i vfNextB = _mm256_permute2x128_si256(vf256Even, vf256Odd, 0x31);

            // First 8 output pixels (sx..sx+7)
            {
                const __m256i hA = _mm256_alignr_epi8(vfCurr, vfPrev, 12);
                const __m256i hB = _mm256_alignr_epi8(vfNextA, vfCurr, 4);

                __m256i destA, destB;
                HORIZONTAL_FILTER_256(hA, destA);
                HORIZONTAL_FILTER_256(hB, destB);

                const __m128i base = AVX_LOAD_BASE8(srcRow2Ptr + sx);
                PREDICTED_AVERAGE_PAIR_256(destA, destB, base);

                __m256i destRow0 = _mm256_permute2x128_si256(destA, destB, 0x20);
                __m256i destRow1 = _mm256_permute2x128_si256(destA, destB, 0x31);
                DITHER_ADD_256(destRow0, ditherBuffer, ditherScale256);
                DITHER_ADD_256(destRow1, ditherBuffer, ditherScale256);

                const int32_t destX = sx * 2;
                AVX_STORE_ROW(destRow0Ptr + destX, destRow0);
                AVX_STORE_ROW(destRow1Ptr + destX, destRow1);
            }

            // Second 8 output pixels (sx+8..sx+15)
            {
                const __m256i hA = _mm256_alignr_epi8(vfNextA, vfCurr, 12);
                const __m256i hB = _mm256_alignr_epi8(vfNextB, vfNextA, 4);

                __m256i destA, destB;
                HORIZONTAL_FILTER_256(hA, destA);
                HORIZONTAL_FILTER_256(hB, destB);

                const __m128i base = AVX_LOAD_BASE8(srcRow2Ptr + sx + 8);
                PREDICTED_AVERAGE_PAIR_256(destA, destB, base);

                __m256i destRow0 = _mm256_permute2x128_si256(destA, destB, 0x20);
                __m256i destRow1 = _mm256_permute2x128_si256(destA, destB, 0x31);
                DITHER_ADD_256(destRow0, ditherBuffer, ditherScale256);
                DITHER_ADD_256(destRow1, ditherBuffer, ditherScale256);

                const int32_t destX = (sx + 8) * 2;
                AVX_STORE_ROW(destRow0Ptr + destX, destRow0);
                AVX_STORE_ROW(destRow1Ptr + destX, destRow1);
            }

            // Update carry forward
            vfPrev = vfNextA;
            vfCurr = vfNextB;
        }

        // Drain remaining 8-pixel chunk (when width not a multiple of 16)
        for (; sx < bodyEnd; sx += 8) {
            const __m128i srcRow0 = AVX_LOAD_INPUT_128(srcRow0Ptr + sx + 8);
            const __m128i srcRow1 = AVX_LOAD_INPUT_128(srcRow1Ptr + sx + 8);
            const __m128i srcRow2 = AVX_LOAD_INPUT_128(srcRow2Ptr + sx + 8);
            const __m128i srcRow3 = AVX_LOAD_INPUT_128(srcRow3Ptr + sx + 8);
            const __m128i srcRow4 = AVX_LOAD_INPUT_128(srcRow4Ptr + sx + 8);

            __m128i vfNextEven, vfNextOdd;
            VERTICAL_FILTER(srcRow3, srcRow2, srcRow1, srcRow0, vfNextEven);
            VERTICAL_FILTER(srcRow1, srcRow2, srcRow3, srcRow4, vfNextOdd);
            const __m256i vfNext =
                _mm256_inserti128_si256(_mm256_castsi128_si256(vfNextEven), vfNextOdd, 1);

            const __m256i hA = _mm256_alignr_epi8(vfCurr, vfPrev, 12);
            const __m256i hB = _mm256_alignr_epi8(vfNext, vfCurr, 4);

            vfPrev = vfCurr;
            vfCurr = vfNext;

            __m256i destA, destB;
            HORIZONTAL_FILTER_256(hA, destA);
            HORIZONTAL_FILTER_256(hB, destB);

            const __m128i base = AVX_LOAD_BASE8(srcRow2Ptr + sx);
            PREDICTED_AVERAGE_PAIR_256(destA, destB, base);

            __m256i destRow0 = _mm256_permute2x128_si256(destA, destB, 0x20);
            __m256i destRow1 = _mm256_permute2x128_si256(destA, destB, 0x31);
            DITHER_ADD_256(destRow0, ditherBuffer, ditherScale256);
            DITHER_ADD_256(destRow1, ditherBuffer, ditherScale256);

            const int32_t destX = sx * 2;
            AVX_STORE_ROW(destRow0Ptr + destX, destRow0);
            AVX_STORE_ROW(destRow1Ptr + destX, destRow1);
        }

        // Right edge: construct windows from last carried vertical filter results
        {
            const __m256i hA = shuffle2_256(vfPrev, vfCurr, rightEdgeShufA256);
            const __m256i hB = shuffle2_256(vfPrev, vfCurr, rightEdgeShufB256);

            __m256i destA, destB;
            HORIZONTAL_FILTER_256(hA, destA);
            HORIZONTAL_FILTER_256(hB, destB);

            const __m128i base = AVX_LOAD_BASE8(srcRow2Ptr + rightEdgeOffset);
            PREDICTED_AVERAGE_PAIR_256(destA, destB, base);

            __m256i destRow0 = _mm256_permute2x128_si256(destA, destB, 0x20);
            __m256i destRow1 = _mm256_permute2x128_si256(destA, destB, 0x31);
            DITHER_ADD_256(destRow0, ditherBuffer, ditherScale256);
            DITHER_ADD_256(destRow1, ditherBuffer, ditherScale256);

            // Overlapping edge layouts disable NT stores for the whole call. Otherwise
            // this edge is aligned and remains in the same NT stream as the body.
            const int32_t destX = rightEdgeOffset * 2;
            AVX_STORE_ROW(destRow0Ptr + destX, destRow0);
            AVX_STORE_ROW(destRow1Ptr + destX, destRow1);
        }
    }

    if (ntStores) {
        _mm_sfence();
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
