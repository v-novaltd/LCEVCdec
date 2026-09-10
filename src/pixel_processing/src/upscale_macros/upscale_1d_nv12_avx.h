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

// AVX2 optimized 1D interleaved (NV12/UV) upscaler - doubles width with a 4-tap horizontal filter
//
// 16-wide body loop: U and V remain in separate AVX2 lanes through the
// horizontal filter, PA and dither, then re-interleave for one format-aware store.
//

// Upscale a single interleaved UV plane by doubling width with a 4-tap horizontal filter
//
// Source is interleaved UV: [U0,V0, U1,V1, ...]. Each channel is filtered independently
// using same-channel neighbours with edge replication at boundaries.
//
// Destination plane dimensions need to be twice the width of source plane dimensions.
//
// AVX2 optimised version - processes 8 source UV pairs per main loop iteration (32 outputs).
//
// Does not need to handle source widths less than 16.
//
// Should exactly match Upscale1DPlaneInterleavedReferenceS16.
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

    // 256-bit filter constants. U occupies the low lane and V the high lane.
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

    const __m256i hfShufOdd01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11));
    const __m256i hfShufOdd23_256 = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15));
    const __m256i hfShufEven01_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 8, 9, 12, 13, 10, 11));
    const __m256i hfShufEven23_256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(2, 3, 0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7));

    // Edge replication shuffle (32-bit UV pair units): [pair0, pair0, pair0, pair1]
    const __m128i leftEdgeShuf = _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0);

    // Deinterleave shuffle: within each 128-bit lane, move even 16-bit words to low qword, odd to high qword
    const __m256i deintShuf = _mm256_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0,
                                              15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0);

    // Right edge: replicate last UV pair (bytes 12-15 of srcHi) into positions beyond srcW
    const __m128i rightEdgeShuf =
        _mm_setr_epi8(8, 9, 10, 11, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15);

