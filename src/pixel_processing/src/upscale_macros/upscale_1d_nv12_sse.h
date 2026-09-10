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

// SSE4.1 optimized 1D interleaved (NV12/UV) upscaler - doubles width with a 4-tap horizontal filter
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

// Upscale a single interleaved UV plane by doubling width with a 4-tap horizontal filter
//
// Source is interleaved UV: [U0,V0, U1,V1, ...]. Each channel is filtered independently
// using same-channel neighbours with edge replication at boundaries.
//
// Destination plane dimensions need to be twice the width of source plane dimensions.
//
// SSE4.1 optimised version - processes 4 source UV pairs per iteration (16 outputs).
//
// Does not need to handle source widths less than 16.
//
// Should exactly match Upscale1DPlaneInterleavedReferenceS16.
//
{
    assert((srcWidth % 2) == 0);
    assert(srcWidth >= 16);

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

    // Edge replication shuffle (32-bit UV pair units): [pair0, pair0, pair0, pair1]
    const __m128i leftEdgeShuf = _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0);

    // Deinterleave shuffle: within 128 bits, move even 16-bit words to low qword, odd to high qword
    const __m128i deintShuf = _mm_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0);

    // Right edge: replicate last UV pair (bytes 12-15 of srcHi) into positions beyond srcW
    const __m128i rightEdgeShuf =
        _mm_setr_epi8(8, 9, 10, 11, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15);

#if APPLY_PA
    const __m128i ones = _mm_set1_epi16(1);
    const __m128i pa1d_ones = _mm_set1_epi32(1);
    const __m128i paExtractU = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 13, 12, 9, 8, 5, 4, 1, 0);
    const __m128i paExtractV = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 15, 14, 11, 10, 7, 6, 3, 2);
#endif

#if DITHER
    const bool ditherEnabled = true;
    const __m128i ditherOffset = _mm_set1_epi16((int16_t)dither->strength);
    const __m128i ditherShift = _mm_cvtsi32_si128((int32_t)params.shift);
    const __m128i ditherMul = _mm_set1_epi16((int16_t)(dither->strength * 2u + 1u));
