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
// ARM NEON optimized 1D upscaler - doubles width with a 4-tap horizontal filter
//

// Upscale a single plane by doubling width with a 4-tap horizontal filter
//
// Destination plane dimensions need to be twice the width of source plane dimensions
//
// ARM NEON optimised version - processes 8 source pixels per iteration (16 outputs)
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
#define NEON_DECODED_UPSCALE_LOOP 1
#endif
#if !defined(IN_FIXED_POINT) || !defined(OUT_FIXED_POINT) || !defined(APPLY_PA) || !defined(DITHER)
#error "Define VN_UPSCALE_LOOP or IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA and DITHER"
#endif
#if IN_FIXED_POINT == FP_NV12 || OUT_FIXED_POINT == FP_NV12
#error "Use upscale_1d_nv12_neon.h for FP_NV12"
#endif

#if IN_FIXED_POINT == FP_U8
#define LOAD_SRC8(row, x) neonPromoteU8x8((row) + (x))
#define LOAD_SRC4(row, x) neonPromoteU8x4((row) + (x))
#elif IN_FIXED_POINT == FP_U16
#define LOAD_SRC8(row, x) neonPromoteU16x8((row) + ((size_t)(x)*2), params.shift)
#define LOAD_SRC4(row, x) neonPromoteU16x4((row) + ((size_t)(x)*2), params.shift)
#elif IN_FIXED_POINT == FP_S16
#define LOAD_SRC8(row, x) vld1q_s16((const int16_t*)(row) + (x))
#define LOAD_SRC4(row, x) vld1_s16((const int16_t*)(row) + (x))
#else
#error "Unsupported IN_FIXED_POINT"
#endif

#if OUT_FIXED_POINT == FP_U8
#define STORE_VECTOR(row, x, values) vst1_u8((row) + (x), neonDemoteU8x8(values))
#elif OUT_FIXED_POINT == FP_U16
#define STORE_VECTOR(row, x, values)  \
    vst1q_u16((uint16_t*)(row) + (x), \
              neonDemoteU16x8((values), params.offset, params.shift, params.midpoint))
#elif OUT_FIXED_POINT == FP_S16
#define STORE_VECTOR(row, x, values) vst1q_s16((int16_t*)(row) + (x), (values))
#else
#error "Unsupported OUT_FIXED_POINT"
#endif

