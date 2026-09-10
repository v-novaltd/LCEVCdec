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
// AVX2 optimized 1D upscaler - 16 source sample loop step
//

// Upscale a single plane by doubling width (horizontal only) with a 4 tap filter
//
// AVX2 optimised version - processes 16 source pixels per main loop iteration (32 outputs)
//
// Does not need to handle source widths less than 16
//
// Should exactly match Upscale1DPlaneReferenceS16
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
    // Each entry selects from prev (bytes 0-15) and curr (bytes 16-31).
    //
    static const int8_t rightEdgeControlA[8][16] = {
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

    // 256-bit kernel coefficients and rounding offset. Each 128-bit lane filters
    // one independent four-source-pixel block.
    const __m256i k01_256 =
        _mm256_set1_epi32((int32_t)(((uint16_t)params.kernel[1] << 16) | (uint16_t)params.kernel[0]));
    const __m256i k23_256 =
        _mm256_set1_epi32((int32_t)(((uint16_t)params.kernel[3] << 16) | (uint16_t)params.kernel[2]));
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

    // Horizontal filter tap-pair shuffles, broadcast to both lanes.
    const __m256i hfShufOdd01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11));
    const __m256i hfShufOdd23_256 = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15));
    const __m256i hfShufEven01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 8, 9, 12, 13, 10, 11));
    const __m256i hfShufEven23_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7));

