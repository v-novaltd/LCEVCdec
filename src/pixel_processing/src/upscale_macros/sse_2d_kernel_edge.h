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

#if !defined(KERNEL_VARIANT)
#error "KERNEL_VARIANT must be defined before including this header"
#endif

{
#if KERNEL_VARIANT == 1
#if IN_FIXED_POINT == FP_U8
    const __m128i src0Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)srcRow0));
    const __m128i src1Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)srcRow1));
    const __m128i src2Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)srcRow2));
    const __m128i src3Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)srcRow3));
    const __m128i src4Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)srcRow4));
#elif IN_FIXED_POINT == FP_U16
    const __m128i src0Raw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow0));
    const __m128i src1Raw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow1));
    const __m128i src2Raw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow2));
    const __m128i src3Raw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow3));
    const __m128i src4Raw = U16ToS16(_mm_loadu_si128((const __m128i*)srcRow4));
#else
    const __m128i src0Raw = _mm_loadu_si128((const __m128i*)srcRow0);
    const __m128i src1Raw = _mm_loadu_si128((const __m128i*)srcRow1);
    const __m128i src2Raw = _mm_loadu_si128((const __m128i*)srcRow2);
    const __m128i src3Raw = _mm_loadu_si128((const __m128i*)srcRow3);
    const __m128i src4Raw = _mm_loadu_si128((const __m128i*)srcRow4);
#endif

    const __m128i src0 = _mm_shuffle_epi8(src0Raw, kLeftEdgeShuffle);
    const __m128i src1 = _mm_shuffle_epi8(src1Raw, kLeftEdgeShuffle);
    const __m128i src2 = _mm_shuffle_epi8(src2Raw, kLeftEdgeShuffle);
    const __m128i src3 = _mm_shuffle_epi8(src3Raw, kLeftEdgeShuffle);
    const __m128i src4 = _mm_shuffle_epi8(src4Raw, kLeftEdgeShuffle);

    const int32_t dstX = 0;
#if APPLY_PA == 1
    const __m128i base = src2Raw;
#endif
#elif KERNEL_VARIANT == 2
    const int32_t loadPos = (int32_t)srcWidth - 8;

#if IN_FIXED_POINT == FP_U8
    const __m128i src0Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow0 + loadPos)));
    const __m128i src1Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow1 + loadPos)));
    const __m128i src2Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow2 + loadPos)));
    const __m128i src3Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow3 + loadPos)));
    const __m128i src4Raw = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow4 + loadPos)));
#elif IN_FIXED_POINT == FP_U16
    const __m128i src0Raw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow0 + loadPos)));
    const __m128i src1Raw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow1 + loadPos)));
    const __m128i src2Raw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow2 + loadPos)));
    const __m128i src3Raw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow3 + loadPos)));
    const __m128i src4Raw = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow4 + loadPos)));
#else
    const __m128i src0Raw = _mm_loadu_si128((const __m128i*)(srcRow0 + loadPos));
    const __m128i src1Raw = _mm_loadu_si128((const __m128i*)(srcRow1 + loadPos));
    const __m128i src2Raw = _mm_loadu_si128((const __m128i*)(srcRow2 + loadPos));
    const __m128i src3Raw = _mm_loadu_si128((const __m128i*)(srcRow3 + loadPos));
    const __m128i src4Raw = _mm_loadu_si128((const __m128i*)(srcRow4 + loadPos));
#endif

    const __m128i src0 = _mm_shuffle_epi8(src0Raw, kRightEdgeShuffle);
    const __m128i src1 = _mm_shuffle_epi8(src1Raw, kRightEdgeShuffle);
    const __m128i src2 = _mm_shuffle_epi8(src2Raw, kRightEdgeShuffle);
    const __m128i src3 = _mm_shuffle_epi8(src3Raw, kRightEdgeShuffle);
    const __m128i src4 = _mm_shuffle_epi8(src4Raw, kRightEdgeShuffle);

    const int32_t dstX = ((int32_t)srcWidth * 2) - 8;