#if APPLY_PA
    const __m256i ones256 = _mm256_set1_epi16(1);
    const __m256i pa1d_ones256 = _mm256_set1_epi32(1);
    const __m256i paExtractUV = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 13, 12, 9, 8, 5, 4, 1, 0)),
        _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 15, 14, 11, 10, 7, 6, 3, 2), 1);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m256i ditherOffset256 = _mm256_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m256i ditherMul256 = _mm256_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // [U0..U7 | V0..V7] -> [U0,V0,U1,V1,...] after qword regrouping.
    const __m256i interleaveShuf256 =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15));

    // bodyEnd: largest multiple of 8 (int16 elements = 4 UV pairs) strictly less than srcW
    const int32_t bodyEnd = ((srcWidth / 2 - 1) & ~3) * 2;

    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif
        const AVX_INPUT_TYPE* const srcRowPtr = AVX_INPUT_ROW(src, srcStride, srcHeight, sy);
        AVX_OUTPUT_TYPE* const destRowPtr = AVX_OUTPUT_ROW(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        __m128i srcPrev, srcCurr;

        // Left edge block: 256-bit load of src[0..15], split into halves,
        // apply left-edge pair replication for pairs 0..3, writing dest[0..15].
        {
            const __m256i src256 = AVX_LOAD_INPUT_256(srcRowPtr);
            const __m128i src0 = _mm256_castsi256_si128(src256);
            const __m128i src8 = _mm256_extracti128_si256(src256, 1);

            // hLo: [pair(-2), pair(-1), pair(0), pair(1)] — pair(0) replicated into phantoms
            const __m128i hLo = _mm_shuffle_epi8(src0, leftEdgeShuf);
            // hHi: [pair(2), pair(3), pair(4), pair(5)]
            const __m128i hHi = _mm_alignr_epi8(src8, src0, 8);

            // Combine into 256-bit for deinterleave
            const __m256i h256 = _mm256_inserti128_si256(_mm256_castsi128_si256(hLo), hHi, 1);

            __m256i hUV;
            DEINTERLEAVE_256(h256, hUV);
            __m256i destUV;
            HORIZONTAL_FILTER_256(hUV, destUV);

#if APPLY_PA
            if (params.applyPA) {
                const __m256i base8 = _mm256_broadcastsi128_si256(src0);
                const __m256i baseUV = _mm256_shuffle_epi8(base8, paExtractUV);
                PREDICTED_AVERAGE_1D_256(destUV, baseUV);
            }
#endif

            DITHER_ADD_256(destUV, ditherBuffer, ditherScale256);
            const __m256i inter = _mm256_shuffle_epi8(
                _mm256_permute4x64_epi64(destUV, _MM_SHUFFLE(3, 1, 2, 0)), interleaveShuf256);
            AVX_STORE_OUTPUT(destRowPtr, inter);

            srcPrev = src0;
            srcCurr = src8;
        }

        // Main 16-wide body loop: process two groups of 4 UV pairs.
        int32_t sx = 8;
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-BEGIN avx1d_ilv_body" ::: "memory");
#endif
        for (; sx + 16 <= bodyEnd; sx += 16) {
            const __m256i srcNext256 = AVX_LOAD_INPUT_256(srcRowPtr + sx + 8);
            const __m128i srcNext0 = _mm256_castsi256_si128(srcNext256);
            const __m128i srcNext8 = _mm256_extracti128_si256(srcNext256, 1);

            // First 4 UV pairs (sx..sx+7)
            {
                const __m128i hLo = _mm_alignr_epi8(srcCurr, srcPrev, 8);
                const __m128i hHi = _mm_alignr_epi8(srcNext0, srcCurr, 8);

                const __m256i h256 = _mm256_inserti128_si256(_mm256_castsi128_si256(hLo), hHi, 1);

                __m256i hUV;
                DEINTERLEAVE_256(h256, hUV);
                __m256i destUV;
                HORIZONTAL_FILTER_256(hUV, destUV);

#if APPLY_PA
                if (params.applyPA) {
                    const __m256i base8 = _mm256_broadcastsi128_si256(srcCurr);
                    const __m256i baseUV = _mm256_shuffle_epi8(base8, paExtractUV);
                    PREDICTED_AVERAGE_1D_256(destUV, baseUV);
                }
#endif

                DITHER_ADD_256(destUV, ditherBuffer, ditherScale256);
                const __m256i inter = _mm256_shuffle_epi8(
                    _mm256_permute4x64_epi64(destUV, _MM_SHUFFLE(3, 1, 2, 0)), interleaveShuf256);
                AVX_STORE_OUTPUT(destRowPtr + sx * 2, inter);
            }

            // Second 4 UV pairs (sx+8..sx+15)
            {
                const __m128i hLo = _mm_alignr_epi8(srcNext0, srcCurr, 8);
                const __m128i hHi = _mm_alignr_epi8(srcNext8, srcNext0, 8);

                const __m256i h256 = _mm256_inserti128_si256(_mm256_castsi128_si256(hLo), hHi, 1);

                __m256i hUV;
                DEINTERLEAVE_256(h256, hUV);
                __m256i destUV;
                HORIZONTAL_FILTER_256(hUV, destUV);

#if APPLY_PA
                if (params.applyPA) {
                    const __m256i base8 = _mm256_broadcastsi128_si256(srcNext0);
                    const __m256i baseUV = _mm256_shuffle_epi8(base8, paExtractUV);
                    PREDICTED_AVERAGE_1D_256(destUV, baseUV);
                }
#endif

                DITHER_ADD_256(destUV, ditherBuffer, ditherScale256);
                const __m256i inter = _mm256_shuffle_epi8(
                    _mm256_permute4x64_epi64(destUV, _MM_SHUFFLE(3, 1, 2, 0)), interleaveShuf256);
                AVX_STORE_OUTPUT(destRowPtr + (sx + 8) * 2, inter);
            }

            // Update carry forward
            srcPrev = srcNext0;
            srcCurr = srcNext8;
        }
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-END avx1d_ilv_body" ::: "memory");
#endif

        // Drain remaining 8-sample chunk (when width not a multiple of 8 UV pairs)
        for (; sx < bodyEnd; sx += 8) {
            const __m128i srcNext = AVX_LOAD_INPUT_128(srcRowPtr + sx + 8);

#if APPLY_PA
            const __m128i currSrc = srcCurr;
#endif

            const __m128i hLo = _mm_alignr_epi8(srcCurr, srcPrev, 8);
            const __m128i hHi = _mm_alignr_epi8(srcNext, srcCurr, 8);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            const __m256i h256 = _mm256_inserti128_si256(_mm256_castsi128_si256(hLo), hHi, 1);

            __m256i hUV;
            DEINTERLEAVE_256(h256, hUV);
            __m256i destUV;
            HORIZONTAL_FILTER_256(hUV, destUV);

#if APPLY_PA
            if (params.applyPA) {
                const __m256i base8 = _mm256_broadcastsi128_si256(currSrc);
                const __m256i baseUV = _mm256_shuffle_epi8(base8, paExtractUV);
                PREDICTED_AVERAGE_1D_256(destUV, baseUV);
            }
#endif

            DITHER_ADD_256(destUV, ditherBuffer, ditherScale256);
            const __m256i inter = _mm256_shuffle_epi8(
                _mm256_permute4x64_epi64(destUV, _MM_SHUFFLE(3, 1, 2, 0)), interleaveShuf256);
            AVX_STORE_OUTPUT(destRowPtr + sx * 2, inter);
        }

        // Right edge block: fresh loads from srcW-16 and srcW-8, applies right-edge pair
        // replication, and writes at (srcW-8)*2. May overlap and overwrite the last body
        // write when the overhang is small, replacing it with the correctly edge-clamped value.
        {
            const __m128i srcLo = AVX_LOAD_INPUT_128(srcRowPtr + srcWidth - 16);
            const __m128i srcHi = AVX_LOAD_INPUT_128(srcRowPtr + srcWidth - 8);

            // hLo: 4 pairs ending at pair(nP-3), bridging the two loads
            const __m128i hLo = _mm_alignr_epi8(srcHi, srcLo, 8);
            // hHi: last pair replicated into phantom positions
            const __m128i hHi = _mm_shuffle_epi8(srcHi, rightEdgeShuf);

            const __m256i h256 = _mm256_inserti128_si256(_mm256_castsi128_si256(hLo), hHi, 1);

            __m256i hUV;
            DEINTERLEAVE_256(h256, hUV);
            __m256i destUV;
            HORIZONTAL_FILTER_256(hUV, destUV);

#if APPLY_PA
            if (params.applyPA) {
                const __m256i base8 = _mm256_broadcastsi128_si256(srcHi);
                const __m256i baseUV = _mm256_shuffle_epi8(base8, paExtractUV);
                PREDICTED_AVERAGE_1D_256(destUV, baseUV);
            }
#endif

            DITHER_ADD_256(destUV, ditherBuffer, ditherScale256);
            const __m256i inter = _mm256_shuffle_epi8(
                _mm256_permute4x64_epi64(destUV, _MM_SHUFFLE(3, 1, 2, 0)), interleaveShuf256);
            AVX_STORE_OUTPUT(destRowPtr + (srcWidth - 8) * 2, inter);
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
