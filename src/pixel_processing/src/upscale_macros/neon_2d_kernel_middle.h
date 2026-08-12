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
// NEON upscaler kernel (8-wide) - body loop only
//
// This header implements the 8-wide body loop that processes 8 source pixels
// producing 16 output pixels per iteration. Edge handling uses the 4-wide kernel.
//
{
    // Body: normal load from srcX-2 and srcX+2
#if IN_FIXED_POINT == FP_U8
    const int16x8_t src0a = U8ToS16(vld1_u8(srcRow0 + srcX - 2));
    const int16x8_t src1a = U8ToS16(vld1_u8(srcRow1 + srcX - 2));
    const int16x8_t src2a = U8ToS16(vld1_u8(srcRow2 + srcX - 2));
    const int16x8_t src3a = U8ToS16(vld1_u8(srcRow3 + srcX - 2));
    const int16x8_t src4a = U8ToS16(vld1_u8(srcRow4 + srcX - 2));

    const int16x8_t src0b = U8ToS16(vld1_u8(srcRow0 + srcX + 2));
    const int16x8_t src1b = U8ToS16(vld1_u8(srcRow1 + srcX + 2));
    const int16x8_t src2b = U8ToS16(vld1_u8(srcRow2 + srcX + 2));
    const int16x8_t src3b = U8ToS16(vld1_u8(srcRow3 + srcX + 2));
    const int16x8_t src4b = U8ToS16(vld1_u8(srcRow4 + srcX + 2));
#elif IN_FIXED_POINT == FP_U16
    const int16x8_t src0a = U16ToS16(vld1q_s16(srcRow0 + srcX - 2));
    const int16x8_t src1a = U16ToS16(vld1q_s16(srcRow1 + srcX - 2));
    const int16x8_t src2a = U16ToS16(vld1q_s16(srcRow2 + srcX - 2));
    const int16x8_t src3a = U16ToS16(vld1q_s16(srcRow3 + srcX - 2));
    const int16x8_t src4a = U16ToS16(vld1q_s16(srcRow4 + srcX - 2));

    const int16x8_t src0b = U16ToS16(vld1q_s16(srcRow0 + srcX + 2));
    const int16x8_t src1b = U16ToS16(vld1q_s16(srcRow1 + srcX + 2));
    const int16x8_t src2b = U16ToS16(vld1q_s16(srcRow2 + srcX + 2));
    const int16x8_t src3b = U16ToS16(vld1q_s16(srcRow3 + srcX + 2));
    const int16x8_t src4b = U16ToS16(vld1q_s16(srcRow4 + srcX + 2));
#else // S16
    const int16x8_t src0a = vld1q_s16(srcRow0 + srcX - 2);
    const int16x8_t src1a = vld1q_s16(srcRow1 + srcX - 2);
    const int16x8_t src2a = vld1q_s16(srcRow2 + srcX - 2);
    const int16x8_t src3a = vld1q_s16(srcRow3 + srcX - 2);
    const int16x8_t src4a = vld1q_s16(srcRow4 + srcX - 2);

    const int16x8_t src0b = vld1q_s16(srcRow0 + srcX + 2);
    const int16x8_t src1b = vld1q_s16(srcRow1 + srcX + 2);
    const int16x8_t src2b = vld1q_s16(srcRow2 + srcX + 2);
    const int16x8_t src3b = vld1q_s16(srcRow3 + srcX + 2);
    const int16x8_t src4b = vld1q_s16(srcRow4 + srcX + 2);
#endif

    const int32_t dstX = srcX * 2;

    // === Vertical convolution for row0 src0-src3 -> v0a, v0b ===
    int32x4_t v0aLo = vmull_s16(vget_low_s16(src0a), k3);
    v0aLo = vmlal_s16(v0aLo, vget_low_s16(src1a), k2);
    v0aLo = vmlal_s16(v0aLo, vget_low_s16(src2a), k1);
    v0aLo = vmlal_s16(v0aLo, vget_low_s16(src3a), k0);

    int32x4_t v0aHi = vmull_s16(vget_high_s16(src0a), k3);
    v0aHi = vmlal_s16(v0aHi, vget_high_s16(src1a), k2);
    v0aHi = vmlal_s16(v0aHi, vget_high_s16(src2a), k1);
    v0aHi = vmlal_s16(v0aHi, vget_high_s16(src3a), k0);

    int16x8_t v0a = vcombine_s16(vqrshrn_n_s32(v0aLo, 14), vqrshrn_n_s32(v0aHi, 14));
    v0a = vmaxq_s16(vminq_s16(v0a, kMax16), kMin16); // limit to S15 range

    int32x4_t v0bLo = vmull_s16(vget_low_s16(src0b), k3);
    v0bLo = vmlal_s16(v0bLo, vget_low_s16(src1b), k2);
    v0bLo = vmlal_s16(v0bLo, vget_low_s16(src2b), k1);
    v0bLo = vmlal_s16(v0bLo, vget_low_s16(src3b), k0);

    int32x4_t v0bHi = vmull_s16(vget_high_s16(src0b), k3);
    v0bHi = vmlal_s16(v0bHi, vget_high_s16(src1b), k2);
    v0bHi = vmlal_s16(v0bHi, vget_high_s16(src2b), k1);
    v0bHi = vmlal_s16(v0bHi, vget_high_s16(src3b), k0);

    int16x8_t v0b = vcombine_s16(vqrshrn_n_s32(v0bLo, 14), vqrshrn_n_s32(v0bHi, 14));
    v0b = vmaxq_s16(vminq_s16(v0b, kMax16), kMin16); // limit to S15 range

    // === Vertical convolution for row1 src1-src4 -> v1a, v1b ===
    int32x4_t v1aLo = vmull_s16(vget_low_s16(src1a), k0);
    v1aLo = vmlal_s16(v1aLo, vget_low_s16(src2a), k1);
    v1aLo = vmlal_s16(v1aLo, vget_low_s16(src3a), k2);
    v1aLo = vmlal_s16(v1aLo, vget_low_s16(src4a), k3);

    int32x4_t v1aHi = vmull_s16(vget_high_s16(src1a), k0);
    v1aHi = vmlal_s16(v1aHi, vget_high_s16(src2a), k1);
    v1aHi = vmlal_s16(v1aHi, vget_high_s16(src3a), k2);
    v1aHi = vmlal_s16(v1aHi, vget_high_s16(src4a), k3);

    int16x8_t v1a = vcombine_s16(vqrshrn_n_s32(v1aLo, 14), vqrshrn_n_s32(v1aHi, 14));
    v1a = vmaxq_s16(vminq_s16(v1a, kMax16), kMin16); // limit to S15 range

    int32x4_t v1bLo = vmull_s16(vget_low_s16(src1b), k0);
    v1bLo = vmlal_s16(v1bLo, vget_low_s16(src2b), k1);
    v1bLo = vmlal_s16(v1bLo, vget_low_s16(src3b), k2);
    v1bLo = vmlal_s16(v1bLo, vget_low_s16(src4b), k3);

    int32x4_t v1bHi = vmull_s16(vget_high_s16(src1b), k0);
    v1bHi = vmlal_s16(v1bHi, vget_high_s16(src2b), k1);
    v1bHi = vmlal_s16(v1bHi, vget_high_s16(src3b), k2);
    v1bHi = vmlal_s16(v1bHi, vget_high_s16(src4b), k3);

    int16x8_t v1b = vcombine_s16(vqrshrn_n_s32(v1bLo, 14), vqrshrn_n_s32(v1bHi, 14));
    v1b = vmaxq_s16(vminq_s16(v1b, kMax16), kMin16); // limit to S15 range

    // === Horizontal convolution for row0 ===
    const int16x4_t v0a_s0 = vget_low_s16(v0a);
    const int16x4_t v0a_s1 = vget_low_s16(vextq_s16(v0a, v0a, 1));
    const int16x4_t v0a_s2 = vget_low_s16(vextq_s16(v0a, v0a, 2));
    const int16x4_t v0a_s3 = vget_low_s16(vextq_s16(v0a, v0a, 3));
    const int16x4_t v0a_s4 = vget_low_s16(vextq_s16(v0a, v0a, 4));

    int32x4_t row0a_odd = vmull_s16(v0a_s0, k3);
    row0a_odd = vmlal_s16(row0a_odd, v0a_s1, k2);
    row0a_odd = vmlal_s16(row0a_odd, v0a_s2, k1);
    row0a_odd = vmlal_s16(row0a_odd, v0a_s3, k0);

    int32x4_t row0a_even = vmull_s16(v0a_s1, k0);
    row0a_even = vmlal_s16(row0a_even, v0a_s2, k1);
    row0a_even = vmlal_s16(row0a_even, v0a_s3, k2);
    row0a_even = vmlal_s16(row0a_even, v0a_s4, k3);

    const int16x4_t v0b_s0 = vget_low_s16(v0b);
    const int16x4_t v0b_s1 = vget_low_s16(vextq_s16(v0b, v0b, 1));
    const int16x4_t v0b_s2 = vget_low_s16(vextq_s16(v0b, v0b, 2));
    const int16x4_t v0b_s3 = vget_low_s16(vextq_s16(v0b, v0b, 3));
    const int16x4_t v0b_s4 = vget_low_s16(vextq_s16(v0b, v0b, 4));

    int32x4_t row0b_odd = vmull_s16(v0b_s0, k3);
    row0b_odd = vmlal_s16(row0b_odd, v0b_s1, k2);
    row0b_odd = vmlal_s16(row0b_odd, v0b_s2, k1);
    row0b_odd = vmlal_s16(row0b_odd, v0b_s3, k0);

    int32x4_t row0b_even = vmull_s16(v0b_s1, k0);
    row0b_even = vmlal_s16(row0b_even, v0b_s2, k1);
    row0b_even = vmlal_s16(row0b_even, v0b_s3, k2);
    row0b_even = vmlal_s16(row0b_even, v0b_s4, k3);

    // === Horizontal convolution for row1 ===
    const int16x4_t v1a_s0 = vget_low_s16(v1a);
    const int16x4_t v1a_s1 = vget_low_s16(vextq_s16(v1a, v1a, 1));
    const int16x4_t v1a_s2 = vget_low_s16(vextq_s16(v1a, v1a, 2));
    const int16x4_t v1a_s3 = vget_low_s16(vextq_s16(v1a, v1a, 3));
    const int16x4_t v1a_s4 = vget_low_s16(vextq_s16(v1a, v1a, 4));

    int32x4_t row1a_odd = vmull_s16(v1a_s0, k3);
    row1a_odd = vmlal_s16(row1a_odd, v1a_s1, k2);
    row1a_odd = vmlal_s16(row1a_odd, v1a_s2, k1);
    row1a_odd = vmlal_s16(row1a_odd, v1a_s3, k0);

    int32x4_t row1a_even = vmull_s16(v1a_s1, k0);
    row1a_even = vmlal_s16(row1a_even, v1a_s2, k1);
    row1a_even = vmlal_s16(row1a_even, v1a_s3, k2);
    row1a_even = vmlal_s16(row1a_even, v1a_s4, k3);

    const int16x4_t v1b_s0 = vget_low_s16(v1b);
    const int16x4_t v1b_s1 = vget_low_s16(vextq_s16(v1b, v1b, 1));
    const int16x4_t v1b_s2 = vget_low_s16(vextq_s16(v1b, v1b, 2));
    const int16x4_t v1b_s3 = vget_low_s16(vextq_s16(v1b, v1b, 3));
    const int16x4_t v1b_s4 = vget_low_s16(vextq_s16(v1b, v1b, 4));

    int32x4_t row1b_odd = vmull_s16(v1b_s0, k3);
    row1b_odd = vmlal_s16(row1b_odd, v1b_s1, k2);
    row1b_odd = vmlal_s16(row1b_odd, v1b_s2, k1);
    row1b_odd = vmlal_s16(row1b_odd, v1b_s3, k0);

    int32x4_t row1b_even = vmull_s16(v1b_s1, k0);
    row1b_even = vmlal_s16(row1b_even, v1b_s2, k1);
    row1b_even = vmlal_s16(row1b_even, v1b_s3, k2);
    row1b_even = vmlal_s16(row1b_even, v1b_s4, k3);

    // === Saturating pack and interleave results ===
    const int16x4_t row0a_odd_16 = vqrshrn_n_s32(row0a_odd, 14);
    const int16x4_t row0a_even_16 = vqrshrn_n_s32(row0a_even, 14);
    const int16x4_t row0b_odd_16 = vqrshrn_n_s32(row0b_odd, 14);
    const int16x4_t row0b_even_16 = vqrshrn_n_s32(row0b_even, 14);

    const int16x4_t row1a_odd_16 = vqrshrn_n_s32(row1a_odd, 14);
    const int16x4_t row1a_even_16 = vqrshrn_n_s32(row1a_even, 14);
    const int16x4_t row1b_odd_16 = vqrshrn_n_s32(row1b_odd, 14);
    const int16x4_t row1b_even_16 = vqrshrn_n_s32(row1b_even, 14);

    const int16x4x2_t row0a_zip = vzip_s16(row0a_odd_16, row0a_even_16);
    int16x8_t row0a_16 = vcombine_s16(row0a_zip.val[0], row0a_zip.val[1]);

    const int16x4x2_t row0b_zip = vzip_s16(row0b_odd_16, row0b_even_16);
    int16x8_t row0b_16 = vcombine_s16(row0b_zip.val[0], row0b_zip.val[1]);

    const int16x4x2_t row1a_zip = vzip_s16(row1a_odd_16, row1a_even_16);
    int16x8_t row1a_16 = vcombine_s16(row1a_zip.val[0], row1a_zip.val[1]);

    const int16x4x2_t row1b_zip = vzip_s16(row1b_odd_16, row1b_even_16);
    int16x8_t row1b_16 = vcombine_s16(row1b_zip.val[0], row1b_zip.val[1]);

    // === Limit to S15 range after horizontal convolution ===
    row0a_16 = vmaxq_s16(vminq_s16(row0a_16, kMax16), kMin16);
    row0b_16 = vmaxq_s16(vminq_s16(row0b_16, kMax16), kMin16);
    row1a_16 = vmaxq_s16(vminq_s16(row1a_16, kMax16), kMin16);
    row1b_16 = vmaxq_s16(vminq_s16(row1b_16, kMax16), kMin16);

    // === Apply PA ===
#if APPLY_PA == 1
    // Load base pixel values with fixed point conversion
#if IN_FIXED_POINT == FP_U8
    const int16x8_t base = U8ToS16(vld1_u8(srcRow2 + srcX));
#elif IN_FIXED_POINT == FP_U16
    const int16x8_t base = U16ToS16(vld1q_s16(srcRow2 + srcX));
#else
    const int16x8_t base = vld1q_s16(srcRow2 + srcX);
#endif

    neonApplyPA(&row0a_16, &row1a_16, vget_low_s16(base));
    neonApplyPA(&row0b_16, &row1b_16, vget_high_s16(base));
#endif

    // === Apply dithering ===
#if DITHER == 1
    /* ---- Row 0: apply to (row0a_16, row0b_16) ---- */
    {
        // Load 16 u16 (interleaved into 2 vectors) and advance pointer
        uint16x8x2_t buffer = vld2q_u16(ditherBuffer);
        ditherBuffer += 16;

        // Multiply by scalar (u16*u16 -> u32)
        uint32x4x2_t lo;
        lo.val[0] = vmull_n_u16(vget_low_u16(buffer.val[0]), ditherMul);
        lo.val[1] = vmull_n_u16(vget_low_u16(buffer.val[1]), ditherMul);

#if defined(__aarch64__)
        uint32x4_t hi0 = vmull_high_n_u16(buffer.val[0], ditherMul);
        uint32x4_t hi1 = vmull_high_n_u16(buffer.val[1], ditherMul);

        // Pack: take high 16 bits of each 32-bit product (matches original vuzp2 path)
        buffer.val[0] = vuzp2q_u16(vreinterpretq_u16_u32(lo.val[0]), vreinterpretq_u16_u32(hi0));
        buffer.val[1] = vuzp2q_u16(vreinterpretq_u16_u32(lo.val[1]), vreinterpretq_u16_u32(hi1));
#else
        uint32x4x2_t hi;
        hi.val[0] = vmull_n_u16(vget_high_u16(buffer.val[0]), ditherMul);
        hi.val[1] = vmull_n_u16(vget_high_u16(buffer.val[1]), ditherMul);

        buffer.val[0] =
            vuzpq_u16(vreinterpretq_u16_u32(lo.val[0]), vreinterpretq_u16_u32(hi.val[0])).val[1];
        buffer.val[1] =
            vuzpq_u16(vreinterpretq_u16_u32(lo.val[1]), vreinterpretq_u16_u32(hi.val[1])).val[1];
#endif

        // Convert to signed range [-strength, +strength] via (ditherOffset - dither), saturating
        const int16x8_t d0 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer.val[0]));
        const int16x8_t d1 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer.val[1]));

        // Shift and add (saturating)
        row0a_16 = vqaddq_s16(row0a_16, vshlq_s16(d0, ditherShift));
        row0b_16 = vqaddq_s16(row0b_16, vshlq_s16(d1, ditherShift));
    }

    /* ---- Row 1: apply to (row1a_16, row1b_16) ---- */
    {
        uint16x8x2_t buffer = vld2q_u16(ditherBuffer);
        ditherBuffer += 16;

        uint32x4x2_t lo;
        lo.val[0] = vmull_n_u16(vget_low_u16(buffer.val[0]), ditherMul);
        lo.val[1] = vmull_n_u16(vget_low_u16(buffer.val[1]), ditherMul);

#if defined(__aarch64__)
        uint32x4_t hi0 = vmull_high_n_u16(buffer.val[0], ditherMul);
        uint32x4_t hi1 = vmull_high_n_u16(buffer.val[1], ditherMul);

        buffer.val[0] = vuzp2q_u16(vreinterpretq_u16_u32(lo.val[0]), vreinterpretq_u16_u32(hi0));
        buffer.val[1] = vuzp2q_u16(vreinterpretq_u16_u32(lo.val[1]), vreinterpretq_u16_u32(hi1));
#else
        uint32x4x2_t hi;
        hi.val[0] = vmull_n_u16(vget_high_u16(buffer.val[0]), ditherMul);
        hi.val[1] = vmull_n_u16(vget_high_u16(buffer.val[1]), ditherMul);

        buffer.val[0] =
            vuzpq_u16(vreinterpretq_u16_u32(lo.val[0]), vreinterpretq_u16_u32(hi.val[0])).val[1];
        buffer.val[1] =
            vuzpq_u16(vreinterpretq_u16_u32(lo.val[1]), vreinterpretq_u16_u32(hi.val[1])).val[1];
#endif

        const int16x8_t d0 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer.val[0]));
        const int16x8_t d1 = vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer.val[1]));

        row1a_16 = vqaddq_s16(row1a_16, vshlq_s16(d0, ditherShift));
        row1b_16 = vqaddq_s16(row1b_16, vshlq_s16(d1, ditherShift));
    }