#if APPLY_PA == 1
    const __m128i base = _mm_srli_si128(src2, 4);
#endif
#else
#if IN_FIXED_POINT == FP_U8
    const __m128i src0 = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow0 + srcX - 2)));
    const __m128i src1 = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow1 + srcX - 2)));
    const __m128i src2 = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow2 + srcX - 2)));
    const __m128i src3 = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow3 + srcX - 2)));
    const __m128i src4 = U8ToS16(_mm_loadl_epi64((const __m128i*)(srcRow4 + srcX - 2)));
#elif IN_FIXED_POINT == FP_U16
    const __m128i src0 = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow0 + srcX - 2)));
    const __m128i src1 = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow1 + srcX - 2)));
    const __m128i src2 = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow2 + srcX - 2)));
    const __m128i src3 = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow3 + srcX - 2)));
    const __m128i src4 = U16ToS16(_mm_loadu_si128((const __m128i*)(srcRow4 + srcX - 2)));
#else
    const __m128i src0 = _mm_loadu_si128((const __m128i*)(srcRow0 + srcX - 2));
    const __m128i src1 = _mm_loadu_si128((const __m128i*)(srcRow1 + srcX - 2));
    const __m128i src2 = _mm_loadu_si128((const __m128i*)(srcRow2 + srcX - 2));
    const __m128i src3 = _mm_loadu_si128((const __m128i*)(srcRow3 + srcX - 2));
    const __m128i src4 = _mm_loadu_si128((const __m128i*)(srcRow4 + srcX - 2));
#endif

    const int32_t dstX = srcX * 2;
#if APPLY_PA == 1
    const __m128i base = _mm_srli_si128(src2, 4);
#endif
#endif

    const __m128i src01Lo = _mm_unpacklo_epi16(src0, src1);
    const __m128i src01Hi = _mm_unpackhi_epi16(src0, src1);
    const __m128i src23Lo = _mm_unpacklo_epi16(src2, src3);
    const __m128i src23Hi = _mm_unpackhi_epi16(src2, src3);

    const __m128i src12Lo = _mm_unpacklo_epi16(src1, src2);
    const __m128i src12Hi = _mm_unpackhi_epi16(src1, src2);
    const __m128i src34Lo = _mm_unpacklo_epi16(src3, src4);
    const __m128i src34Hi = _mm_unpackhi_epi16(src3, src4);

    const __m128i v0Lo = _mm_srai_epi32(
        _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(src01Lo, k32), _mm_madd_epi16(src23Lo, k10)), kRound), 14);
    const __m128i v0Hi = _mm_srai_epi32(
        _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(src01Hi, k32), _mm_madd_epi16(src23Hi, k10)), kRound), 14);

    const __m128i v1Lo = _mm_srai_epi32(
        _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(src12Lo, k01), _mm_madd_epi16(src34Lo, k23)), kRound), 14);
    const __m128i v1Hi = _mm_srai_epi32(
        _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(src12Hi, k01), _mm_madd_epi16(src34Hi, k23)), kRound), 14);

    __m128i v0 = _mm_packs_epi32(v0Lo, v0Hi);
    __m128i v1 = _mm_packs_epi32(v1Lo, v1Hi);

    v0 = _mm_max_epi16(_mm_min_epi16(v0, kMax16), kMin16);
    v1 = _mm_max_epi16(_mm_min_epi16(v1, kMax16), kMin16);

    const __m128i v0_s1 = _mm_srli_si128(v0, 2);
    const __m128i v0_s2 = _mm_srli_si128(v0, 4);
    const __m128i v0_s3 = _mm_srli_si128(v0, 6);
    const __m128i v0_s4 = _mm_srli_si128(v0, 8);

    const __m128i w0_01 = _mm_unpacklo_epi64(v0, v0_s1);
    const __m128i w0_23 = _mm_unpacklo_epi64(v0_s2, v0_s3);
    const __m128i w0_12 = _mm_alignr_epi8(v0_s2, w0_01, 8);
    const __m128i w0_34 = _mm_alignr_epi8(v0_s4, w0_23, 8);

    const __m128i row0Odd = _mm_srai_epi32(
        _mm_add_epi32(_mm_hadd_epi32(_mm_madd_epi16(w0_01, kHOdd), _mm_madd_epi16(w0_23, kHOdd)), kRound),
        14);
    const __m128i row0Even = _mm_srai_epi32(
        _mm_add_epi32(_mm_hadd_epi32(_mm_madd_epi16(w0_12, kHEven), _mm_madd_epi16(w0_34, kHEven)), kRound),
        14);

    const __m128i v1_s1 = _mm_srli_si128(v1, 2);
    const __m128i v1_s2 = _mm_srli_si128(v1, 4);
    const __m128i v1_s3 = _mm_srli_si128(v1, 6);
    const __m128i v1_s4 = _mm_srli_si128(v1, 8);

    const __m128i w1_01 = _mm_unpacklo_epi64(v1, v1_s1);
    const __m128i w1_23 = _mm_unpacklo_epi64(v1_s2, v1_s3);
    const __m128i w1_12 = _mm_alignr_epi8(v1_s2, w1_01, 8);
    const __m128i w1_34 = _mm_alignr_epi8(v1_s4, w1_23, 8);

    const __m128i row1Odd = _mm_srai_epi32(
        _mm_add_epi32(_mm_hadd_epi32(_mm_madd_epi16(w1_01, kHOdd), _mm_madd_epi16(w1_23, kHOdd)), kRound),
        14);
    const __m128i row1Even = _mm_srai_epi32(
        _mm_add_epi32(_mm_hadd_epi32(_mm_madd_epi16(w1_12, kHEven), _mm_madd_epi16(w1_34, kHEven)), kRound),
        14);

    __m128i row0_16 =
        _mm_packs_epi32(_mm_unpacklo_epi32(row0Odd, row0Even), _mm_unpackhi_epi32(row0Odd, row0Even));
    __m128i row1_16 =
        _mm_packs_epi32(_mm_unpacklo_epi32(row1Odd, row1Even), _mm_unpackhi_epi32(row1Odd, row1Even));

    row0_16 = _mm_max_epi16(_mm_min_epi16(row0_16, kMax16), kMin16);
    row1_16 = _mm_max_epi16(_mm_min_epi16(row1_16, kMax16), kMin16);

