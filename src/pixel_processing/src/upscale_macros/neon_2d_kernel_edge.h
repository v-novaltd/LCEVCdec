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
// NEON upscaler kernel - shared code for filter computation
//
// This header is included multiple times with different KERNEL_VARIANT definitions
// to generate start, body, and end variants of the filter loop.
//
// KERNEL_VARIANT should be defined before including:
//   0 = body (normal load)
//   1 = start (left edge, replicate first samples)
//   2 = end (right edge, replicate last samples)

#if !defined(KERNEL_VARIANT)
#error "KERNEL_VARIANT must be defined before including this header"
#endif

#if !defined(VTBL1Q_U8_COMPAT)
#error "VTBL1Q_U8_COMPAT must be defined before including this header"
#endif

{
#if KERNEL_VARIANT == 1
    // Start: load from position 0 and replicate first sample into border positions
    // We need samples at logical positions [-2,-1,0,1,2,3,4,5] but clamp negatives to 0
    // Load [0,1,2,3,4,5,6,7] and permute to [0,0,0,1,2,3,4,5]
#if IN_FIXED_POINT == FP_U8
    const uint8x8_t src0_raw = vld1_u8(srcRow0);
    const uint8x8_t src1_raw = vld1_u8(srcRow1);
    const uint8x8_t src2_raw = vld1_u8(srcRow2);
    const uint8x8_t src3_raw = vld1_u8(srcRow3);
    const uint8x8_t src4_raw = vld1_u8(srcRow4);

    const uint8x8_t startperm = {0, 0, 0, 1, 2, 3, 4, 5};
    const int16x8_t src0 = U8ToS16(vtbl1_u8(src0_raw, startperm));
    const int16x8_t src1 = U8ToS16(vtbl1_u8(src1_raw, startperm));
    const int16x8_t src2 = U8ToS16(vtbl1_u8(src2_raw, startperm));
    const int16x8_t src3 = U8ToS16(vtbl1_u8(src3_raw, startperm));
    const int16x8_t src4 = U8ToS16(vtbl1_u8(src4_raw, startperm));
    const int32_t dstX = 0;

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(U8ToS16(src2_raw));
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const int16x8_t src0_raw = U16ToS16(vld1q_s16(srcRow0));
    const int16x8_t src1_raw = U16ToS16(vld1q_s16(srcRow1));
    const int16x8_t src2_raw = U16ToS16(vld1q_s16(srcRow2));
    const int16x8_t src3_raw = U16ToS16(vld1q_s16(srcRow3));
    const int16x8_t src4_raw = U16ToS16(vld1q_s16(srcRow4));
#else
    const int16x8_t src0_raw = vld1q_s16(srcRow0);
    const int16x8_t src1_raw = vld1q_s16(srcRow1);
    const int16x8_t src2_raw = vld1q_s16(srcRow2);
    const int16x8_t src3_raw = vld1q_s16(srcRow3);
    const int16x8_t src4_raw = vld1q_s16(srcRow4);
#endif

    // Permute using table lookup: indices [0,0,0,1,2,3,4,5] as bytes [0,1,0,1,0,1,2,3,4,5,6,7,8,9,10,11]
    const uint8x16_t startperm = {0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const int16x8_t src0 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src0_raw), startperm));
    const int16x8_t src1 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src1_raw), startperm));
    const int16x8_t src2 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src2_raw), startperm));
    const int16x8_t src3 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src3_raw), startperm));
    const int16x8_t src4 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src4_raw), startperm));
    const int32_t dstX = 0;

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(src2_raw);
#endif
#endif
#elif KERNEL_VARIANT == 2
    // End: load from safe position and replicate last sample into border positions
    // Load from bodyEnd-4 to get [w-6,w-5,w-4,w-3,w-2,w-1,w-2,w-1] where last two would be OOB
    // Permute [0,1,2,3,4,5,6,7] -> [2,3,4,5,6,7,7,7] (replicate position 7)
