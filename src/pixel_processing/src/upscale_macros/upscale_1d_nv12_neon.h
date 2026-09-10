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
// ARM NEON optimized 1D interleaved (NV12/UV) upscaler - doubles width with a 4-tap horizontal filter
//

// Upscale a single interleaved UV plane by doubling width with a 4-tap horizontal filter
//
// Source is interleaved UV: [U0,V0, U1,V1, ...]. Each channel is filtered independently
// using same-channel neighbours with edge replication at boundaries.
//
// Destination plane dimensions need to be twice the width of source plane dimensions.
//
// ARM NEON optimised version - processes 4 source UV pairs per iteration (16 outputs).
//
// The whole pipeline runs in the interleaved domain: the horizontal filter uses
// pair-stride tap windows (HORIZONTAL_FILTER_X2_UV) so no deinterleave or
// reinterleave is needed — outputs come out directly in NV12 store layout, and
// the PA bases are the interleaved source pairs themselves.
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
#define STORE_VECTOR(row, x, values) vst1q_s16((int16_t*)(row) + (x), (values))
#else
#if IN_FIXED_POINT != FP_NV12 || OUT_FIXED_POINT != FP_NV12
#error "NV12 is U8 interleaved and requires FP_NV12 input and output"
#endif
#define LOAD_SRC8(row, x) neonPromoteU8x8((row) + (x))
#define STORE_VECTOR(row, x, values) vst1_u8((row) + (x), neonDemoteU8x8(values))
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

    // Rounding constant for the filter accumulator
    const int32x4_t round = vdupq_n_s32(0x2000);

    const bool ditherEnabled = (DITHER) && dither != NULL && dither->strength != 0;

    const uint16_t ditherScale = ditherEnabled ? (uint16_t)(dither->strength * 2u + 1u) : 0;
    const int16x8_t ditherOffset = vdupq_n_s16(ditherEnabled ? (int16_t)dither->strength : 0);
    const int16x8_t ditherShift = vdupq_n_s16((int16_t)params.shift);

    // bodyEnd: largest multiple of 8 (int16 elements = 4 UV pairs) strictly less than srcW
    const int32_t bodyEnd = ((srcWidth / 2 - 1) & ~3) * 2;

    for (int32_t sy = (int32_t)yStart; sy < (int32_t)yEnd; ++sy) {
        const uint16_t* ditherBuffer =
            ditherEnabled ? ldppDitherGetBuffer(dither, (size_t)srcWidth << 2) : NULL;
        const uint8_t* const srcRowPtr = neonRowPtr(src, srcStride, srcHeight, sy);
        uint8_t* const destRowPtr = neonRowPtr(dst, dstStride, srcHeight, sy);

        // Carry-forward source vectors (prev and curr aligned 8-wide chunks)
        int16x8_t srcPrev, srcCurr;

        // Left edge block: loads src[0..7] and src[8..15], applies left-edge pair replication
        // and processes source pairs 0..3, writing dest[0..15].
        {
            const int16x8_t src0 = LOAD_SRC8(srcRowPtr, 0);
            const int16x8_t src8 = LOAD_SRC8(srcRowPtr, 8);

            // Pair-stride tap windows for centers pair 0..3, left edge replicated
            const int16x8_t t0 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(src0), leftEdgeShufT0));
            const int16x8_t t1 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(src0), leftEdgeShufT1));
            const int16x8_t t3 = vextq_s16(src0, src8, 2);
            const int16x8_t t4 = vextq_s16(src0, src8, 4);

            int16x8_t destA, destB;
            HORIZONTAL_FILTER_X2_UV(destA, destB, t0, t1, src0, t3, t4);

#if APPLY_PA
            PREDICTED_AVERAGE_UV_1D(destA, vget_low_s16(src0));
            PREDICTED_AVERAGE_UV_1D(destB, vget_high_s16(src0));
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            STORE_VECTOR(destRowPtr, 0, destA);
            STORE_VECTOR(destRowPtr, 8, destB);

            srcPrev = src0;
            srcCurr = src8;
        }

        // Body loop: each iteration processes 4 source UV pairs (8 int16 elements), producing
        // 8 output UV pairs (16 int16 elements). Carries srcPrev and srcCurr forward.
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-BEGIN neon1d_nv12_body");
        for (int32_t sx = 8; sx < bodyEnd; sx += 8) {
            const int16x8_t srcNext = LOAD_SRC8(srcRowPtr, sx + 8);

            // Save srcCurr for PA bases before advancing the carry
            const int16x8_t currSrc = srcCurr;

            // Pair-stride tap windows for centers pair sx/2..sx/2+3
            const int16x8_t t0 = vextq_s16(srcPrev, srcCurr, 4);
            const int16x8_t t1 = vextq_s16(srcPrev, srcCurr, 6);
            const int16x8_t t3 = vextq_s16(srcCurr, srcNext, 2);
            const int16x8_t t4 = vextq_s16(srcCurr, srcNext, 4);

            srcPrev = srcCurr;
            srcCurr = srcNext;

            int16x8_t destA, destB;
            HORIZONTAL_FILTER_X2_UV(destA, destB, t0, t1, currSrc, t3, t4);

#if APPLY_PA
            PREDICTED_AVERAGE_UV_1D(destA, vget_low_s16(currSrc));
            PREDICTED_AVERAGE_UV_1D(destB, vget_high_s16(currSrc));
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = sx * 2;
            STORE_VECTOR(destRowPtr, destX + 0, destA);
            STORE_VECTOR(destRowPtr, destX + 8, destB);
        }
        NEON_LLVM_MCA_MARKER("# LLVM-MCA-END neon1d_nv12_body");

        // Right edge block: fresh loads from srcW-16 and srcW-8, applies right-edge pair
        // replication, and writes at (srcW-8)*2. May overlap and overwrite the last body
        // write when the overhang is small, replacing it with the correctly edge-clamped value.
        {
            const int16x8_t srcLo = LOAD_SRC8(srcRowPtr, srcWidth - 16);
            const int16x8_t srcHi = LOAD_SRC8(srcRowPtr, srcWidth - 8);

            // Pair-stride tap windows for the last 4 pair centers, right edge replicated
            const int16x8_t t0 = vextq_s16(srcLo, srcHi, 4);
            const int16x8_t t1 = vextq_s16(srcLo, srcHi, 6);
            const int16x8_t t3 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(srcHi), rightEdgeShufT3));
            const int16x8_t t4 =
                vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(srcHi), rightEdgeShufT4));

            int16x8_t destA, destB;
            HORIZONTAL_FILTER_X2_UV(destA, destB, t0, t1, srcHi, t3, t4);

#if APPLY_PA
            PREDICTED_AVERAGE_UV_1D(destA, vget_low_s16(srcHi));
            PREDICTED_AVERAGE_UV_1D(destB, vget_high_s16(srcHi));
#endif

            DITHER_ADD(destA, ditherBuffer, ditherScale);
            DITHER_ADD(destB, ditherBuffer, ditherScale);

            const int32_t destX = (srcWidth - 8) * 2;
            STORE_VECTOR(destRowPtr, destX + 0, destA);
            STORE_VECTOR(destRowPtr, destX + 8, destB);
        }
    }
}

#undef STORE_VECTOR
#undef LOAD_SRC8
#ifdef NEON_DECODED_UPSCALE_LOOP
#undef NEON_DECODED_UPSCALE_LOOP
#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
#endif