#if APPLY_PA
    const __m256i ones256 = _mm256_set1_epi16(1);
    const __m256i pa1d_ones256 = _mm256_set1_epi32(1);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m256i ditherOffset256 = _mm256_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m256i ditherMul256 = _mm256_set1_epi16((int16_t)(dither->strength * 2u + 1u));
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
        const AVX_INPUT_TYPE* const srcRowPtr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy);
        AVX_OUTPUT_TYPE* const destRowPtr = AVX_OUTPUT_ROW(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        __m128i srcPrev, srcCurr;

        // Left edge block: pair the edge-replicated and interior windows in AVX2
        // lanes, then keep filtering, PA, dither and the store full-width.
        {
            const __m256i src256 = AVX_LOAD_INPUT_256(srcRowPtr);
            const __m128i src0 = _mm256_castsi256_si128(src256);
            const __m128i src8 = _mm256_extracti128_si256(src256, 1);
            const __m128i hA = _mm_shuffle_epi8(src0, leftEdgeShuf);
            const __m128i hB = _mm_alignr_epi8(src8, src0, 4);
            const __m256i h = _mm256_inserti128_si256(_mm256_castsi128_si256(hA), hB, 1);

            __m256i dest;
            HORIZONTAL_FILTER_256(h, dest);

#if APPLY_PA
            if (params.applyPA) {
                const __m128i baseA = AVX_LOAD_BASE4(srcRowPtr + 0);
                const __m128i baseB = AVX_LOAD_BASE4(srcRowPtr + 4);
                const __m256i base = _mm256_inserti128_si256(_mm256_castsi128_si256(baseA), baseB, 1);
                PREDICTED_AVERAGE_1D_256(dest, base);
            }
#endif
            DITHER_ADD_256(dest, ditherBuffer, ditherScale256);
            AVX_STORE_OUTPUT(destRowPtr, dest);

            srcPrev = src0;
            srcCurr = src8;
        }

        // Main 16-wide body loop: each half of the load feeds one full-width
        // filter/PA/dither/store pipeline for eight source pixels.
        int32_t sx = 8;
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-BEGIN avx1d_body" ::: "memory");
#endif
        for (; sx + 16 <= bodyEnd; sx += 16) {
            const __m256i srcNext256 = AVX_LOAD_INPUT_256(srcRowPtr + sx + 8);
            const __m128i srcNext0 = _mm256_castsi256_si128(srcNext256);
            const __m128i srcNext8 = _mm256_extracti128_si256(srcNext256, 1);

            {
                const __m128i hA = _mm_alignr_epi8(srcCurr, srcPrev, 12);
                const __m128i hB = _mm_alignr_epi8(srcNext0, srcCurr, 4);
                const __m256i h = _mm256_inserti128_si256(_mm256_castsi128_si256(hA), hB, 1);
                __m256i dest;
                HORIZONTAL_FILTER_256(h, dest);

#if APPLY_PA
                if (params.applyPA) {
                    const __m128i baseA = AVX_LOAD_BASE4(srcRowPtr + sx);
                    const __m128i baseB = AVX_LOAD_BASE4(srcRowPtr + sx + 4);
                    const __m256i base = _mm256_inserti128_si256(_mm256_castsi128_si256(baseA), baseB, 1);
                    PREDICTED_AVERAGE_1D_256(dest, base);
                }
#endif
                DITHER_ADD_256(dest, ditherBuffer, ditherScale256);
                AVX_STORE_OUTPUT(destRowPtr + sx * 2, dest);
            }

            {
                const __m128i hA = _mm_alignr_epi8(srcNext0, srcCurr, 12);
                const __m128i hB = _mm_alignr_epi8(srcNext8, srcNext0, 4);
                const __m256i h = _mm256_inserti128_si256(_mm256_castsi128_si256(hA), hB, 1);
                __m256i dest;
                HORIZONTAL_FILTER_256(h, dest);

#if APPLY_PA
                if (params.applyPA) {
                    const __m128i baseA = AVX_LOAD_BASE4(srcRowPtr + sx + 8);
                    const __m128i baseB = AVX_LOAD_BASE4(srcRowPtr + sx + 12);
                    const __m256i base = _mm256_inserti128_si256(_mm256_castsi128_si256(baseA), baseB, 1);
                    PREDICTED_AVERAGE_1D_256(dest, base);
                }
#endif
                DITHER_ADD_256(dest, ditherBuffer, ditherScale256);
                AVX_STORE_OUTPUT(destRowPtr + (sx + 8) * 2, dest);
            }

            srcPrev = srcNext0;
            srcCurr = srcNext8;
        }
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-END avx1d_body" ::: "memory");
#endif

        // Drain remaining 8-pixel chunk (when width is not a multiple of 16).
        for (; sx < bodyEnd; sx += 8) {
            const __m128i srcNext = AVX_LOAD_INPUT_128(srcRowPtr + sx + 8);
            const __m128i hA = _mm_alignr_epi8(srcCurr, srcPrev, 12);
            const __m128i hB = _mm_alignr_epi8(srcNext, srcCurr, 4);
            const __m256i h = _mm256_inserti128_si256(_mm256_castsi128_si256(hA), hB, 1);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            __m256i dest;
            HORIZONTAL_FILTER_256(h, dest);
#if APPLY_PA
            if (params.applyPA) {
                const __m128i baseA = AVX_LOAD_BASE4(srcRowPtr + sx);
                const __m128i baseB = AVX_LOAD_BASE4(srcRowPtr + sx + 4);
                const __m256i base = _mm256_inserti128_si256(_mm256_castsi128_si256(baseA), baseB, 1);
                PREDICTED_AVERAGE_1D_256(dest, base);
            }
#endif
            DITHER_ADD_256(dest, ditherBuffer, ditherScale256);
            AVX_STORE_OUTPUT(destRowPtr + sx * 2, dest);
        }

        // Right edge block.
        {
            const __m128i hA = shuffle2(srcPrev, srcCurr, rightEdgeShufA);
            const __m128i hB = shuffle2(srcPrev, srcCurr, rightEdgeShufB);
            const __m256i h = _mm256_inserti128_si256(_mm256_castsi128_si256(hA), hB, 1);

            __m256i dest;
            HORIZONTAL_FILTER_256(h, dest);
#if APPLY_PA
            if (params.applyPA) {
                const __m128i baseA = AVX_LOAD_BASE4(srcRowPtr + rightEdgeOffset);
                const __m128i baseB = AVX_LOAD_BASE4(srcRowPtr + rightEdgeOffset + 4);
                const __m256i base = _mm256_inserti128_si256(_mm256_castsi128_si256(baseA), baseB, 1);
                PREDICTED_AVERAGE_1D_256(dest, base);
            }
#endif
            DITHER_ADD_256(dest, ditherBuffer, ditherScale256);
            AVX_STORE_OUTPUT(destRowPtr + rightEdgeOffset * 2, dest);
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