#if IN_FIXED_POINT == FP_U8
    const uint8x8_t src0_raw = vld1_u8(srcRow0 + srcWidth - 8);
    const uint8x8_t src1_raw = vld1_u8(srcRow1 + srcWidth - 8);
    const uint8x8_t src2_raw = vld1_u8(srcRow2 + srcWidth - 8);
    const uint8x8_t src3_raw = vld1_u8(srcRow3 + srcWidth - 8);
    const uint8x8_t src4_raw = vld1_u8(srcRow4 + srcWidth - 8);

    const uint8x8_t endperm = {2, 3, 4, 5, 6, 7, 7, 7};
    const int16x8_t src0 = U8ToS16(vtbl1_u8(src0_raw, endperm));
    const int16x8_t src1 = U8ToS16(vtbl1_u8(src1_raw, endperm));
    const int16x8_t src2 = U8ToS16(vtbl1_u8(src2_raw, endperm));
    const int16x8_t src3 = U8ToS16(vtbl1_u8(src3_raw, endperm));
    const int16x8_t src4 = U8ToS16(vtbl1_u8(src4_raw, endperm));
    const int32_t dstX = (srcWidth * 2) - 8;

    // Pick out the corresponding src elements for PA (lanes 2..5 of permuted src2)
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(vextq_s16(src2, src2, 2));
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const int16x8_t src0_raw = U16ToS16(vld1q_s16(srcRow0 + srcWidth - 8));
    const int16x8_t src1_raw = U16ToS16(vld1q_s16(srcRow1 + srcWidth - 8));
    const int16x8_t src2_raw = U16ToS16(vld1q_s16(srcRow2 + srcWidth - 8));
    const int16x8_t src3_raw = U16ToS16(vld1q_s16(srcRow3 + srcWidth - 8));
    const int16x8_t src4_raw = U16ToS16(vld1q_s16(srcRow4 + srcWidth - 8));
#else
    const int16x8_t src0_raw = vld1q_s16(srcRow0 + srcWidth - 8);
    const int16x8_t src1_raw = vld1q_s16(srcRow1 + srcWidth - 8);
    const int16x8_t src2_raw = vld1q_s16(srcRow2 + srcWidth - 8);
    const int16x8_t src3_raw = vld1q_s16(srcRow3 + srcWidth - 8);
    const int16x8_t src4_raw = vld1q_s16(srcRow4 + srcWidth - 8);
#endif

    // Permute: indices [2,3,4,5,6,7,7,7] as bytes [4,5,6,7,8,9,10,11,12,13,14,15,14,15,14,15]
    const uint8x16_t endperm = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 14, 15, 14, 15};
    const int16x8_t src0 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src0_raw), endperm));
    const int16x8_t src1 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src1_raw), endperm));
    const int16x8_t src2 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src2_raw), endperm));
    const int16x8_t src3 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src3_raw), endperm));
    const int16x8_t src4 =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src4_raw), endperm));
    const int32_t dstX = (srcWidth * 2) - 8;

    // Pick out the corresponding src elements for PA (lanes 2..5 of permuted src2)
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(vextq_s16(src2, src2, 2));
#endif
#endif
#else // KERNEL_VARIANT == 0
    // Body: normal load from srcX-2
#if IN_FIXED_POINT == FP_U8
    const int16x8_t src0 = U8ToS16(vld1_u8(srcRow0 + srcX - 2));
    const int16x8_t src1 = U8ToS16(vld1_u8(srcRow1 + srcX - 2));
    const int16x8_t src2 = U8ToS16(vld1_u8(srcRow2 + srcX - 2));
    const int16x8_t src3 = U8ToS16(vld1_u8(srcRow3 + srcX - 2));
    const int16x8_t src4 = U8ToS16(vld1_u8(srcRow4 + srcX - 2));
#elif IN_FIXED_POINT == FP_U16
    const int16x8_t src0 = U16ToS16(vld1q_s16(srcRow0 + srcX - 2));
    const int16x8_t src1 = U16ToS16(vld1q_s16(srcRow1 + srcX - 2));
    const int16x8_t src2 = U16ToS16(vld1q_s16(srcRow2 + srcX - 2));
    const int16x8_t src3 = U16ToS16(vld1q_s16(srcRow3 + srcX - 2));
    const int16x8_t src4 = U16ToS16(vld1q_s16(srcRow4 + srcX - 2));
#else // S16
    const int16x8_t src0 = vld1q_s16(srcRow0 + srcX - 2);
    const int16x8_t src1 = vld1q_s16(srcRow1 + srcX - 2);
    const int16x8_t src2 = vld1q_s16(srcRow2 + srcX - 2);
    const int16x8_t src3 = vld1q_s16(srcRow3 + srcX - 2);
    const int16x8_t src4 = vld1q_s16(srcRow4 + srcX - 2);
