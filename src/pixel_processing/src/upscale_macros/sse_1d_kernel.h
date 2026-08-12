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
// SSE upscaler kernel - shared code for filter computation
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

{
#if KERNEL_VARIANT == 1
    // Start: load from position 0 and replicate first sample into border positions
    // We need samples at logical positions [-2,-1,0,1,2,3,4,5] but clamp negatives to 0
    // Load [0,1,2,3,4,5,6,7] and permute to [0,0,0,1,2,3,4,5]
#if IN_FIXED_POINT == FP_U8
    const __m128i srcRaw = _mm_loadl_epi64((const __m128i*)srcRow);
    const __m128i srcPel = U8ToS16(_mm_shuffle_epi8(srcRaw, kLeftEdgeShuffleU8));

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const __m128i base = U8ToS16(srcRaw);
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const __m128i srcRaw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow));
#else
    const __m128i srcRaw = _mm_loadu_si128((const __m128i*)srcRow);
#endif

    const __m128i srcPel = _mm_shuffle_epi8(srcRaw, kLeftEdgeShuffleS16);

    // Pick out the corresponding src elements for PA
#if APPLY_PA == 1
    const __m128i base = srcRaw;
#endif
#endif
    const int32_t dstX = 0;

#elif KERNEL_VARIANT == 2
    // End: load from safe position and replicate last sample into border positions
    // Load from srcWidth-8 and permute [0,1,2,3,4,5,6,7] -> [2,3,4,5,6,7,7,7]
    const int32_t loadPos = (int32_t)srcWidth - 8;

#if IN_FIXED_POINT == FP_U8
    const __m128i srcRaw = _mm_loadl_epi64((const __m128i*)(srcRow + loadPos));
    const __m128i srcPel = U8ToS16(_mm_shuffle_epi8(srcRaw, kRightEdgeShuffleU8));

#if APPLY_PA == 1
    const __m128i base = _mm_srli_si128(srcPel, 4);
#endif
#else // N16
#if IN_FIXED_POINT == FP_U16
    const __m128i srcRaw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow + loadPos)));
#else
    const __m128i srcRaw = _mm_loadu_si128((const __m128i*)(srcRow + loadPos));
#endif

    const __m128i srcPel = _mm_shuffle_epi8(srcRaw, kRightEdgeShuffleS16);

#if APPLY_PA == 1
    const __m128i base = _mm_srli_si128(srcPel, 4);
#endif
#endif
    const int32_t dstX = (((int32_t)srcWidth) * 2) - 8;

#else // KERNEL_VARIANT 0
    // Body: normal load from srcX-2
#if IN_FIXED_POINT == FP_U8
    const __m128i srcPel = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow + srcX - 2)));
#elif IN_FIXED_POINT == FP_U16
    const __m128i srcPel = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow + srcX - 2)));
#else // S16
    const __m128i srcPel = _mm_loadu_si128((const __m128i*)(srcRow + srcX - 2));
#endif
    const int32_t dstX = srcX * 2;

    // Load the corresponding src elements for PA
#if APPLY_PA == 1
#if IN_FIXED_POINT == FP_U8
    const __m128i base = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow + srcX)));
#elif IN_FIXED_POINT == FP_U16
    const __m128i base = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow + srcX)));
#else // S16
    const __m128i base = _mm_loadu_si128((const __m128i*)(srcRow + srcX));
#endif
#endif
#endif

    // === Horizontal convolution ===
    const __m128i src1 = _mm_srli_si128(srcPel, 2);
    const __m128i src2 = _mm_srli_si128(srcPel, 4);
    const __m128i src3 = _mm_srli_si128(srcPel, 6);
    const __m128i src4 = _mm_srli_si128(srcPel, 8);

    const __m128i x0 = _mm_cvtepi16_epi32(srcPel);
    const __m128i x1 = _mm_cvtepi16_epi32(src1);
    const __m128i x2 = _mm_cvtepi16_epi32(src2);
    const __m128i x3 = _mm_cvtepi16_epi32(src3);
    const __m128i x4 = _mm_cvtepi16_epi32(src4);

    // Row 0 odd positions: h[i]*k3 + h[i+1]*k2 + h[i+2]*k1 + h[i+3]*k0
    __m128i rowOdd = _mm_mullo_epi32(x0, k3);
    rowOdd = _mm_add_epi32(rowOdd, _mm_mullo_epi32(x1, k2));
    rowOdd = _mm_add_epi32(rowOdd, _mm_mullo_epi32(x2, k1));
    rowOdd = _mm_add_epi32(rowOdd, _mm_mullo_epi32(x3, k0));

    // Row 0 even positions: h[i+1]*k0 + h[i+2]*k1 + h[i+3]*k2 + h[i+4]*k3
    __m128i rowEven = _mm_mullo_epi32(x1, k0);
    rowEven = _mm_add_epi32(rowEven, _mm_mullo_epi32(x2, k1));
    rowEven = _mm_add_epi32(rowEven, _mm_mullo_epi32(x3, k2));
    rowEven = _mm_add_epi32(rowEven, _mm_mullo_epi32(x4, k3));

    // === Pack and interleave results ===
    rowOdd = _mm_srai_epi32(_mm_add_epi32(rowOdd, kRound), 14);
    rowEven = _mm_srai_epi32(_mm_add_epi32(rowEven, kRound), 14);

    const __m128i r0Odd16 = _mm_packs_epi32(rowOdd, _mm_setzero_si128());
    const __m128i r0Even16 = _mm_packs_epi32(rowEven, _mm_setzero_si128());

    __m128i row16 = _mm_unpacklo_epi16(r0Odd16, r0Even16);

    // === Limit to S15 range after horizontal convolution ===
    row16 = _mm_max_epi16(_mm_min_epi16(row16, kMax16), kMin16);

    // === Apply PA ===
#if APPLY_PA == 1
    const __m128i sum = _mm_madd_epi16(row16, kPAOnes);
    const __m128i mean32 = _mm_srai_epi32(_mm_add_epi32(sum, kPARound), 1);
    const __m128i adjust32 = _mm_sub_epi32(_mm_cvtepi16_epi32(base), mean32);
    const __m128i adjustLo = _mm_unpacklo_epi32(adjust32, adjust32);
    const __m128i adjustHi = _mm_unpackhi_epi32(adjust32, adjust32);
    const __m128i rowLo = _mm_add_epi32(_mm_cvtepi16_epi32(row16), adjustLo);
    const __m128i rowHi = _mm_add_epi32(_mm_cvtepi16_epi32(_mm_srli_si128(row16, 8)), adjustHi);

    row16 = _mm_packs_epi32(rowLo, rowHi);
#endif

    // === Apply dithering ===
#if DITHER == 1
    row16 = sseApplyDither(row16, &ditherBuffer, ditherOffset, ditherShift, ditherMul);
#endif

    // === Store results ===
#if OUT_FIXED_POINT == FP_U8
    const __m128i rowU16 = S16ToU8(row16, kOffset, kMidpoint);
    const __m128i rowU8 = _mm_packus_epi16(rowU16, rowU16);
    _mm_storel_epi64((__m128i*)(dstRow + dstX), rowU8);
#elif OUT_FIXED_POINT == FP_U16
    const __m128i rowU16 = S16ToU16(row16, kOffset, kRightShift, kMidpoint);
    _mm_storeu_si128((__m128i*)(dstRow + dstX), rowU16);
#else
    _mm_storeu_si128((__m128i*)(dstRow + dstX), row16);
#endif
}

#undef KERNEL_VARIANT