#if APPLY_PA == 1
    sseApplyPA(&row0_16, &row1_16, base);
#endif

#if DITHER == 1
    row0_16 = sseApplyDither(row0_16, &ditherBuffer, ditherOffset, ditherShift, ditherMul);
    row1_16 = sseApplyDither(row1_16, &ditherBuffer, ditherOffset, ditherShift, ditherMul);
#endif

#if OUT_FIXED_POINT == FP_U8
    const __m128i row0_u16 = S16ToU8(row0_16, kOffset, kMidpoint);
    const __m128i row1_u16 = S16ToU8(row1_16, kOffset, kMidpoint);
    const __m128i row0_u8 = _mm_packus_epi16(row0_u16, row0_u16);
    const __m128i row1_u8 = _mm_packus_epi16(row1_u16, row1_u16);
    _mm_storel_epi64((__m128i*)(dstRow0 + dstX), row0_u8);
    _mm_storel_epi64((__m128i*)(dstRow1 + dstX), row1_u8);
#elif OUT_FIXED_POINT == FP_U16
    const __m128i row0_u16 = S16ToU16(row0_16, kOffset, kRightShift, kMidpoint);
    const __m128i row1_u16 = S16ToU16(row1_16, kOffset, kRightShift, kMidpoint);
    _mm_storeu_si128((__m128i*)(dstRow0 + dstX), row0_u16);
    _mm_storeu_si128((__m128i*)(dstRow1 + dstX), row1_u16);
#else
    _mm_storeu_si128((__m128i*)(dstRow0 + dstX), row0_16);
    _mm_storeu_si128((__m128i*)(dstRow1 + dstX), row1_16);
#endif
}

#undef KERNEL_VARIANT
