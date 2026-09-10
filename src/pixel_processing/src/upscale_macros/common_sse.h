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

#pragma once

#include <string.h>

// Vertical filter: 4 input rows -> 1 packed int16x8_t output (clamped to [-16384 .. 16363])
//
// Clamping is handled by shifting 1 less, (13), doing a saturating pack, then shifting last step.
//
#define VERTICAL_FILTER(r0, r1, r2, r3, out)                                                                 \
    do {                                                                                                     \
        const __m128i r01Lo = _mm_unpacklo_epi16((r0), (r1));                                                \
        const __m128i r01Hi = _mm_unpackhi_epi16((r0), (r1));                                                \
        const __m128i r23Lo = _mm_unpacklo_epi16((r2), (r3));                                                \
        const __m128i r23Hi = _mm_unpackhi_epi16((r2), (r3));                                                \
        const __m128i lo = _mm_srai_epi32(                                                                   \
            _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(r01Lo, (k01)), _mm_madd_epi16(r23Lo, (k23))), round), \
            13);                                                                                             \
        const __m128i hi = _mm_srai_epi32(                                                                   \
            _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(r01Hi, (k01)), _mm_madd_epi16(r23Hi, (k23))), round), \
            13);                                                                                             \
        (out) = _mm_srai_epi16(_mm_packs_epi32(lo, hi), 1);                                                  \
    } while (0)

// Horizontal filter: 1 vertical result → 1 output row (8 interleaved odd/even samples, clamped)
//
// Each madd tap-pair input is a fixed byte permutation of the window, built with a
// single pshufb from the hoisted controls hfShufOdd01/hfShufOdd23/hfShufEven01/
// hfShufEven23 (half the shuffle-port ops of the shift+unpack formulation).
//
#define HORIZONTAL_FILTER(h, out)                                                                    \
    do {                                                                                             \
        const __m128i odd = _mm_srai_epi32(                                                          \
            _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_shuffle_epi8((h), hfShufOdd01), (k01)),   \
                                        _mm_madd_epi16(_mm_shuffle_epi8((h), hfShufOdd23), (k23))),  \
                          round),                                                                    \
            13);                                                                                     \
        const __m128i even = _mm_srai_epi32(                                                         \
            _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_shuffle_epi8((h), hfShufEven01), (k01)),  \
                                        _mm_madd_epi16(_mm_shuffle_epi8((h), hfShufEven23), (k23))), \
                          round),                                                                    \
            13);                                                                                     \
        (out) = _mm_srai_epi16(                                                                      \
            _mm_packs_epi32(_mm_unpacklo_epi32(even, odd), _mm_unpackhi_epi32(even, odd)), 1);       \
    } while (0)

#define SSE_JOIN_IMPL(a, b) a##b
#define SSE_JOIN(a, b) SSE_JOIN_IMPL(a, b)

// PA and dither dispatch when these macros are expanded, not when this header is included.
// This allows common_sse.h to be included once and the loop headers to be instantiated repeatedly.
#define PREDICTED_AVERAGE(row0, row1, base) \
    SSE_JOIN(SSE_PREDICTED_AVERAGE_, APPLY_PA)(row0, row1, base)
#define PREDICTED_AVERAGE_PAIR(row0A, row1A, row0B, row1B, base) \
    SSE_JOIN(SSE_PREDICTED_AVERAGE_PAIR_, APPLY_PA)(row0A, row1A, row0B, row1B, base)
#define PREDICTED_AVERAGE_1D(row, base) SSE_JOIN(SSE_PREDICTED_AVERAGE_1D_, APPLY_PA)(row, base)

#define SSE_PREDICTED_AVERAGE_0(row0, row1, base) (void)0
#define SSE_PREDICTED_AVERAGE_PAIR_0(row0A, row1A, row0B, row1B, base) (void)0
#define SSE_PREDICTED_AVERAGE_1D_0(row, base) (void)0

// NB: PA relies on source/base and filtered samples being in [-16384, 16383].
#define SSE_PREDICTED_AVERAGE_1(row0, row1, base)                                                  \
    do {                                                                                           \
        if (!params.applyPA) {                                                                     \
            break;                                                                                 \
        }                                                                                          \
        const __m128i sum0 = _mm_madd_epi16((row0), ones);                                         \
        const __m128i sum1 = _mm_madd_epi16((row1), ones);                                         \
        const __m128i mean_32 = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum0, sum1), twos), 2); \
        const __m128i mean_16 = _mm_packs_epi32(mean_32, _mm_setzero_si128());                     \
        const __m128i adjust = _mm_sub_epi16((base), mean_16);                                     \
        const __m128i adjust_copy = _mm_unpacklo_epi16(adjust, adjust);                            \
        (row0) = _mm_adds_epi16((row0), adjust_copy);                                              \
        (row1) = _mm_adds_epi16((row1), adjust_copy);                                              \
    } while (0)

