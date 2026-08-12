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
// NEON NV12 upscaler kernel (8-wide)
//
// This header is included multiple times with different KERNEL_VARIANT definitions
// to generate start, body, and end variants of the 8-wide NV12 filter loop.
//
#if !defined(KERNEL_VARIANT)
#error "KERNEL_VARIANT must be defined before including this header"
#endif

#if IN_FIXED_POINT != FP_NV12 || OUT_FIXED_POINT != FP_NV12
#error "neon_kernel_nv12.h is for FP_NV12 -> FP_NV12 only"
#endif

#if !defined(DIMENSION)
#error "DIMENSION must be defined before including this header"
#endif

#if DIMENSION != DIM_1D && DIMENSION != DIM_2D
#error "DIMENSION must be DIM_1D or DIM_2D"
#endif

{
#if KERNEL_VARIANT == 1
    const uint8x8_t startperm = {0, 1, 0, 1, 0, 1, 2, 3};

    const int16x8_t src0a = U8ToS16(vtbl1_u8(vld1_u8(srcRow0), startperm));
    const int16x8_t src1a = U8ToS16(vtbl1_u8(vld1_u8(srcRow1), startperm));
#if DIMENSION == DIM_2D
    const int16x8_t src2a = U8ToS16(vtbl1_u8(vld1_u8(srcRow2), startperm));
    const int16x8_t src3a = U8ToS16(vtbl1_u8(vld1_u8(srcRow3), startperm));
    const int16x8_t src4a = U8ToS16(vtbl1_u8(vld1_u8(srcRow4), startperm));
#endif

    const int16x8_t src0b = U8ToS16(vld1_u8(srcRow0 + 4));
    const int16x8_t src1b = U8ToS16(vld1_u8(srcRow1 + 4));
#if DIMENSION == DIM_2D
    const int16x8_t src2b = U8ToS16(vld1_u8(srcRow2 + 4));
    const int16x8_t src3b = U8ToS16(vld1_u8(srcRow3 + 4));
    const int16x8_t src4b = U8ToS16(vld1_u8(srcRow4 + 4));
#endif
    const int32_t dstX = 0;
#if APPLY_PA == 1 && DIMENSION == DIM_2D
    const uint8x8_t base_raw = vld1_u8(srcRow2);
    const uint8x8x2_t base_uv = vuzp_u8(base_raw, base_raw);
    const int16x8_t base_u = U8ToS16(base_uv.val[0]);
    const int16x8_t base_v = U8ToS16(base_uv.val[1]);
    const int16x8_t base = vcombine_s16(vget_low_s16(base_u), vget_low_s16(base_v));
#elif APPLY_PA == 1 // && DIMENSION == DIM_1D
    const uint8x8_t base0_raw = vld1_u8(srcRow0);
    const uint8x8x2_t base0_uv = vuzp_u8(base0_raw, base0_raw);
    const int16x8_t base0_u = U8ToS16(base0_uv.val[0]);
    const int16x8_t base0_v = U8ToS16(base0_uv.val[1]);
    const int16x8_t base0 = vcombine_s16(vget_low_s16(base0_u), vget_low_s16(base0_v));

    const uint8x8_t base1_raw = vld1_u8(srcRow1);
    const uint8x8x2_t base1_uv = vuzp_u8(base1_raw, base1_raw);
    const int16x8_t base1_u = U8ToS16(base1_uv.val[0]);
    const int16x8_t base1_v = U8ToS16(base1_uv.val[1]);
    const int16x8_t base1 = vcombine_s16(vget_low_s16(base1_u), vget_low_s16(base1_v));
#endif
#elif KERNEL_VARIANT == 2
    const uint8x8_t endperm = {4, 5, 6, 7, 6, 7, 6, 7};

    const int16x8_t src0a = U8ToS16(vld1_u8(srcRow0 + srcWidth - 12));
    const int16x8_t src1a = U8ToS16(vld1_u8(srcRow1 + srcWidth - 12));
#if DIMENSION == DIM_2D
    const int16x8_t src2a = U8ToS16(vld1_u8(srcRow2 + srcWidth - 12));
    const int16x8_t src3a = U8ToS16(vld1_u8(srcRow3 + srcWidth - 12));
    const int16x8_t src4a = U8ToS16(vld1_u8(srcRow4 + srcWidth - 12));
#endif

    const int16x8_t src0b = U8ToS16(vtbl1_u8(vld1_u8(srcRow0 + srcWidth - 8), endperm));
    const int16x8_t src1b = U8ToS16(vtbl1_u8(vld1_u8(srcRow1 + srcWidth - 8), endperm));
#if DIMENSION == DIM_2D
    const int16x8_t src2b = U8ToS16(vtbl1_u8(vld1_u8(srcRow2 + srcWidth - 8), endperm));
    const int16x8_t src3b = U8ToS16(vtbl1_u8(vld1_u8(srcRow3 + srcWidth - 8), endperm));
    const int16x8_t src4b = U8ToS16(vtbl1_u8(vld1_u8(srcRow4 + srcWidth - 8), endperm));
#endif
    const int32_t dstX = (srcWidth * 2) - 16;
#if APPLY_PA == 1 && DIMENSION == DIM_2D
    const uint8x8_t base_raw = vld1_u8(srcRow2 + srcWidth - 8);
    const uint8x8x2_t base_uv = vuzp_u8(base_raw, base_raw);
    const int16x8_t base_u = U8ToS16(base_uv.val[0]);
    const int16x8_t base_v = U8ToS16(base_uv.val[1]);
    const int16x8_t base = vcombine_s16(vget_low_s16(base_u), vget_low_s16(base_v));
#elif APPLY_PA == 1 // && DIMENSION == DIM_1D
    const uint8x8_t base0_raw = vld1_u8(srcRow0 + srcWidth - 8);
    const uint8x8x2_t base0_uv = vuzp_u8(base0_raw, base0_raw);
    const int16x8_t base0_u = U8ToS16(base0_uv.val[0]);
    const int16x8_t base0_v = U8ToS16(base0_uv.val[1]);
    const int16x8_t base0 = vcombine_s16(vget_low_s16(base0_u), vget_low_s16(base0_v));

    const uint8x8_t base1_raw = vld1_u8(srcRow1 + srcWidth - 8);
    const uint8x8x2_t base1_uv = vuzp_u8(base1_raw, base1_raw);
    const int16x8_t base1_u = U8ToS16(base1_uv.val[0]);
    const int16x8_t base1_v = U8ToS16(base1_uv.val[1]);
    const int16x8_t base1 = vcombine_s16(vget_low_s16(base1_u), vget_low_s16(base1_v));
#endif
#else
    // Body: normal load from srcX-4 and srcX+4.
    const int16x8_t src0a = U8ToS16(vld1_u8(srcRow0 + srcX - 4));
    const int16x8_t src1a = U8ToS16(vld1_u8(srcRow1 + srcX - 4));
#if DIMENSION == DIM_2D
    const int16x8_t src2a = U8ToS16(vld1_u8(srcRow2 + srcX - 4));
    const int16x8_t src3a = U8ToS16(vld1_u8(srcRow3 + srcX - 4));
    const int16x8_t src4a = U8ToS16(vld1_u8(srcRow4 + srcX - 4));
#endif

    const int16x8_t src0b = U8ToS16(vld1_u8(srcRow0 + srcX + 4));
    const int16x8_t src1b = U8ToS16(vld1_u8(srcRow1 + srcX + 4));
#if DIMENSION == DIM_2D
    const int16x8_t src2b = U8ToS16(vld1_u8(srcRow2 + srcX + 4));
    const int16x8_t src3b = U8ToS16(vld1_u8(srcRow3 + srcX + 4));
    const int16x8_t src4b = U8ToS16(vld1_u8(srcRow4 + srcX + 4));
#endif
    const int32_t dstX = srcX * 2;
#if APPLY_PA == 1 && DIMENSION == DIM_2D
    const uint8x8_t base_raw = vld1_u8(srcRow2 + srcX);
    const uint8x8x2_t base_uv = vuzp_u8(base_raw, base_raw);
    const int16x8_t base_u = U8ToS16(base_uv.val[0]);
    const int16x8_t base_v = U8ToS16(base_uv.val[1]);
    const int16x8_t base = vcombine_s16(vget_low_s16(base_u), vget_low_s16(base_v));
#elif APPLY_PA == 1 // && DIMENSION == DIM_1D
    const uint8x8_t base0_raw = vld1_u8(srcRow0 + srcX);
    const uint8x8x2_t base0_uv = vuzp_u8(base0_raw, base0_raw);
    const int16x8_t base0_u = U8ToS16(base0_uv.val[0]);
    const int16x8_t base0_v = U8ToS16(base0_uv.val[1]);
    const int16x8_t base0 = vcombine_s16(vget_low_s16(base0_u), vget_low_s16(base0_v));

    const uint8x8_t base1_raw = vld1_u8(srcRow1 + srcX);
    const uint8x8x2_t base1_uv = vuzp_u8(base1_raw, base1_raw);
    const int16x8_t base1_u = U8ToS16(base1_uv.val[0]);
    const int16x8_t base1_v = U8ToS16(base1_uv.val[1]);
    const int16x8_t base1 = vcombine_s16(vget_low_s16(base1_u), vget_low_s16(base1_v));
#endif
#endif

    // === Row preparation ===
#if DIMENSION == DIM_1D
    // 1D mode: skip vertical convolution, feed loaded rows directly into horizontal stage.
    int16x8_t v0a = src0a;
    int16x8_t v0b = src0b;
    int16x8_t v1a = src1a;
    int16x8_t v1b = src1b;
#else // 2D
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
#endif

    {
        // Deinterleave U,V,U,V,... lanes so each register holds a single channel.
        const int16x8x2_t v0_uv = vuzpq_s16(v0a, v0b);
        v0a = v0_uv.val[0];
        v0b = v0_uv.val[1];
        const int16x8x2_t v1_uv = vuzpq_s16(v1a, v1b);
        v1a = v1_uv.val[0];
        v1b = v1_uv.val[1];
    }

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
#if APPLY_PA == 1 && DIMENSION == DIM_2D
    /* avg = base - ((row0_pel_even + row0_pel_odd + row1_pel_even + row1_pel_odd + 2) >> 2) */
    const int32x4_t tmp0 = vpaddlq_s16(row0a_16);
    const int32x4_t tmp1 = vpaddlq_s16(row0b_16);
    const int32x4_t tmp2 = vpaddlq_s16(row1a_16);
    const int32x4_t tmp3 = vpaddlq_s16(row1b_16);
    const int32x4_t sum0 = vaddq_s32(tmp0, tmp2);
    const int32x4_t sum1 = vaddq_s32(tmp1, tmp3);
    const int16x8_t sum = vcombine_s16(vrshrn_n_s32(sum0, 2), vrshrn_n_s32(sum1, 2));
    const int16x8_t avg = vsubq_s16(base, sum);

    /* Repeat each avg to apply to values. */
    const int16x8x2_t broadcast = vzipq_s16(avg, avg);
    row0a_16 = vqaddq_s16(row0a_16, broadcast.val[0]);
    row0b_16 = vqaddq_s16(row0b_16, broadcast.val[1]);
    row1a_16 = vqaddq_s16(row1a_16, broadcast.val[0]);
    row1b_16 = vqaddq_s16(row1b_16, broadcast.val[1]);
#elif APPLY_PA == 1 // && DIMENSION == DIM_1D
    // 1D PA: apply independently per row, with per-row NV12 base values.
    const int16x4_t base0u = vget_low_s16(base0);
    const int16x4_t base0v = vget_high_s16(base0);
    const int16x4_t mean0u = vrshrn_n_s32(vpaddlq_s16(row0a_16), 1);
    const int16x4_t mean0v = vrshrn_n_s32(vpaddlq_s16(row0b_16), 1);
    const int16x4_t avg0u = vsub_s16(base0u, mean0u);
    const int16x4_t avg0v = vsub_s16(base0v, mean0v);
    const int16x8_t avg0u8 = vcombine_s16(avg0u, avg0u);
    const int16x8_t avg0v8 = vcombine_s16(avg0v, avg0v);
    row0a_16 = vqaddq_s16(row0a_16, vzipq_s16(avg0u8, avg0u8).val[0]);
    row0b_16 = vqaddq_s16(row0b_16, vzipq_s16(avg0v8, avg0v8).val[0]);

    const int16x4_t base1u = vget_low_s16(base1);
    const int16x4_t base1v = vget_high_s16(base1);
    const int16x4_t mean1u = vrshrn_n_s32(vpaddlq_s16(row1a_16), 1);
    const int16x4_t mean1v = vrshrn_n_s32(vpaddlq_s16(row1b_16), 1);
    const int16x4_t avg1u = vsub_s16(base1u, mean1u);
    const int16x4_t avg1v = vsub_s16(base1v, mean1v);
    const int16x8_t avg1u8 = vcombine_s16(avg1u, avg1u);
    const int16x8_t avg1v8 = vcombine_s16(avg1v, avg1v);
    row1a_16 = vqaddq_s16(row1a_16, vzipq_s16(avg1u8, avg1u8).val[0]);
    row1b_16 = vqaddq_s16(row1b_16, vzipq_s16(avg1v8, avg1v8).val[0]);
#endif

    {
        // Re-interleave NV12
        const int16x8x2_t row0_uv = vzipq_s16(row0a_16, row0b_16);
        row0a_16 = row0_uv.val[0];
        row0b_16 = row0_uv.val[1];

        const int16x8x2_t row1_uv = vzipq_s16(row1a_16, row1b_16);
        row1a_16 = row1_uv.val[0];
        row1b_16 = row1_uv.val[1];
    }

    // === Store results ===
    const uint8x16_t row0_u8 = vcombine_u8(S16ToU8(row0a_16), S16ToU8(row0b_16));
    const uint8x16_t row1_u8 = vcombine_u8(S16ToU8(row1a_16), S16ToU8(row1b_16));
    vst1q_u8(dstRow0 + dstX, row0_u8);
    vst1q_u8(dstRow1 + dstX, row1_u8);
}

#undef KERNEL_VARIANT
