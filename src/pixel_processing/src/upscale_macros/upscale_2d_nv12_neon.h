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

// Interleaved (two-channel) NEON upscaler
// Processes 8 interleaved source samples (4 per channel) per iteration.
//
// The whole pipeline runs in the interleaved domain: the vertical filter is
// channel-agnostic, and the horizontal filter uses pair-stride tap windows
// (HORIZONTAL_FILTER_X2_UV) so no deinterleave/reinterleave is ever needed —
// outputs come out directly in NV12 store layout, and the PA bases load
// directly as interleaved pairs.
//
#if defined(VN_UPSCALE_LOOP)
#define IN_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT(VN_UPSCALE_LOOP)
#define OUT_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT(VN_UPSCALE_LOOP)
#define APPLY_PA VN_UPSCALE_LOOP_CONFIG_APPLY_PA(VN_UPSCALE_LOOP)
#define DITHER VN_UPSCALE_LOOP_CONFIG_DITHER(VN_UPSCALE_LOOP)
#define NEON_DECODED_UPSCALE_LOOP 1
#endif
#if !defined(IN_FIXED_POINT) || !defined(OUT_FIXED_POINT) || !defined(APPLY_PA) || !defined(DITHER)
#error "Define VN_UPSCALE_LOOP or IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA and DITHER"
#endif
#if defined(NEON_STANDALONE_S16_NV12)
#if IN_FIXED_POINT != FP_S16 || OUT_FIXED_POINT != FP_S16
#error "The standalone legacy NV12 wrapper requires FP_S16 input and output"
#endif
#define LOAD_SRC8(row, x) vld1q_s16((const int16_t*)(row) + (x))
#define LOAD_SRC4(row, x) vld1_s16((const int16_t*)(row) + (x))
#define STORE_VECTOR(row, x, values) vst1q_s16((int16_t*)(row) + (x), (values))
#define DEST_PTR(row, x) ((row) + ((size_t)(x)*2))
#else
#if IN_FIXED_POINT != FP_NV12 || OUT_FIXED_POINT != FP_NV12
#error "NV12 is U8 interleaved and requires FP_NV12 input and output"
#endif
#define LOAD_SRC8(row, x) neonPromoteU8x8((row) + (x))
#define LOAD_SRC4(row, x) neonPromoteU8x4((row) + (x))
#define STORE_VECTOR(row, x, values) vst1_u8((row) + (x), neonDemoteU8x8(values))
#define DEST_PTR(row, x) ((row) + (x))
#endif