#endif

    // === Store results ===
#if OUT_FIXED_POINT == FP_U8
    const uint8x16_t row0_u8 = vcombine_u8(S16ToU8(row0a_16), S16ToU8(row0b_16));
    const uint8x16_t row1_u8 = vcombine_u8(S16ToU8(row1a_16), S16ToU8(row1b_16));
    vst1q_u8(dstRow0 + dstX, row0_u8);
    vst1q_u8(dstRow1 + dstX, row1_u8);
#elif OUT_FIXED_POINT == FP_U16
#if defined(__aarch64__)
    const int16x8x2_t row0_u16 = {{S16ToU16(row0a_16), S16ToU16(row0b_16)}};
    const int16x8x2_t row1_u16 = {{S16ToU16(row1a_16), S16ToU16(row1b_16)}};
    vst1q_s16_x2(dstRow0 + dstX, row0_u16);
    vst1q_s16_x2(dstRow1 + dstX, row1_u16);
#else
    vst1q_s16(dstRow0 + dstX, S16ToU16(row0a_16));
    vst1q_s16(dstRow0 + dstX + 8, S16ToU16(row0b_16));
    vst1q_s16(dstRow1 + dstX, S16ToU16(row1a_16));
    vst1q_s16(dstRow1 + dstX + 8, S16ToU16(row1b_16));
#endif
#else // S16
#if defined(__aarch64__)
    const int16x8x2_t row0_s16 = {{row0a_16, row0b_16}};
    const int16x8x2_t row1_s16 = {{row1a_16, row1b_16}};
    vst1q_s16_x2(dstRow0 + dstX, row0_s16);
    vst1q_s16_x2(dstRow1 + dstX, row1_s16);
#else
    vst1q_s16(dstRow0 + dstX, row0a_16);
    vst1q_s16(dstRow0 + dstX + 8, row0b_16);
    vst1q_s16(dstRow1 + dstX, row1a_16);
    vst1q_s16(dstRow1 + dstX + 8, row1b_16);
#endif
#endif
}