#endif
    const int32_t dstX = srcX * 2;

    // Load the corresponding src elements for PA
#if APPLY_PA == 1
#if IN_FIXED_POINT == FP_U8
    const int16x4_t base = U8ToS16x4(vld1_u8(srcRow2 + srcX));
#elif IN_FIXED_POINT == FP_U16
    const int16x4_t base = U16ToS16x4(vld1_s16(srcRow2 + srcX));
#else // S16
    const int16x4_t base = vld1_s16(srcRow2 + srcX);
#endif
#endif
#endif

    // === Vertical convolution for row0 src0-src3 -> v0 ===
    int32x4_t v0Lo = vmull_s16(vget_low_s16(src0), k3);
    v0Lo = vmlal_s16(v0Lo, vget_low_s16(src1), k2);
    v0Lo = vmlal_s16(v0Lo, vget_low_s16(src2), k1);
    v0Lo = vmlal_s16(v0Lo, vget_low_s16(src3), k0);

    int32x4_t v0Hi = vmull_s16(vget_high_s16(src0), k3);
    v0Hi = vmlal_s16(v0Hi, vget_high_s16(src1), k2);
    v0Hi = vmlal_s16(v0Hi, vget_high_s16(src2), k1);
    v0Hi = vmlal_s16(v0Hi, vget_high_s16(src3), k0);

    // Pack vertical results to 16-bit and clamp to S15
    int16x8_t v0 = vcombine_s16(vqrshrn_n_s32(v0Lo, 14), vqrshrn_n_s32(v0Hi, 14));
    v0 = vmaxq_s16(vminq_s16(v0, kMax16), kMin16);

    // === Vertical convolution for row1 src1-src4 -> v1 ===
    int32x4_t v1Lo = vmull_s16(vget_low_s16(src1), k0);
    v1Lo = vmlal_s16(v1Lo, vget_low_s16(src2), k1);
    v1Lo = vmlal_s16(v1Lo, vget_low_s16(src3), k2);
    v1Lo = vmlal_s16(v1Lo, vget_low_s16(src4), k3);

    int32x4_t v1Hi = vmull_s16(vget_high_s16(src1), k0);
    v1Hi = vmlal_s16(v1Hi, vget_high_s16(src2), k1);
    v1Hi = vmlal_s16(v1Hi, vget_high_s16(src3), k2);
    v1Hi = vmlal_s16(v1Hi, vget_high_s16(src4), k3);

    // Pack vertical results to 16-bit and clamp to S15
    int16x8_t v1 = vcombine_s16(vqrshrn_n_s32(v1Lo, 14), vqrshrn_n_s32(v1Hi, 14));
    v1 = vmaxq_s16(vminq_s16(v1, kMax16), kMin16);

    // === Horizontal convolution for row0 ===
    const int16x8_t v0_s1 = vextq_s16(v0, v0, 1);
    const int16x8_t v0_s2 = vextq_s16(v0, v0, 2);
    const int16x8_t v0_s3 = vextq_s16(v0, v0, 3);
    const int16x8_t v0_s4 = vextq_s16(v0, v0, 4);

    // Row 0 odd positions: h[i]*k3 + h[i+1]*k2 + h[i+2]*k1 + h[i+3]*k0
    int32x4_t row0_odd = vmull_s16(vget_low_s16(v0), k3);
    row0_odd = vmlal_s16(row0_odd, vget_low_s16(v0_s1), k2);
    row0_odd = vmlal_s16(row0_odd, vget_low_s16(v0_s2), k1);
    row0_odd = vmlal_s16(row0_odd, vget_low_s16(v0_s3), k0);

    // Row 0 even positions: h[i+1]*k0 + h[i+2]*k1 + h[i+3]*k2 + h[i+4]*k3
    int32x4_t row0_even = vmull_s16(vget_low_s16(v0_s1), k0);
    row0_even = vmlal_s16(row0_even, vget_low_s16(v0_s2), k1);
    row0_even = vmlal_s16(row0_even, vget_low_s16(v0_s3), k2);
    row0_even = vmlal_s16(row0_even, vget_low_s16(v0_s4), k3);

    // === Horizontal convolution for row1 ===
    const int16x8_t v1_s1 = vextq_s16(v1, v1, 1);
    const int16x8_t v1_s2 = vextq_s16(v1, v1, 2);
    const int16x8_t v1_s3 = vextq_s16(v1, v1, 3);
    const int16x8_t v1_s4 = vextq_s16(v1, v1, 4);

    int32x4_t row1_odd = vmull_s16(vget_low_s16(v1), k3);
    row1_odd = vmlal_s16(row1_odd, vget_low_s16(v1_s1), k2);
    row1_odd = vmlal_s16(row1_odd, vget_low_s16(v1_s2), k1);
    row1_odd = vmlal_s16(row1_odd, vget_low_s16(v1_s3), k0);

    int32x4_t row1_even = vmull_s16(vget_low_s16(v1_s1), k0);
    row1_even = vmlal_s16(row1_even, vget_low_s16(v1_s2), k1);
    row1_even = vmlal_s16(row1_even, vget_low_s16(v1_s3), k2);
    row1_even = vmlal_s16(row1_even, vget_low_s16(v1_s4), k3);

    // === Saturating pack and interleave results ===
    const int16x4_t r0_odd_16 = vqrshrn_n_s32(row0_odd, 14);
    const int16x4_t r0_even_16 = vqrshrn_n_s32(row0_even, 14);
    const int16x4_t r1_odd_16 = vqrshrn_n_s32(row1_odd, 14);
    const int16x4_t r1_even_16 = vqrshrn_n_s32(row1_even, 14);

    const int16x4x2_t row0_zip = vzip_s16(r0_odd_16, r0_even_16);
    int16x8_t row0_16 = vcombine_s16(row0_zip.val[0], row0_zip.val[1]);

    const int16x4x2_t row1_zip = vzip_s16(r1_odd_16, r1_even_16);
    int16x8_t row1_16 = vcombine_s16(row1_zip.val[0], row1_zip.val[1]);

    // === Limit to S15 range after horizontal convolution ===
    row0_16 = vmaxq_s16(vminq_s16(row0_16, kMax16), kMin16);
    row1_16 = vmaxq_s16(vminq_s16(row1_16, kMax16), kMin16);

    // === Apply PA ===