{
    assert((srcWidth % 2) == 0);
    assert(srcWidth >= 16);

    // Kernel coefficients broadcast to vectors (full width for the _high
    // multiplies, low halves for the rest)
    const int16x8_t kq0 = vdupq_n_s16(params.kernel[0]);
    const int16x8_t kq1 = vdupq_n_s16(params.kernel[1]);
    const int16x8_t kq2 = vdupq_n_s16(params.kernel[2]);
    const int16x8_t kq3 = vdupq_n_s16(params.kernel[3]);
    const int16x4_t k0 = vget_low_s16(kq0);
    const int16x4_t k1 = vget_low_s16(kq1);
    const int16x4_t k2 = vget_low_s16(kq2);
    const int16x4_t k3 = vget_low_s16(kq3);

    // 'Half' for filter result rounding
    const int32x4_t round = vdupq_n_s32(0x2000);

    const bool ditherEnabled = (DITHER) && dither != NULL && dither->strength != 0;

    const uint16_t ditherScale = ditherEnabled ? (uint16_t)(dither->strength * 2u + 1u) : 0;
    const int16x8_t ditherOffset = vdupq_n_s16(ditherEnabled ? (int16_t)dither->strength : 0);
    const int16x8_t ditherShift = vdupq_n_s16((int16_t)params.shift);

    // Edge-extension shuffle for non-8-aligned widths: replicates the last valid
    // interleaved UV pair into padding positions. Operates on 4-byte (pair) units
    // since interleaved data has alternating U,V samples.
    const int32_t overhang = srcWidth & 7;
    uint8x16_t edgeExtShuf = {0};
    if (overhang > 0) {
        const uint8x16_t identity = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        const uint8x16_t lastPair = vdupq_n_u8((uint8_t)(2 * (overhang - 2)));
        const uint8x16_t pairByte = vandq_u8(identity, vdupq_n_u8(3));
        edgeExtShuf = vminq_u8(identity, vaddq_u8(lastPair, pairByte));
    }

    for (int32_t sy = (int32_t)yStart; sy < (int32_t)yEnd; ++sy) {
        const uint16_t* ditherBuffer =
            ditherEnabled ? ldppDitherGetBuffer(dither, (size_t)srcWidth << 2) : NULL;
        const uint8_t* const srcRow0Ptr = neonRowPtr(src, srcStride, srcHeight, sy - 2);
        const uint8_t* const srcRow1Ptr = neonRowPtr(src, srcStride, srcHeight, sy - 1);
        const uint8_t* const srcRow2Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 0);
        const uint8_t* const srcRow3Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 1);
        const uint8_t* const srcRow4Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 2);

        uint8_t* const destRow0Ptr = neonRowPtr(dst, dstStride, srcHeight * 2, sy * 2 + 0);
        uint8_t* const destRow1Ptr = neonRowPtr(dst, dstStride, srcHeight * 2, sy * 2 + 1);

        // Carry-forward vertical filter results (two full vectors: prev and curr)
        int16x8_t vfPrevEven, vfPrevOdd, vfCurrEven, vfCurrOdd;

        // Start section (sx=0): vertical filter on aligned loads, then edge-replicate results
        {
            const int16x8_t srcRow0Lo = LOAD_SRC8(srcRow0Ptr, 0);
            const int16x8_t srcRow0Hi = LOAD_SRC8(srcRow0Ptr, 8);
            const int16x8_t srcRow1Lo = LOAD_SRC8(srcRow1Ptr, 0);
            const int16x8_t srcRow1Hi = LOAD_SRC8(srcRow1Ptr, 8);
            const int16x8_t srcRow2Lo = LOAD_SRC8(srcRow2Ptr, 0);
            const int16x8_t srcRow2Hi = LOAD_SRC8(srcRow2Ptr, 8);
            const int16x8_t srcRow3Lo = LOAD_SRC8(srcRow3Ptr, 0);
            const int16x8_t srcRow3Hi = LOAD_SRC8(srcRow3Ptr, 8);
            const int16x8_t srcRow4Lo = LOAD_SRC8(srcRow4Ptr, 0);
            const int16x8_t srcRow4Hi = LOAD_SRC8(srcRow4Ptr, 8);

            // Vertical filter on raw lo and hi halves
            int16x8_t vfEvenLo, vfOddLo, vfEvenHi, vfOddHi;
            VERTICAL_FILTER_PAIR(vfEvenLo, vfOddLo, srcRow0Lo, srcRow1Lo, srcRow2Lo, srcRow3Lo, srcRow4Lo);
            VERTICAL_FILTER_PAIR(vfEvenHi, vfOddHi, srcRow0Hi, srcRow1Hi, srcRow2Hi, srcRow3Hi, srcRow4Hi);

            // Pair-stride tap windows for centers pair 0..3, left edge replicated
            const int16x8_t tEven0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfEvenLo), leftEdgeShufT0));
            const int16x8_t tOdd0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfOddLo), leftEdgeShufT0));
            const int16x8_t tEven1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfEvenLo), leftEdgeShufT1));
            const int16x8_t tOdd1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfOddLo), leftEdgeShufT1));
            const int16x8_t tEven3 = vextq_s16(vfEvenLo, vfEvenHi, 2);
            const int16x8_t tOdd3 = vextq_s16(vfOddLo, vfOddHi, 2);
            const int16x8_t tEven4 = vextq_s16(vfEvenLo, vfEvenHi, 4);
            const int16x8_t tOdd4 = vextq_s16(vfOddLo, vfOddHi, 4);

            // Save raw VF results for body loop carry-forward
            vfPrevEven = vfEvenLo;
            vfPrevOdd = vfOddLo;
            vfCurrEven = vfEvenHi;
            vfCurrOdd = vfOddHi;

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2_UV(destRow0A, destRow0B, tEven0, tEven1, vfEvenLo, tEven3, tEven4);
            HORIZONTAL_FILTER_X2_UV(destRow1A, destRow1B, tOdd0, tOdd1, vfOddLo, tOdd3, tOdd4);

