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
    const uint8x8_t src_raw = vld1_u8(srcRow);

    const uint8x8_t startperm = {0, 0, 0, 1, 2, 3, 4, 5};
    const int16x8_t srcPel = U8ToS16(vtbl1_u8(src_raw, startperm));

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(U8ToS16(src_raw));
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const int16x8_t src_raw = U16ToS16(vld1q_s16(srcRow));
#else
    const int16x8_t src_raw = vld1q_s16(srcRow);
#endif

    // Permute using table lookup: indices [0,0,0,1,2,3,4,5] as bytes [0,1,0,1,0,1,2,3,4,5,6,7,8,9,10,11]
    const uint8x16_t startperm = {0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const int16x8_t srcPel =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src_raw), startperm));

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(src_raw);
#endif
#endif
    const int32_t dstX = 0;

#elif KERNEL_VARIANT == 2
    // End: load from safe position and replicate last sample into border positions
    // Load from bodyEnd-4 to get [w-6,w-5,w-4,w-3,w-2,w-1,w-2,w-1] where last two would be OOB
    // Permute [0,1,2,3,4,5,6,7] -> [2,3,4,5,6,7,7,7] (replicate position 7)
#if IN_FIXED_POINT == FP_U8
    const uint8x8_t src_raw = vld1_u8(srcRow + srcWidth - 8);

    const uint8x8_t endperm = {2, 3, 4, 5, 6, 7, 7, 7};
    const int16x8_t srcPel = U8ToS16(vtbl1_u8(src_raw, endperm));

#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(vextq_s16(srcPel, srcPel, 2));
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const int16x8_t src_raw = U16ToS16(vld1q_s16(srcRow + srcWidth - 8));
#else
    const int16x8_t src_raw = vld1q_s16(srcRow + srcWidth - 8);
#endif

    // Permute: indices [2,3,4,5,6,7,7,7] as bytes [4,5,6,7,8,9,10,11,12,13,14,15,14,15,14,15]
    const uint8x16_t endperm = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 14, 15, 14, 15};
    const int16x8_t srcPel =
        vreinterpretq_s16_u8(VTBL1Q_U8_COMPAT(vreinterpretq_u8_s16(src_raw), endperm));

#if APPLY_PA == 1
    const int16x4_t base = vget_low_s16(vextq_s16(srcPel, srcPel, 2));
#endif
#endif
    const int32_t dstX = (srcWidth * 2) - 8;

#else // KERNEL_VARIANT 0
    // Body: normal load from srcX-2
#if IN_FIXED_POINT == FP_U8
    const int16x8_t srcPel = U8ToS16(vld1_u8(srcRow + srcX - 2));
#elif IN_FIXED_POINT == FP_U16
    const int16x8_t srcPel = U16ToS16(vld1q_s16(srcRow + srcX - 2));
#else // S16
    const int16x8_t srcPel = vld1q_s16(srcRow + srcX - 2);
#endif
    const int32_t dstX = srcX * 2;

    // Load the corresponding src elements for PA
#if APPLY_PA == 1
#if IN_FIXED_POINT == FP_U8
    const int16x4_t base = U8ToS16x4(vld1_u8(srcRow + srcX));
#elif IN_FIXED_POINT == FP_U16
    const int16x4_t base = U16ToS16x4(vld1_s16(srcRow + srcX));
#else // S16
    const int16x4_t base = vld1_s16(srcRow + srcX);
#endif
#endif
#endif

    // === Horizontal convolution ===
    const int16x8_t v0_s1 = vextq_s16(srcPel, srcPel, 1);
    const int16x8_t v0_s2 = vextq_s16(srcPel, srcPel, 2);
    const int16x8_t v0_s3 = vextq_s16(srcPel, srcPel, 3);
    const int16x8_t v0_s4 = vextq_s16(srcPel, srcPel, 4);

    // Row 0 odd positions: h[i]*k3 + h[i+1]*k2 + h[i+2]*k1 + h[i+3]*k0
    int32x4_t row_odd = vmull_s16(vget_low_s16(srcPel), k3);
    row_odd = vmlal_s16(row_odd, vget_low_s16(v0_s1), k2);
    row_odd = vmlal_s16(row_odd, vget_low_s16(v0_s2), k1);
    row_odd = vmlal_s16(row_odd, vget_low_s16(v0_s3), k0);

    // Row 0 even positions: h[i+1]*k0 + h[i+2]*k1 + h[i+3]*k2 + h[i+4]*k3
    int32x4_t row_even = vmull_s16(vget_low_s16(v0_s1), k0);
    row_even = vmlal_s16(row_even, vget_low_s16(v0_s2), k1);
    row_even = vmlal_s16(row_even, vget_low_s16(v0_s3), k2);
    row_even = vmlal_s16(row_even, vget_low_s16(v0_s4), k3);

    // === Saturating pack and interleave results ===
    // Cubic filter overshoot can exceed S16, so saturate before the subsequent S15 clamp.
    const int16x4_t r0_odd_16 = vqrshrn_n_s32(row_odd, 14);
    const int16x4_t r0_even_16 = vqrshrn_n_s32(row_even, 14);

    const int16x4x2_t row_zip = vzip_s16(r0_odd_16, r0_even_16);
    int16x8_t row_16 = vcombine_s16(row_zip.val[0], row_zip.val[1]);

    // === Limit to S15 range after horizontal convolution ===
    row_16 = vmaxq_s16(vminq_s16(row_16, kMax16), kMin16);

    // === Apply PA ===
#if APPLY_PA == 1
    const int32x4_t sum = vpaddlq_s16(row_16);
    const int32x4_t mean = vrshrq_n_s32(sum, 1);
    const int32x4_t adjust = vsubq_s32(vmovl_s16(base), mean);

    const int32x4x2_t broadcast = vzipq_s32(adjust, adjust);
    const int32x4_t rowLo = vaddq_s32(vmovl_s16(vget_low_s16(row_16)), broadcast.val[0]);
    const int32x4_t rowHi = vaddq_s32(vmovl_s16(vget_high_s16(row_16)), broadcast.val[1]);

    row_16 = vcombine_s16(vqmovn_s32(rowLo), vqmovn_s32(rowHi));
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
        row_16 = vqaddq_s16(row_16, vshlq_s16(d0, ditherShift));
    }
#endif

    // === Store results ===
#if OUT_FIXED_POINT == FP_U8
    vst1_u8(dstRow + dstX, S16ToU8(row_16));
#elif OUT_FIXED_POINT == FP_U16
    vst1q_s16(dstRow + dstX, S16ToU16(row_16));
#else
    vst1q_s16(dstRow + dstX, row_16);
#endif
}

#undef KERNEL_VARIANT