#define SSE_PREDICTED_AVERAGE_PAIR_1(row0A, row1A, row0B, row1B, base)                                \
    do {                                                                                              \
        if (!params.applyPA) {                                                                        \
            break;                                                                                    \
        }                                                                                             \
        const __m128i sum_a0 = _mm_madd_epi16((row0A), ones);                                         \
        const __m128i sum_a1 = _mm_madd_epi16((row1A), ones);                                         \
        const __m128i mean_a = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum_a0, sum_a1), twos), 2); \
        const __m128i sum_b0 = _mm_madd_epi16((row0B), ones);                                         \
        const __m128i sum_b1 = _mm_madd_epi16((row1B), ones);                                         \
        const __m128i mean_b = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum_b0, sum_b1), twos), 2); \
        const __m128i mean_16 = _mm_packs_epi32(mean_a, mean_b);                                      \
        const __m128i adjust = _mm_sub_epi16((base), mean_16);                                        \
        const __m128i adjust_a = _mm_unpacklo_epi16(adjust, adjust);                                  \
        const __m128i adjust_b = _mm_unpackhi_epi16(adjust, adjust);                                  \
        (row0A) = _mm_adds_epi16((row0A), adjust_a);                                                  \
        (row1A) = _mm_adds_epi16((row1A), adjust_a);                                                  \
        (row0B) = _mm_adds_epi16((row0B), adjust_b);                                                  \
        (row1B) = _mm_adds_epi16((row1B), adjust_b);                                                  \
    } while (0)

#define SSE_PREDICTED_AVERAGE_1D_1(row, base)                                     \
    do {                                                                          \
        if (!params.applyPA) {                                                    \
            break;                                                                \
        }                                                                         \
        const __m128i sum = _mm_madd_epi16((row), ones);                          \
        const __m128i mean_32 = _mm_srai_epi32(_mm_add_epi32(sum, pa1d_ones), 1); \
        const __m128i mean_16 = _mm_packs_epi32(mean_32, _mm_setzero_si128());    \
        const __m128i adjust = _mm_sub_epi16((base), mean_16);                    \
        const __m128i adjust_copy = _mm_unpacklo_epi16(adjust, adjust);           \
        (row) = _mm_adds_epi16((row), adjust_copy);                               \
    } while (0)

#define DITHER_ADD(row, dither_ptr, scale) SSE_JOIN(SSE_DITHER_ADD_, DITHER)(row, dither_ptr, scale)
#define SSE_DITHER_ADD_0(row, dither_ptr, scale) (void)0

#define SSE_DITHER_ADD_1(row, dither_ptr, scale)                                  \
    do {                                                                          \
        if (!ditherEnabled) {                                                     \
            break;                                                                \
        }                                                                         \
        __m128i dither_values = _mm_loadu_si128((const __m128i*)(dither_ptr));    \
        (dither_ptr) += 8;                                                        \
        dither_values = _mm_mulhi_epu16(dither_values, ditherMul);                \
        dither_values = _mm_sub_epi16(ditherOffset, dither_values);               \
        (row) = _mm_adds_epi16((row), _mm_sll_epi16(dither_values, ditherShift)); \
    } while (0)

static inline __m128i sseLoadU8x8(const uint8_t* ptr)
{
    return _mm_slli_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)ptr)), 7);
}

static inline __m128i sseLoadU8x4(const uint8_t* ptr)
{
    uint32_t values;
    memcpy(&values, ptr, sizeof(values));
    return _mm_slli_epi16(_mm_cvtepu8_epi16(_mm_cvtsi32_si128((int32_t)values)), 7);
}

#define SSE_INPUT_TYPE_1 uint8_t
#define SSE_INPUT_TYPE_2 uint8_t
#define SSE_INPUT_TYPE_3 uint16_t
#define SSE_INPUT_TYPE_4 int16_t
#define SSE_OUTPUT_TYPE_1 uint8_t
#define SSE_OUTPUT_TYPE_2 uint8_t
#define SSE_OUTPUT_TYPE_3 uint16_t
#define SSE_OUTPUT_TYPE_4 int16_t
#define SSE_INPUT_TYPE SSE_JOIN(SSE_INPUT_TYPE_, IN_FIXED_POINT)
#define SSE_OUTPUT_TYPE SSE_JOIN(SSE_OUTPUT_TYPE_, OUT_FIXED_POINT)

static inline int32_t sseClampRow(int32_t y, uint32_t height)
{
    if (y < 0) {
        return 0;
    }
    if (y >= (int32_t)height) {
        return (int32_t)height - 1;
    }
    return y;
}
#define SSE_ROW_U8(ptr, stride, height, y) getRowU8(ptr, stride, sseClampRow(y, height))
#define SSE_ROW_S16(ptr, stride, height, y) getRowS16(ptr, stride, sseClampRow(y, height))