#if APPLY_PA == 1
    neonApplyPA(&row0_16, &row1_16, base);
#endif

    // === Apply dithering ===
#if DITHER == 1
    {
        uint16x8_t buffer = vld1q_u16(ditherBuffer);
        ditherBuffer += 8;

        const uint32x4_t lo = vmull_n_u16(vget_low_u16(buffer), ditherMul);

#if defined(__aarch64__)
        const uint32x4_t hi = vmull_high_n_u16(buffer, ditherMul);
        buffer = vuzp2q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
#else
        const uint32x4_t hi = vmull_n_u16(vget_high_u16(buffer), ditherMul);
        buffer = vuzpq_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi)).val[1];
#endif

        const int16x8_t d0 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer));
        row0_16 = vqaddq_s16(row0_16, vshlq_s16(d0, ditherShift));
    }

    {
        uint16x8_t buffer = vld1q_u16(ditherBuffer);
        ditherBuffer += 8;

        const uint32x4_t lo = vmull_n_u16(vget_low_u16(buffer), ditherMul);

#if defined(__aarch64__)
        const uint32x4_t hi = vmull_high_n_u16(buffer, ditherMul);
        buffer = vuzp2q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
#else
        const uint32x4_t hi = vmull_n_u16(vget_high_u16(buffer), ditherMul);
        buffer = vuzpq_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi)).val[1];
#endif

        const int16x8_t d1 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer));
        row1_16 = vqaddq_s16(row1_16, vshlq_s16(d1, ditherShift));
    }
#endif

    // === Store results ===
#if OUT_FIXED_POINT == FP_U8
    vst1_u8(dstRow0 + dstX, S16ToU8(row0_16));
    vst1_u8(dstRow1 + dstX, S16ToU8(row1_16));
#elif OUT_FIXED_POINT == FP_U16
    vst1q_s16(dstRow0 + dstX, S16ToU16(row0_16));
    vst1q_s16(dstRow1 + dstX, S16ToU16(row1_16));
#else
    vst1q_s16(dstRow0 + dstX, row0_16);
    vst1q_s16(dstRow1 + dstX, row1_16);
#endif
}

#undef KERNEL_VARIANT