#endif

    // bodyEnd: largest multiple of 8 (int16 elements = 4 UV pairs) strictly less than srcW
    const int32_t bodyEnd = ((srcWidth / 2 - 1) & ~3) * 2;

    for (uint32_t sy = yStart; sy < yEnd; ++sy) {
#if DITHER
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
#endif
        const SSE_INPUT_TYPE* const srcRowPtr = SSE_INPUT_ROW(src, srcStride, srcHeight, sy);
        SSE_OUTPUT_TYPE* const destRowPtr = SSE_OUTPUT_ROW(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        __m128i srcPrev, srcCurr;

        // Left edge block: loads src[0..7] and src[8..15], applies left-edge pair replication
        // and processes source pairs 0..3, writing dest[0..15].
        {
            const __m128i src0 = SSE_LOAD_INPUT(srcRowPtr);
            const __m128i src8 = SSE_LOAD_INPUT(srcRowPtr + 8);

            // hLo: [pair(-2), pair(-1), pair(0), pair(1)] — pair(0) replicated into phantoms
            const __m128i hLo = _mm_shuffle_epi8(src0, leftEdgeShuf);
            // hHi: [pair(2), pair(3), pair(4), pair(5)]
            const __m128i hHi = _mm_alignr_epi8(src8, src0, 8);

            // Deinterleave to per-channel windows for the horizontal filter
            __m128i uVec, vVec;
            DEINTERLEAVE(hLo, hHi, uVec, vVec);

            __m128i destU, destV;
            HORIZONTAL_FILTER(uVec, destU);
            HORIZONTAL_FILTER(vVec, destV);

#if APPLY_PA
            // PA bases: deinterleave src0 to get [U0..U3] and [V0..V3]
            const __m128i baseU = _mm_shuffle_epi8(src0, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(src0, paExtractV);
            PREDICTED_AVERAGE_1D(destU, baseU);
            PREDICTED_AVERAGE_1D(destV, baseV);
#endif

            DITHER_ADD(destU, ditherBuffer, ditherScale);
            DITHER_ADD(destV, ditherBuffer, ditherScale);

            // Re-interleave and store
            SSE_STORE_OUTPUT(destRowPtr + 0, _mm_unpacklo_epi16(destU, destV));
            SSE_STORE_OUTPUT(destRowPtr + 8, _mm_unpackhi_epi16(destU, destV));

            srcPrev = src0;
            srcCurr = src8;
        }

        // Body loop: each iteration processes 4 source UV pairs (8 int16 elements), producing
        // 8 output UV pairs (16 int16 elements). Carries srcPrev and srcCurr forward.
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-BEGIN sse1d_ilv_body" ::: "memory");
#endif
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            const __m128i srcNext = SSE_LOAD_INPUT(srcRowPtr + sx + 8);

#if APPLY_PA
            // Save srcCurr for PA bases before advancing the carry
            const __m128i currSrc = srcCurr;
#endif

            // hLo: [pair(sx/2-2)..pair(sx/2+1)] — 2-pair look-back via carry
            const __m128i hLo = _mm_alignr_epi8(srcCurr, srcPrev, 8);
            // hHi: [pair(sx/2+2)..pair(sx/2+5)] — 2-pair look-ahead via next block
            const __m128i hHi = _mm_alignr_epi8(srcNext, srcCurr, 8);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            __m128i uVec, vVec;
            DEINTERLEAVE(hLo, hHi, uVec, vVec);

            __m128i destU, destV;
            HORIZONTAL_FILTER(uVec, destU);
            HORIZONTAL_FILTER(vVec, destV);

#if APPLY_PA
            // PA bases: the 4 source pairs centred on sx
            const __m128i baseU = _mm_shuffle_epi8(currSrc, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(currSrc, paExtractV);
            PREDICTED_AVERAGE_1D(destU, baseU);
            PREDICTED_AVERAGE_1D(destV, baseV);
#endif

            DITHER_ADD(destU, ditherBuffer, ditherScale);
            DITHER_ADD(destV, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            SSE_STORE_OUTPUT(destRowPtr + destX + 0, _mm_unpacklo_epi16(destU, destV));
            SSE_STORE_OUTPUT(destRowPtr + destX + 8, _mm_unpackhi_epi16(destU, destV));
        }
#if !defined(_MSC_VER)
        __asm volatile("# LLVM-MCA-END sse1d_ilv_body" ::: "memory");
#endif

        // Right edge block: fresh loads from srcW-16 and srcW-8, applies right-edge pair
        // replication, and writes at (srcW-8)*2. May overlap and overwrite the last body
        // write when the overhang is small, replacing it with the correctly edge-clamped value.
        {
            const __m128i srcLo = SSE_LOAD_INPUT(srcRowPtr + srcWidth - 16);
            const __m128i srcHi = SSE_LOAD_INPUT(srcRowPtr + srcWidth - 8);

            // hLo: 4 pairs ending at pair(nP-3), bridging the two loads
            const __m128i hLo = _mm_alignr_epi8(srcHi, srcLo, 8);
            // hHi: last pair replicated into phantom positions
            const __m128i hHi = _mm_shuffle_epi8(srcHi, rightEdgeShuf);

            __m128i uVec, vVec;
            DEINTERLEAVE(hLo, hHi, uVec, vVec);

            __m128i destU, destV;
            HORIZONTAL_FILTER(uVec, destU);
            HORIZONTAL_FILTER(vVec, destV);

#if APPLY_PA
            // PA bases: srcHi covers source pairs nP-4..nP-1
            const __m128i baseU = _mm_shuffle_epi8(srcHi, paExtractU);
            const __m128i baseV = _mm_shuffle_epi8(srcHi, paExtractV);
            PREDICTED_AVERAGE_1D(destU, baseU);
            PREDICTED_AVERAGE_1D(destV, baseV);
#endif

            DITHER_ADD(destU, ditherBuffer, ditherScale);
            DITHER_ADD(destV, ditherBuffer, ditherScale);

            const int32_t destX = (srcWidth - 8) * 2;
            SSE_STORE_OUTPUT(destRowPtr + destX + 0, _mm_unpacklo_epi16(destU, destV));
            SSE_STORE_OUTPUT(destRowPtr + destX + 8, _mm_unpackhi_epi16(destU, destV));
        }
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