#define SSE_INPUT_ROW_1(ptr, stride, height, y) ((uint8_t*)SSE_ROW_U8(ptr, stride, height, y))
#define SSE_INPUT_ROW_2(ptr, stride, height, y) ((uint8_t*)SSE_ROW_U8(ptr, stride, height, y))
#define SSE_INPUT_ROW_3(ptr, stride, height, y) ((uint16_t*)SSE_ROW_S16(ptr, stride, height, y))
#define SSE_INPUT_ROW_4(ptr, stride, height, y) ((int16_t*)SSE_ROW_S16(ptr, stride, height, y))
#define SSE_OUTPUT_ROW_1(ptr, stride, height, y) ((uint8_t*)SSE_ROW_U8(ptr, stride, height, y))
#define SSE_OUTPUT_ROW_2(ptr, stride, height, y) ((uint8_t*)SSE_ROW_U8(ptr, stride, height, y))
#define SSE_OUTPUT_ROW_3(ptr, stride, height, y) ((uint16_t*)SSE_ROW_S16(ptr, stride, height, y))
#define SSE_OUTPUT_ROW_4(ptr, stride, height, y) ((int16_t*)SSE_ROW_S16(ptr, stride, height, y))
#define SSE_INPUT_ROW(ptr, stride, height, y) \
    SSE_JOIN(SSE_INPUT_ROW_, IN_FIXED_POINT)(ptr, stride, height, y)
#define SSE_OUTPUT_ROW(ptr, stride, height, y) \
    SSE_JOIN(SSE_OUTPUT_ROW_, OUT_FIXED_POINT)(ptr, stride, height, y)

#define SSE_LOAD_INPUT_1(ptr) _mm_sub_epi16(sseLoadU8x8((const uint8_t*)(ptr)), inputMidpoint)
#define SSE_LOAD_INPUT_2(ptr) SSE_LOAD_INPUT_1(ptr)
#define SSE_LOAD_INPUT_3(ptr) \
    _mm_sub_epi16(_mm_sll_epi16(_mm_loadu_si128((const __m128i*)(ptr)), inputShift), inputMidpoint)
#define SSE_LOAD_INPUT_4(ptr) _mm_loadu_si128((const __m128i*)(ptr))
#define SSE_LOAD_INPUT(ptr) SSE_JOIN(SSE_LOAD_INPUT_, IN_FIXED_POINT)(ptr)

#define SSE_LOAD_BASE4_1(ptr) _mm_sub_epi16(sseLoadU8x4((const uint8_t*)(ptr)), inputMidpoint)
#define SSE_LOAD_BASE4_2(ptr) SSE_LOAD_BASE4_1(ptr)
#define SSE_LOAD_BASE4_3(ptr) \
    _mm_sub_epi16(_mm_sll_epi16(_mm_loadl_epi64((const __m128i*)(ptr)), inputShift), inputMidpoint)
#define SSE_LOAD_BASE4_4(ptr) _mm_loadl_epi64((const __m128i*)(ptr))
#define SSE_LOAD_BASE4(ptr) SSE_JOIN(SSE_LOAD_BASE4_, IN_FIXED_POINT)(ptr)

#define SSE_DEMOTE_U8(value) \
    _mm_add_epi16(_mm_srai_epi16(_mm_add_epi16((value), outputOffset), 7), outputMidpoint)
#define SSE_STORE_OUTPUT_1(ptr, value) \
    _mm_storel_epi64((__m128i*)(ptr), _mm_packus_epi16(SSE_DEMOTE_U8(value), _mm_setzero_si128()))
#define SSE_STORE_OUTPUT_2(ptr, value) SSE_STORE_OUTPUT_1(ptr, value)
#define SSE_STORE_OUTPUT_3(ptr, value)                                                               \
    _mm_storeu_si128((__m128i*)(ptr),                                                                \
                     _mm_add_epi16(_mm_sra_epi16(_mm_add_epi16((value), outputOffset), outputShift), \
                                   outputMidpoint))
#define SSE_STORE_OUTPUT_4(ptr, value) _mm_storeu_si128((__m128i*)(ptr), (value))
#define SSE_STORE_OUTPUT(ptr, value) SSE_JOIN(SSE_STORE_OUTPUT_, OUT_FIXED_POINT)(ptr, value)

// Deinterleave two 128-bit loads into 8 U and 8 V samples
#define DEINTERLEAVE(loIn, hiIn, uOut, vOut)                    \
    do {                                                        \
        const __m128i lo = _mm_shuffle_epi8((loIn), deintShuf); \
        const __m128i hi = _mm_shuffle_epi8((hiIn), deintShuf); \
        (uOut) = _mm_unpacklo_epi64(lo, hi);                    \
        (vOut) = _mm_unpackhi_epi64(lo, hi);                    \
    } while (0)

// Two-register byte shuffle (equivalent of NEON vqtbl2q_u8)
//
static inline __m128i shuffle2(__m128i v0, __m128i v1, __m128i control)
{
    const __m128i blend_mask = _mm_cmpgt_epi8(control, _mm_set1_epi8(15));
    const __m128i from_v0 = _mm_shuffle_epi8(v0, control);
    const __m128i from_v1 = _mm_shuffle_epi8(v1, _mm_sub_epi8(control, _mm_set1_epi8(16)));
    return _mm_blendv_epi8(from_v0, from_v1, blend_mask);
}