#if APPLY_PA
            // PA bases are extracted from the already-promoted source vector.
            PREDICTED_AVERAGE_UV(destRow0A, destRow1A, vget_low_s16(srcRow2Lo));
            PREDICTED_AVERAGE_UV(destRow0B, destRow1B, vget_high_s16(srcRow2Lo));
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            STORE(destRow0Ptr, destRow1Ptr, destRow0A, destRow0B, destRow1A, destRow1B);
        }

        int32_t sx = 8;
        // Body loop — aligned loads with vext window construction
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-BEGIN neon2d_nv12_body");
        for (; sx + 15 < (int32_t)srcWidth; sx += 8) {
            // Load at sx+8 (aligned — sx and 8 are both multiples of 8)
            const int16x8_t srcRow0Next = LOAD_SRC8(srcRow0Ptr, sx + 8);
            const int16x8_t srcRow1Next = LOAD_SRC8(srcRow1Ptr, sx + 8);
            const int16x8_t srcRow2Next = LOAD_SRC8(srcRow2Ptr, sx + 8);
            const int16x8_t srcRow3Next = LOAD_SRC8(srcRow3Ptr, sx + 8);
            const int16x8_t srcRow4Next = LOAD_SRC8(srcRow4Ptr, sx + 8);

            // Vertical filter on next block only
            int16x8_t vfNextEven, vfNextOdd;
            VERTICAL_FILTER_PAIR(vfNextEven, vfNextOdd, srcRow0Next, srcRow1Next, srcRow2Next,
                                 srcRow3Next, srcRow4Next);

            // Pair-stride tap windows for centers pair sx/2..sx/2+3 (shifts move
            // one interleaved pair = 2 lanes per tap)
            const int16x8_t tEven0 = vextq_s16(vfPrevEven, vfCurrEven, 4);
            const int16x8_t tOdd0 = vextq_s16(vfPrevOdd, vfCurrOdd, 4);
            const int16x8_t tEven1 = vextq_s16(vfPrevEven, vfCurrEven, 6);
            const int16x8_t tOdd1 = vextq_s16(vfPrevOdd, vfCurrOdd, 6);
            const int16x8_t tEven3 = vextq_s16(vfCurrEven, vfNextEven, 2);
            const int16x8_t tOdd3 = vextq_s16(vfCurrOdd, vfNextOdd, 2);
            const int16x8_t tEven4 = vextq_s16(vfCurrEven, vfNextEven, 4);
            const int16x8_t tOdd4 = vextq_s16(vfCurrOdd, vfNextOdd, 4);

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2_UV(destRow0A, destRow0B, tEven0, tEven1, vfCurrEven, tEven3, tEven4);
            HORIZONTAL_FILTER_X2_UV(destRow1A, destRow1B, tOdd0, tOdd1, vfCurrOdd, tOdd3, tOdd4);

            // Advance carry
            vfPrevEven = vfCurrEven;
            vfPrevOdd = vfCurrOdd;
            vfCurrEven = vfNextEven;
            vfCurrOdd = vfNextOdd;

#if APPLY_PA
            PREDICTED_AVERAGE_UV(destRow0A, destRow1A, LOAD_SRC4(srcRow2Ptr, sx));
            PREDICTED_AVERAGE_UV(destRow0B, destRow1B, LOAD_SRC4(srcRow2Ptr, sx + 4));
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            STORE(DEST_PTR(destRow0Ptr, destX), DEST_PTR(destRow1Ptr, destX), destRow0A, destRow0B,
                  destRow1A, destRow1B);
        }
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-END neon2d_nv12_body");

        // Bridge: cover gap between body and end block for non-8-aligned widths.
        // Load at the last aligned position with pair-based edge extension.
        if (sx < (int32_t)srcWidth - 8) {
            const int32_t edgePos = srcWidth & ~7;
            int16x8_t r0 = LOAD_SRC8(srcRow0Ptr, edgePos);
            int16x8_t r1 = LOAD_SRC8(srcRow1Ptr, edgePos);
            int16x8_t r2 = LOAD_SRC8(srcRow2Ptr, edgePos);
            int16x8_t r3 = LOAD_SRC8(srcRow3Ptr, edgePos);
            int16x8_t r4 = LOAD_SRC8(srcRow4Ptr, edgePos);

            r0 = vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(r0), edgeExtShuf));
            r1 = vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(r1), edgeExtShuf));
            r2 = vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(r2), edgeExtShuf));
            r3 = vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(r3), edgeExtShuf));
            r4 = vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(r4), edgeExtShuf));

            int16x8_t vfNextEven, vfNextOdd;
            VERTICAL_FILTER_PAIR(vfNextEven, vfNextOdd, r0, r1, r2, r3, r4);

            // Pair-stride tap windows, as in the body loop
            const int16x8_t tEven0 = vextq_s16(vfPrevEven, vfCurrEven, 4);
            const int16x8_t tOdd0 = vextq_s16(vfPrevOdd, vfCurrOdd, 4);
            const int16x8_t tEven1 = vextq_s16(vfPrevEven, vfCurrEven, 6);
            const int16x8_t tOdd1 = vextq_s16(vfPrevOdd, vfCurrOdd, 6);
            const int16x8_t tEven3 = vextq_s16(vfCurrEven, vfNextEven, 2);
            const int16x8_t tOdd3 = vextq_s16(vfCurrOdd, vfNextOdd, 2);
            const int16x8_t tEven4 = vextq_s16(vfCurrEven, vfNextEven, 4);
            const int16x8_t tOdd4 = vextq_s16(vfCurrOdd, vfNextOdd, 4);

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2_UV(destRow0A, destRow0B, tEven0, tEven1, vfCurrEven, tEven3, tEven4);
            HORIZONTAL_FILTER_X2_UV(destRow1A, destRow1B, tOdd0, tOdd1, vfCurrOdd, tOdd3, tOdd4);