{
    assert(srcWidth >= 16);

    // Kernel coefficients broadcast to vectors (full width for HORIZONTAL_FILTER_X2,
    // low halves for the right-edge HORIZONTAL_FILTER)
    const int16x8_t kq0 = vdupq_n_s16(params.kernel[0]);
    const int16x8_t kq1 = vdupq_n_s16(params.kernel[1]);
    const int16x8_t kq2 = vdupq_n_s16(params.kernel[2]);
    const int16x8_t kq3 = vdupq_n_s16(params.kernel[3]);
    const int16x4_t k0 = vget_low_s16(kq0);
    const int16x4_t k1 = vget_low_s16(kq1);
    const int16x4_t k2 = vget_low_s16(kq2);
    const int16x4_t k3 = vget_low_s16(kq3);

    // Rounding constant for the filter accumulator
    const int32x4_t round = vdupq_n_s32(0x2000);

    const bool ditherEnabled = (DITHER) && dither != NULL && dither->strength != 0;

    const uint16_t ditherScale = ditherEnabled ? (uint16_t)(dither->strength * 2u + 1u) : 0;
    const int16x8_t ditherOffset = vdupq_n_s16(ditherEnabled ? (int16_t)dither->strength : 0);
    const int16x8_t ditherShift = vdupq_n_s16((int16_t)params.shift);

    const int32_t bodyEnd = (srcWidth - 1) & ~7;
    const int32_t overhang = srcWidth - bodyEnd;
    const int32_t rightEdgeOffset = (overhang > 4) ? bodyEnd : (bodyEnd - 4);

    // Precompute right-edge shuffle and offset
    const uint8x16_t rightEdgeShufA = rightEdgeControlA[overhang - 1];
    const uint8x16_t rightEdgeShufB = rightEdgeControlB[overhang - 1];

    for (int32_t sy = (int32_t)yStart; sy < (int32_t)yEnd; ++sy) {
        const uint16_t* ditherBuffer =
            ditherEnabled ? ldppDitherGetBuffer(dither, (size_t)srcWidth << 2) : NULL;
        const uint8_t* const srcRowPtr = neonRowPtr(src, srcStride, srcHeight, sy);
        uint8_t* const destRowPtr = neonRowPtr(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        int16x8_t srcPrev, srcCurr;

        // Left edge block: loads src[0..7] and src[8..15], applies left-edge replication
        // and processes source positions 0..7, writing dest[0..15].
        {
            const int16x8_t src0 = LOAD_SRC8(srcRowPtr, 0);
            const int16x8_t src8 = LOAD_SRC8(srcRowPtr, 8);

            // Tap windows for centers 0..7: [-2..5] and [-1..6] via left edge replication,
            // [0..7] is src0 itself, [1..8] and [2..9] bridge into src8
            const int16x8_t t0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(src0), leftEdgeShuf0));
            const int16x8_t t1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(src0), leftEdgeShuf1));
            const int16x8_t t3 = vextq_s16(src0, src8, 1);
            const int16x8_t t4 = vextq_s16(src0, src8, 2);

            int16x8_t destA, destB;
            HORIZONTAL_FILTER_X2(destA, destB, t0, t1, src0, t3, t4);

            PREDICTED_AVERAGE_1D(destA, vget_low_s16(src0));
            PREDICTED_AVERAGE_1D(destB, vget_high_s16(src0));

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            STORE_VECTOR(destRowPtr, 0, destA);
            STORE_VECTOR(destRowPtr, 8, destB);

            srcPrev = src0;
            srcCurr = src8;
        }

        // Main body loop: each iteration processes 8 source positions, producing
        // 16 output samples.  Carries srcPrev and srcCurr forward to build the
        // 5-wide horizontal windows without reloading.
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-BEGIN neon1d_body");
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            const int16x8_t srcNext = LOAD_SRC8(srcRowPtr, sx + 8);

            // Tap windows for centers sx..sx+7 from previous, current and next blocks
            const int16x8_t t0 = vextq_s16(srcPrev, srcCurr, 6);
            const int16x8_t t1 = vextq_s16(srcPrev, srcCurr, 7);
            const int16x8_t t3 = vextq_s16(srcCurr, srcNext, 1);
            const int16x8_t t4 = vextq_s16(srcCurr, srcNext, 2);

            int16x8_t destA, destB;
            HORIZONTAL_FILTER_X2(destA, destB, t0, t1, srcCurr, t3, t4);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            PREDICTED_AVERAGE_1D(destA, vget_low_s16(srcPrev));
            PREDICTED_AVERAGE_1D(destB, vget_high_s16(srcPrev));

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            STORE_VECTOR(destRowPtr, destX + 0, destA);
            STORE_VECTOR(destRowPtr, destX + 8, destB);
        }
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-END neon1d_body");

        // Right edge block: uses byte-shuffle to replicate the last valid source
        // sample into the phantom positions beyond srcW, then writes dest at
        // rightEdgeOffset*2 (may overlap the last body write when overhang <= 4).
        {
            const int16x8_t hA = shuffle2(srcPrev, srcCurr, rightEdgeShufA);
            const int16x8_t hB = shuffle2(srcPrev, srcCurr, rightEdgeShufB);

            int16x8_t destA = HORIZONTAL_FILTER(hA);
            int16x8_t destB = HORIZONTAL_FILTER(hB);

            PREDICTED_AVERAGE_1D(destA, LOAD_SRC4(srcRowPtr, rightEdgeOffset));
            PREDICTED_AVERAGE_1D(destB, LOAD_SRC4(srcRowPtr, rightEdgeOffset + 4));

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = rightEdgeOffset * 2;
            STORE_VECTOR(destRowPtr, destX + 0, destA);
            STORE_VECTOR(destRowPtr, destX + 8, destB);
        }
    }
}

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
