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
// ARM NEON optimized upscaler - 8 source sample loop step
//

// Upscale a single plane by doubling with a 4 tap filter applied vertically then horizontally
//
// Destination plane dimensions need to be twice the input plane dimensions
//
// ARM NEON Optimised version - processes 8 source pixels per iteration (16 outputs)
//
// Does not need to handle source widths less than 16
//
// Should exactly match Upscale2DPlaneReferenceS16
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
#error "Use upscale_2d_nv12_neon.h for FP_NV12"
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
#define DEST_PTR(row, x) ((row) + (x))
#elif OUT_FIXED_POINT == FP_U16
#define STORE_VECTOR(row, x, values)  \
    vst1q_u16((uint16_t*)(row) + (x), \
              neonDemoteU16x8((values), params.offset, params.shift, params.midpoint))
#define DEST_PTR(row, x) ((row) + ((size_t)(x)*2))
#elif OUT_FIXED_POINT == FP_S16
#define STORE_VECTOR(row, x, values) vst1q_s16((int16_t*)(row) + (x), (values))
#define DEST_PTR(row, x) ((row) + ((size_t)(x)*2))
#else
#error "Unsupported OUT_FIXED_POINT"
#endif

{
    assert(srcWidth >= 16);

    // Kernel coefficients broadcast to vectors (full width for HORIZONTAL_FILTER_X2,
    // low halves for VERTICAL_FILTER / HORIZONTAL_FILTER)
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

    //

    const int32_t bodyEnd = (srcWidth - 1) & ~7;
    const int32_t overhang = srcWidth - bodyEnd;
    const int32_t rightEdgeOffset = (overhang > 4) ? bodyEnd : (bodyEnd - 4);

    // Lookup the righthand shuffles depending on overhang
    uint8x16_t rightEdgeShufA = rightEdgeControlA[overhang - 1];
    uint8x16_t rightEdgeShufB = rightEdgeControlB[overhang - 1];

    // Process each row pair (0,1), (2,3), (4,5), etc.
    for (int32_t sy = (int32_t)yStart; sy < (int32_t)yEnd; ++sy) {
        const uint16_t* ditherBuffer =
            ditherEnabled ? ldppDitherGetBuffer(dither, (size_t)srcWidth << 2) : NULL;

        // Get clamped source row indices - need 5 rows for the shifted output pair
        const uint8_t* const srcRow0Ptr = neonRowPtr(src, srcStride, srcHeight, sy - 2);
        const uint8_t* const srcRow1Ptr = neonRowPtr(src, srcStride, srcHeight, sy - 1);
        const uint8_t* const srcRow2Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 0);
        const uint8_t* const srcRow3Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 1);
        const uint8_t* const srcRow4Ptr = neonRowPtr(src, srcStride, srcHeight, sy + 2);

        // Dest row indices - pairs (0,1), (2,3), (4,5), etc.
        uint8_t* const destRow0Ptr = neonRowPtr(dst, dstStride, srcHeight * 2, sy * 2 + 0);
        uint8_t* const destRow1Ptr = neonRowPtr(dst, dstStride, srcHeight * 2, sy * 2 + 1);

        // Carry-forward vertical filter results (two full vectors: prev and curr)
        int16x8_t vfPrevEven, vfPrevOdd, vfCurrEven, vfCurrOdd;

        // Start of horizontal loop - load left hand edge, and use leftEdgeShuf to
        // deal with alignment and edge extension. Sets up initial Prev/Curr values
        // for body of main loop.
        {
            const int16x8_t srcRow0_0 = LOAD_SRC8(srcRow0Ptr, 0);
            const int16x8_t srcRow1_0 = LOAD_SRC8(srcRow1Ptr, 0);
            const int16x8_t srcRow2_0 = LOAD_SRC8(srcRow2Ptr, 0);
            const int16x8_t srcRow3_0 = LOAD_SRC8(srcRow3Ptr, 0);
            const int16x8_t srcRow4_0 = LOAD_SRC8(srcRow4Ptr, 0);

            const int16x8_t srcRow0_8 = LOAD_SRC8(srcRow0Ptr, 8);
            const int16x8_t srcRow1_8 = LOAD_SRC8(srcRow1Ptr, 8);
            const int16x8_t srcRow2_8 = LOAD_SRC8(srcRow2Ptr, 8);
            const int16x8_t srcRow3_8 = LOAD_SRC8(srcRow3Ptr, 8);
            const int16x8_t srcRow4_8 = LOAD_SRC8(srcRow4Ptr, 8);

            int16x8_t vf0Even, vf0Odd, vf8Even, vf8Odd;
            VERTICAL_FILTER_PAIR(vf0Even, vf0Odd, srcRow0_0, srcRow1_0, srcRow2_0, srcRow3_0, srcRow4_0);
            VERTICAL_FILTER_PAIR(vf8Even, vf8Odd, srcRow0_8, srcRow1_8, srcRow2_8, srcRow3_8, srcRow4_8);

            // Tap windows for centers 0..7: [-2..5] and [-1..6] via left edge replication,
            // [0..7] is the block itself, [1..8] and [2..9] bridge into the next block
            const int16x8_t tEven0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vf0Even), leftEdgeShuf0));
            const int16x8_t tOdd0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vf0Odd), leftEdgeShuf0));
            const int16x8_t tEven1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vf0Even), leftEdgeShuf1));
            const int16x8_t tOdd1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(vf0Odd), leftEdgeShuf1));
            const int16x8_t tEven3 = vextq_s16(vf0Even, vf8Even, 1);
            const int16x8_t tOdd3 = vextq_s16(vf0Odd, vf8Odd, 1);
            const int16x8_t tEven4 = vextq_s16(vf0Even, vf8Even, 2);
            const int16x8_t tOdd4 = vextq_s16(vf0Odd, vf8Odd, 2);

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2(destRow0A, destRow0B, tEven0, tEven1, vf0Even, tEven3, tEven4);
            HORIZONTAL_FILTER_X2(destRow1A, destRow1B, tOdd0, tOdd1, vf0Odd, tOdd3, tOdd4);

            PREDICTED_AVERAGE(destRow0A, destRow1A, vget_low_s16(srcRow2_0));
            PREDICTED_AVERAGE(destRow0B, destRow1B, vget_high_s16(srcRow2_0));

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            STORE(destRow0Ptr, destRow1Ptr, destRow0A, destRow0B, destRow1A, destRow1B);

            // Set up carry forward for body loop
            vfPrevEven = vf0Even;
            vfPrevOdd = vf0Odd;
            vfCurrEven = vf8Even;
            vfCurrOdd = vf8Odd;
        }

        // Main 8-wide body loop with aligned loads and vext window construction
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-BEGIN neon2d_body");
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            // Load next aligned chunk of source sammples
            const int16x8_t srcRow0 = LOAD_SRC8(srcRow0Ptr, sx + 8);
            const int16x8_t srcRow1 = LOAD_SRC8(srcRow1Ptr, sx + 8);
            const int16x8_t srcRow2 = LOAD_SRC8(srcRow2Ptr, sx + 8);
            const int16x8_t srcRow3 = LOAD_SRC8(srcRow3Ptr, sx + 8);
            const int16x8_t srcRow4 = LOAD_SRC8(srcRow4Ptr, sx + 8);

            int16x8_t vfNextEven, vfNextOdd;
            VERTICAL_FILTER_PAIR(vfNextEven, vfNextOdd, srcRow0, srcRow1, srcRow2, srcRow3, srcRow4);

            // Tap windows for centers sx..sx+7 from previous, current and next blocks
            const int16x8_t tEven0 = vextq_s16(vfPrevEven, vfCurrEven, 6);
            const int16x8_t tOdd0 = vextq_s16(vfPrevOdd, vfCurrOdd, 6);
            const int16x8_t tEven1 = vextq_s16(vfPrevEven, vfCurrEven, 7);
            const int16x8_t tOdd1 = vextq_s16(vfPrevOdd, vfCurrOdd, 7);
            const int16x8_t tEven3 = vextq_s16(vfCurrEven, vfNextEven, 1);
            const int16x8_t tOdd3 = vextq_s16(vfCurrOdd, vfNextOdd, 1);
            const int16x8_t tEven4 = vextq_s16(vfCurrEven, vfNextEven, 2);
            const int16x8_t tOdd4 = vextq_s16(vfCurrOdd, vfNextOdd, 2);

            int16x8_t destRow0A, destRow0B, destRow1A, destRow1B;
            HORIZONTAL_FILTER_X2(destRow0A, destRow0B, tEven0, tEven1, vfCurrEven, tEven3, tEven4);
            HORIZONTAL_FILTER_X2(destRow1A, destRow1B, tOdd0, tOdd1, vfCurrOdd, tOdd3, tOdd4);

            // Move carry forward on
            vfPrevEven = vfCurrEven;
            vfPrevOdd = vfCurrOdd;
            vfCurrEven = vfNextEven;
            vfCurrOdd = vfNextOdd;

            PREDICTED_AVERAGE(destRow0A, destRow1A, LOAD_SRC4(srcRow2Ptr, sx));
            PREDICTED_AVERAGE(destRow0B, destRow1B, LOAD_SRC4(srcRow2Ptr, sx + 4));

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            STORE(DEST_PTR(destRow0Ptr, destX), DEST_PTR(destRow1Ptr, destX), destRow0A, destRow0B,
                  destRow1A, destRow1B);
        }
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-END neon2d_body");
        // End of loop
        {
            // Construct the two horizontal filter inputs windows from last carry forward
            const int16x8_t hEvenA = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufA);
            const int16x8_t hOddA = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufA);
            const int16x8_t hEvenB = shuffle2(vfPrevEven, vfCurrEven, rightEdgeShufB);
            const int16x8_t hOddB = shuffle2(vfPrevOdd, vfCurrOdd, rightEdgeShufB);

            int16x8_t destRow0A = HORIZONTAL_FILTER(hEvenA);
            int16x8_t destRow1A = HORIZONTAL_FILTER(hOddA);
            int16x8_t destRow0B = HORIZONTAL_FILTER(hEvenB);
            int16x8_t destRow1B = HORIZONTAL_FILTER(hOddB);

            PREDICTED_AVERAGE(destRow0A, destRow1A, LOAD_SRC4(srcRow2Ptr, rightEdgeOffset));
            PREDICTED_AVERAGE(destRow0B, destRow1B, LOAD_SRC4(srcRow2Ptr, rightEdgeOffset + 4));

            DITHER_ADD(destRow0A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow0B, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1A, ditherBuffer, ditherScale);
            DITHER_ADD(destRow1B, ditherBuffer, ditherScale);

            const int32_t destX = rightEdgeOffset * 2;
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