#if APPLY_PA
            PREDICTED_AVERAGE_UV(destRow0A, destRow1A, LOAD_SRC4(srcRow2Ptr, sx));
            PREDICTED_AVERAGE_UV(destRow0B, destRow1B, LOAD_SRC4(srcRow2Ptr, sx + 4));
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            STORE(DEST_PTR(destRow0Ptr, destX), DEST_PTR(destRow1Ptr, destX), destRow0A, destRow0B,
                  destRow1A, destRow1B);
        }

        // End section: loads with edge replication on the right
        {
            const int16x8_t srcRow0Lo = LOAD_SRC8(srcRow0Ptr, srcWidth - 16);
            const int16x8_t srcRow0Hi = LOAD_SRC8(srcRow0Ptr, srcWidth - 8);
            const int16x8_t srcRow1Lo = LOAD_SRC8(srcRow1Ptr, srcWidth - 16);
            const int16x8_t srcRow1Hi = LOAD_SRC8(srcRow1Ptr, srcWidth - 8);
            const int16x8_t srcRow2Lo = LOAD_SRC8(srcRow2Ptr, srcWidth - 16);
            const int16x8_t srcRow2Hi = LOAD_SRC8(srcRow2Ptr, srcWidth - 8);
            const int16x8_t srcRow3Lo = LOAD_SRC8(srcRow3Ptr, srcWidth - 16);
            const int16x8_t srcRow3Hi = LOAD_SRC8(srcRow3Ptr, srcWidth - 8);
            const int16x8_t srcRow4Lo = LOAD_SRC8(srcRow4Ptr, srcWidth - 16);
            const int16x8_t srcRow4Hi = LOAD_SRC8(srcRow4Ptr, srcWidth - 8);

            // Vertical filter on raw lo and hi halves
            int16x8_t vfEvenLo, vfOddLo, vfEvenHi, vfOddHi;
            VERTICAL_FILTER_PAIR(vfEvenLo, vfOddLo, srcRow0Lo, srcRow1Lo, srcRow2Lo, srcRow3Lo, srcRow4Lo);
            VERTICAL_FILTER_PAIR(vfEvenHi, vfOddHi, srcRow0Hi, srcRow1Hi, srcRow2Hi, srcRow3Hi, srcRow4Hi);

            // Pair-stride tap windows for the last 4 pair centers, right edge replicated
            const int16x8_t tEven0 = vextq_s16(vfEvenLo, vfEvenHi, 4);
            const int16x8_t tOdd0 = vextq_s16(vfOddLo, vfOddHi, 4);
            const int16x8_t tEven1 = vextq_s16(vfEvenLo, vfEvenHi, 6);
            const int16x8_t tOdd1 = vextq_s16(vfOddLo, vfOddHi, 6);
            const int16x8_t tEven3 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfEvenHi), rightEdgeShufT3));
            const int16x8_t tOdd3 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfOddHi), rightEdgeShufT3));
            const int16x8_t tEven4 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfEvenHi), rightEdgeShufT4));
            const int16x8_t tOdd4 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vfOddHi), rightEdgeShufT4));

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2_UV(destRow0A, destRow0B, tEven0, tEven1, vfEvenHi, tEven3, tEven4);
            HORIZONTAL_FILTER_X2_UV(destRow1A, destRow1B, tOdd0, tOdd1, vfOddHi, tOdd3, tOdd4);

#if APPLY_PA
            PREDICTED_AVERAGE_UV(destRow0A, destRow1A, vget_low_s16(srcRow2Hi));
            PREDICTED_AVERAGE_UV(destRow0B, destRow1B, vget_high_s16(srcRow2Hi));
#endif

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            const int32_t destX = (srcWidth - 8) * 2;
            STORE(DEST_PTR(destRow0Ptr, destX), DEST_PTR(destRow1Ptr, destX), destRow0A, destRow0B,
                  destRow1A, destRow1B);
        }
    }
}

#undef DEST_PTR
#undef STORE_VECTOR
#undef LOAD_SRC4
#undef LOAD_SRC8
#ifdef NEON_DECODED_UPSCALE_LOOP
#undef NEON_DECODED_UPSCALE_LOOP
#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
#endif
