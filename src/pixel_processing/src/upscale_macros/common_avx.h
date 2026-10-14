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

// Vertical filter: 4 input rows -> 1 packed int16x8_t output (clamped to [-16384 .. 16383])
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

#define VERTICAL_FILTER_256(r0, r1, r2, r3, out)                                                      \
    do {                                                                                              \
        const __m256i r01Lo = _mm256_unpacklo_epi16((r0), (r1));                                      \
        const __m256i r01Hi = _mm256_unpackhi_epi16((r0), (r1));                                      \
        const __m256i r23Lo = _mm256_unpacklo_epi16((r2), (r3));                                      \
        const __m256i r23Hi = _mm256_unpackhi_epi16((r2), (r3));                                      \
        const __m256i lo =                                                                            \
            _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(r01Lo, (k01_256)),  \
                                                                _mm256_madd_epi16(r23Lo, (k23_256))), \
                                               round256),                                             \
                              13);                                                                    \
        const __m256i hi =                                                                            \
            _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(r01Hi, (k01_256)),  \
                                                                _mm256_madd_epi16(r23Hi, (k23_256))), \
                                               round256),                                             \
                              13);                                                                    \
        (out) = _mm256_srai_epi16(_mm256_packs_epi32(lo, hi), 1);                                     \
    } while (0)

// Horizontal filter: 1 vertical result -> 1 output row (8 interleaved odd/even samples, clamped)
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

// Horizontal filter on two independent 8-tap windows, one per 128-bit lane.
// Each lane yields 8 interleaved even/odd output samples (all ops are in-lane).
// Requires hfShuf*_256 controls (the 128-bit patterns broadcast to both lanes).
//
#define HORIZONTAL_FILTER_256(h, out)                                                                      \
    do {                                                                                                   \
        const __m256i odd = _mm256_srai_epi32(                                                             \
            _mm256_add_epi32(                                                                              \
                _mm256_add_epi32(_mm256_madd_epi16(_mm256_shuffle_epi8((h), hfShufOdd01_256), (k01_256)),  \
                                 _mm256_madd_epi16(_mm256_shuffle_epi8((h), hfShufOdd23_256), (k23_256))), \
                round256),                                                                                 \
            13);                                                                                           \
        const __m256i even = _mm256_srai_epi32(                                                            \
            _mm256_add_epi32(                                                                              \
                _mm256_add_epi32(                                                                          \
                    _mm256_madd_epi16(_mm256_shuffle_epi8((h), hfShufEven01_256), (k01_256)),              \
                    _mm256_madd_epi16(_mm256_shuffle_epi8((h), hfShufEven23_256), (k23_256))),             \
                round256),                                                                                 \
            13);                                                                                           \
        (out) = _mm256_srai_epi16(                                                                         \
            _mm256_packs_epi32(_mm256_unpacklo_epi32(even, odd), _mm256_unpackhi_epi32(even, odd)), 1);    \
    } while (0)

#define AVX_JOIN_IMPL(a, b) a##b
#define AVX_JOIN(a, b) AVX_JOIN_IMPL(a, b)

// PA and dither dispatch when these macros are expanded, not when this header is included.
// This allows common_avx.h to be included once and the loop headers to be instantiated repeatedly.
#define PREDICTED_AVERAGE(row0, row1, base) \
    AVX_JOIN(AVX_PREDICTED_AVERAGE_, APPLY_PA)(row0, row1, base)
#define PREDICTED_AVERAGE_PAIR(row0A, row1A, row0B, row1B, base) \
    AVX_JOIN(AVX_PREDICTED_AVERAGE_PAIR_, APPLY_PA)(row0A, row1A, row0B, row1B, base)
#define PREDICTED_AVERAGE_PAIR_256(rowA, rowB, base) \
    AVX_JOIN(AVX_PREDICTED_AVERAGE_PAIR_256_, APPLY_PA)(rowA, rowB, base)
#define PREDICTED_AVERAGE_256(row0, row1, base) \
    AVX_JOIN(AVX_PREDICTED_AVERAGE_256_, APPLY_PA)(row0, row1, base)
#define PREDICTED_AVERAGE_1D(row, base) AVX_JOIN(AVX_PREDICTED_AVERAGE_1D_, APPLY_PA)(row, base)
#define PREDICTED_AVERAGE_1D_256(row, base) \
    AVX_JOIN(AVX_PREDICTED_AVERAGE_1D_256_, APPLY_PA)(row, base)

#define AVX_PREDICTED_AVERAGE_0(row0, row1, base) (void)(base)
#define AVX_PREDICTED_AVERAGE_PAIR_0(row0A, row1A, row0B, row1B, base) (void)(base)
#define AVX_PREDICTED_AVERAGE_PAIR_256_0(rowA, rowB, base) (void)(base)
#define AVX_PREDICTED_AVERAGE_256_0(row0, row1, base) (void)(base)
#define AVX_PREDICTED_AVERAGE_1D_0(row, base) (void)(base)
#define AVX_PREDICTED_AVERAGE_1D_256_0(row, base) (void)(base)

// NB: PA relies on source/base and filtered samples being in [-16384, 16383].
#define AVX_PREDICTED_AVERAGE_1(row0, row1, base)                                                  \
    do {                                                                                           \
        if (!params.applyPA) {                                                                     \
            break;                                                                                 \
        }                                                                                          \
        const __m128i sum0 = _mm_madd_epi16((row0), ones);                                         \
        const __m128i sum1 = _mm_madd_epi16((row1), ones);                                         \
        const __m128i mean_32 = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum0, sum1), twos), 2); \
        const __m128i mean_16 = _mm_packs_epi32(mean_32, _mm_setzero_si128());                     \
        const __m128i adjust4 = _mm_sub_epi16((base), mean_16);                                    \
        const __m128i adjustCopy = _mm_unpacklo_epi16(adjust4, adjust4);                           \
        (row0) = _mm_adds_epi16((row0), adjustCopy);                                               \
        (row1) = _mm_adds_epi16((row1), adjustCopy);                                               \
    } while (0)

// Combined predicted average for two adjacent blocks.
// Merges two packs_epi32 into one and uses a single sub_epi16 + unpacklo/hi.
#define AVX_PREDICTED_AVERAGE_PAIR_1(row0A, row1A, row0B, row1B, base)                             \
    do {                                                                                           \
        if (!params.applyPA) {                                                                     \
            break;                                                                                 \
        }                                                                                          \
        const __m128i sumA0 = _mm_madd_epi16((row0A), ones);                                       \
        const __m128i sumA1 = _mm_madd_epi16((row1A), ones);                                       \
        const __m128i meanA = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sumA0, sumA1), twos), 2); \
        const __m128i sumB0 = _mm_madd_epi16((row0B), ones);                                       \
        const __m128i sumB1 = _mm_madd_epi16((row1B), ones);                                       \
        const __m128i meanB = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sumB0, sumB1), twos), 2); \
        const __m128i mean16 = _mm_packs_epi32(meanA, meanB);                                      \
        const __m128i adjust = _mm_sub_epi16((base), mean16);                                      \
        const __m128i adjustA = _mm_unpacklo_epi16(adjust, adjust);                                \
        const __m128i adjustB = _mm_unpackhi_epi16(adjust, adjust);                                \
        (row0A) = _mm_adds_epi16((row0A), adjustA);                                                \
        (row1A) = _mm_adds_epi16((row1A), adjustA);                                                \
        (row0B) = _mm_adds_epi16((row0B), adjustB);                                                \
        (row1B) = _mm_adds_epi16((row1B), adjustB);                                                \
    } while (0)

// Combined predicted average for two adjacent 8-pixel blocks held as row pairs:
// rowA/rowB carry the even output row in the low 128-bit lane and the odd output
// row in the high lane. base holds the corresponding 8 source pixels.
//
#define AVX_PREDICTED_AVERAGE_PAIR_256_1(rowA, rowB, base)                                       \
    do {                                                                                         \
        if (!params.applyPA) {                                                                   \
            break;                                                                               \
        }                                                                                        \
        const __m256i sumA = _mm256_madd_epi16((rowA), ones256);                                 \
        const __m256i sumB = _mm256_madd_epi16((rowB), ones256);                                 \
        const __m256i sumRow0 = _mm256_permute2x128_si256(sumA, sumB, 0x20);                     \
        const __m256i sumRow1 = _mm256_permute2x128_si256(sumA, sumB, 0x31);                     \
        const __m256i mean32 =                                                                   \
            _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(sumRow0, sumRow1), twos256), 2); \
        const __m256i meanPacked = _mm256_packs_epi32(mean32, mean32);                           \
        const __m256i mean16 = _mm256_permute4x64_epi64(meanPacked, _MM_SHUFFLE(2, 0, 2, 0));    \
        const __m256i adjust = _mm256_sub_epi16(_mm256_broadcastsi128_si256((base)), mean16);    \
        (rowA) = _mm256_adds_epi16((rowA), _mm256_unpacklo_epi16(adjust, adjust));               \
        (rowB) = _mm256_adds_epi16((rowB), _mm256_unpackhi_epi16(adjust, adjust));               \
    } while (0)

// Predicted average for two 256-bit rows carrying independent channels per lane
// (e.g. U in the low lane, V in the high lane). base carries the 4 source pixels
// for each lane's channel in that lane's low 64 bits.
//
#define AVX_PREDICTED_AVERAGE_256_1(row0, row1, base)                                      \
    do {                                                                                   \
        if (!params.applyPA) {                                                             \
            break;                                                                         \
        }                                                                                  \
        const __m256i sum0 = _mm256_madd_epi16((row0), ones256);                           \
        const __m256i sum1 = _mm256_madd_epi16((row1), ones256);                           \
        const __m256i mean32 =                                                             \
            _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(sum0, sum1), twos256), 2); \
        const __m256i mean16 = _mm256_packs_epi32(mean32, mean32);                         \
        const __m256i adjust4 = _mm256_sub_epi16((base), mean16);                          \
        const __m256i adjustCopy = _mm256_unpacklo_epi16(adjust4, adjust4);                \
        (row0) = _mm256_adds_epi16((row0), adjustCopy);                                    \
        (row1) = _mm256_adds_epi16((row1), adjustCopy);                                    \
    } while (0)

// 1D predicted average: adjust a single output row so each output pair averages to its source pixel
//
// row    - __m128i of 8 output samples, interleaved [p0_odd, p0_even, p1_odd, p1_even, ...]
// base   - __m128i with 4 source pixels in the low 64 bits
//
#define AVX_PREDICTED_AVERAGE_1D_1(row, base)                                     \
    do {                                                                          \
        if (!params.applyPA) {                                                    \
            break;                                                                \
        }                                                                         \
        const __m128i sum = _mm_madd_epi16((row), ones);                          \
        const __m128i mean_32 = _mm_srai_epi32(_mm_add_epi32(sum, pa1d_ones), 1); \
        const __m128i mean_16 = _mm_packs_epi32(mean_32, _mm_setzero_si128());    \
        const __m128i adjust = _mm_sub_epi16((base), mean_16);                    \
        const __m128i adjustCopy = _mm_unpacklo_epi16(adjust, adjust);            \
        (row) = _mm_adds_epi16((row), adjustCopy);                                \
    } while (0)

// 1D predicted average on two independent output blocks, one per 128-bit lane.
// Each lane of row contains 8 outputs; each lane of base contains its 4 source
// samples in the low 64 bits.
#define AVX_PREDICTED_AVERAGE_1D_256_1(row, base)                                         \
    do {                                                                                  \
        if (!params.applyPA) {                                                            \
            break;                                                                        \
        }                                                                                 \
        const __m256i sum = _mm256_madd_epi16((row), ones256);                            \
        const __m256i mean32 = _mm256_srai_epi32(_mm256_add_epi32(sum, pa1d_ones256), 1); \
        const __m256i mean16 = _mm256_packs_epi32(mean32, _mm256_setzero_si256());        \
        const __m256i adjust = _mm256_sub_epi16((base), mean16);                          \
        (row) = _mm256_adds_epi16((row), _mm256_unpacklo_epi16(adjust, adjust));          \
    } while (0)

#define DITHER_ADD(row, ditherPtr, scale) AVX_JOIN(AVX_DITHER_ADD_, DITHER)(row, ditherPtr, scale)
#define DITHER_ADD_256(row, ditherPtr, scale) \
    AVX_JOIN(AVX_DITHER_ADD_256_, DITHER)(row, ditherPtr, scale)
#define AVX_DITHER_ADD_0(row, ditherPtr, scale) (void)0
#define AVX_DITHER_ADD_256_0(row, ditherPtr, scale) (void)0

#define AVX_DITHER_ADD_1(row, ditherPtr, scale)                                  \
    do {                                                                         \
        if (!ditherEnabled) {                                                    \
            break;                                                               \
        }                                                                        \
        __m128i ditherValues = _mm_loadu_si128((const __m128i*)(ditherPtr));     \
        (ditherPtr) += 8;                                                        \
        ditherValues = _mm_mulhi_epu16(ditherValues, ditherMul);                 \
        ditherValues = _mm_sub_epi16(ditherOffset, ditherValues);                \
        (row) = _mm_adds_epi16((row), _mm_sll_epi16(ditherValues, ditherShift)); \
    } while (0)
#define AVX_DITHER_ADD_256_1(row, ditherPtr, scale)                                    \
    do {                                                                               \
        if (!ditherEnabled) {                                                          \
            break;                                                                     \
        }                                                                              \
        __m256i ditherValues = _mm256_loadu_si256((const __m256i*)(ditherPtr));        \
        (ditherPtr) += 16;                                                             \
        ditherValues = _mm256_mulhi_epu16(ditherValues, ditherMul256);                 \
        ditherValues = _mm256_sub_epi16(ditherOffset256, ditherValues);                \
        (row) = _mm256_adds_epi16((row), _mm256_sll_epi16(ditherValues, ditherShift)); \
    } while (0)

static inline __m128i avxLoadU8x4(const uint8_t* ptr)
{
    uint32_t values;
    memcpy(&values, ptr, sizeof(values));
    return _mm_slli_epi16(_mm_cvtepu8_epi16(_mm_cvtsi32_si128((int32_t)values)), 7);
}

static inline __m128i avxLoadU8x8(const uint8_t* ptr)
{
    return _mm_slli_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)ptr)), 7);
}

static inline __m256i avxLoadU8x16(const uint8_t* ptr)
{
    return _mm256_slli_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)ptr)), 7);
}

static inline void avxStoreU8x16(uint8_t* ptr, __m256i value)
{
    const __m256i packed = _mm256_packus_epi16(value, _mm256_setzero_si256());
    const __m256i contiguous = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
    _mm_storeu_si128((__m128i*)ptr, _mm256_castsi256_si128(contiguous));
}

#define AVX_INPUT_TYPE_1 uint8_t
#define AVX_INPUT_TYPE_2 uint8_t
#define AVX_INPUT_TYPE_3 uint16_t
#define AVX_INPUT_TYPE_4 int16_t
#define AVX_OUTPUT_TYPE_1 uint8_t
#define AVX_OUTPUT_TYPE_2 uint8_t
#define AVX_OUTPUT_TYPE_3 uint16_t
#define AVX_OUTPUT_TYPE_4 int16_t
#define AVX_INPUT_TYPE AVX_JOIN(AVX_INPUT_TYPE_, IN_FIXED_POINT)
#define AVX_OUTPUT_TYPE AVX_JOIN(AVX_OUTPUT_TYPE_, OUT_FIXED_POINT)

static inline int32_t avxClampRow(int32_t y, uint32_t height)
{
    if (y < 0) {
        return 0;
    }
    if (y >= (int32_t)height) {
        return (int32_t)height - 1;
    }
    return y;
}
#define AVX_ROW_U8(ptr, stride, height, y) getRowU8(ptr, stride, avxClampRow(y, height))
#define AVX_ROW_S16(ptr, stride, height, y) getRowS16(ptr, stride, avxClampRow(y, height))

#define AVX_INPUT_ROW_1(ptr, stride, height, y) ((uint8_t*)AVX_ROW_U8(ptr, stride, height, y))
#define AVX_INPUT_ROW_2(ptr, stride, height, y) ((uint8_t*)AVX_ROW_U8(ptr, stride, height, y))
#define AVX_INPUT_ROW_3(ptr, stride, height, y) ((uint16_t*)AVX_ROW_S16(ptr, stride, height, y))
#define AVX_INPUT_ROW_4(ptr, stride, height, y) ((int16_t*)AVX_ROW_S16(ptr, stride, height, y))
#define AVX_OUTPUT_ROW_1(ptr, stride, height, y) ((uint8_t*)AVX_ROW_U8(ptr, stride, height, y))
#define AVX_OUTPUT_ROW_2(ptr, stride, height, y) ((uint8_t*)AVX_ROW_U8(ptr, stride, height, y))
#define AVX_OUTPUT_ROW_3(ptr, stride, height, y) ((uint16_t*)AVX_ROW_S16(ptr, stride, height, y))
#define AVX_OUTPUT_ROW_4(ptr, stride, height, y) ((int16_t*)AVX_ROW_S16(ptr, stride, height, y))
#define AVX_INPUT_ROW(ptr, stride, height, y) \
    AVX_JOIN(AVX_INPUT_ROW_, IN_FIXED_POINT)(ptr, stride, height, y)
#define AVX_OUTPUT_ROW(ptr, stride, height, y) \
    AVX_JOIN(AVX_OUTPUT_ROW_, OUT_FIXED_POINT)(ptr, stride, height, y)

#define AVX_LOAD_INPUT_128_1(ptr) _mm_sub_epi16(avxLoadU8x8((const uint8_t*)(ptr)), inputMidpoint)
#define AVX_LOAD_INPUT_128_2(ptr) AVX_LOAD_INPUT_128_1(ptr)
#define AVX_LOAD_INPUT_128_3(ptr) \
    _mm_sub_epi16(_mm_sll_epi16(_mm_loadu_si128((const __m128i*)(ptr)), inputShift), inputMidpoint)
#define AVX_LOAD_INPUT_128_4(ptr) _mm_loadu_si128((const __m128i*)(ptr))
#define AVX_LOAD_INPUT_128(ptr) AVX_JOIN(AVX_LOAD_INPUT_128_, IN_FIXED_POINT)(ptr)

#define AVX_LOAD_INPUT_256_1(ptr) \
    _mm256_sub_epi16(avxLoadU8x16((const uint8_t*)(ptr)), inputMidpoint256)
#define AVX_LOAD_INPUT_256_2(ptr) AVX_LOAD_INPUT_256_1(ptr)
#define AVX_LOAD_INPUT_256_3(ptr) \
    _mm256_sub_epi16(_mm256_sll_epi16(_mm256_loadu_si256((const __m256i*)(ptr)), inputShift), inputMidpoint256)
#define AVX_LOAD_INPUT_256_4(ptr) _mm256_loadu_si256((const __m256i*)(ptr))
#define AVX_LOAD_INPUT_256(ptr) AVX_JOIN(AVX_LOAD_INPUT_256_, IN_FIXED_POINT)(ptr)

#define AVX_LOAD_BASE4_1(ptr) _mm_sub_epi16(avxLoadU8x4((const uint8_t*)(ptr)), inputMidpoint)
#define AVX_LOAD_BASE4_2(ptr) AVX_LOAD_BASE4_1(ptr)
#define AVX_LOAD_BASE4_3(ptr) \
    _mm_sub_epi16(_mm_sll_epi16(_mm_loadl_epi64((const __m128i*)(ptr)), inputShift), inputMidpoint)
#define AVX_LOAD_BASE4_4(ptr) _mm_loadl_epi64((const __m128i*)(ptr))
#define AVX_LOAD_BASE4(ptr) AVX_JOIN(AVX_LOAD_BASE4_, IN_FIXED_POINT)(ptr)

#define AVX_LOAD_BASE8_1(ptr) AVX_LOAD_INPUT_128_1(ptr)
#define AVX_LOAD_BASE8_2(ptr) AVX_LOAD_INPUT_128_2(ptr)
#define AVX_LOAD_BASE8_3(ptr) AVX_LOAD_INPUT_128_3(ptr)
#define AVX_LOAD_BASE8_4(ptr) AVX_LOAD_INPUT_128_4(ptr)
#define AVX_LOAD_BASE8(ptr) AVX_JOIN(AVX_LOAD_BASE8_, IN_FIXED_POINT)(ptr)

#define AVX_DEMOTE_U8(value) \
    _mm256_add_epi16(_mm256_srai_epi16(_mm256_adds_epi16((value), outputOffset256), 7), outputMidpoint256)
#define AVX_CONVERT_U16(value)                                                                   \
    _mm256_add_epi16(_mm256_sra_epi16(_mm256_adds_epi16((value), outputOffset256), outputShift), \
                     outputMidpoint256)
#define AVX_DEMOTE_U16(value)                                                           \
    (_mm256_min_epi16(_mm256_max_epi16(AVX_CONVERT_U16(value), _mm256_setzero_si256()), \
                      _mm256_set1_epi16((int16_t)(2 * params.midpoint - 1))))
#define AVX_STORE_OUTPUT_1(ptr, value) avxStoreU8x16((uint8_t*)(ptr), AVX_DEMOTE_U8(value))
#define AVX_STORE_OUTPUT_2(ptr, value) AVX_STORE_OUTPUT_1(ptr, value)
#define AVX_STORE_OUTPUT_3(ptr, value) _mm256_storeu_si256((__m256i*)(ptr), AVX_DEMOTE_U16(value))
#define AVX_STORE_OUTPUT_4(ptr, value) _mm256_storeu_si256((__m256i*)(ptr), (value))
#define AVX_STORE_OUTPUT(ptr, value) AVX_JOIN(AVX_STORE_OUTPUT_, OUT_FIXED_POINT)(ptr, value)

// Deinterleave a 256-bit register of 16 interleaved samples into 8 U and 8 V (128-bit each)
#define DEINTERLEAVE(reg, uOut, vOut)                                                 \
    do {                                                                              \
        const __m256i shuf = _mm256_shuffle_epi8((reg), deintShuf);                   \
        const __m256i perm = _mm256_permute4x64_epi64(shuf, _MM_SHUFFLE(3, 1, 2, 0)); \
        (uOut) = _mm256_castsi256_si128(perm);                                        \
        (vOut) = _mm256_extracti128_si256(perm, 1);                                   \
    } while (0)

// Store a 32-byte destination row chunk. When the destination rows are 32-byte
// aligned (per-function `ntStores` flag), use a non-temporal store: the
// destination is write-only, so bypassing the cache avoids read-for-ownership
// traffic and cache pollution on large planes. Callers must issue _mm_sfence()
// after the last store when ntStores is set.
#define AVX_STORE_ROW_WIDE(ptr, v)                     \
    do {                                               \
        if (ntStores) {                                \
            _mm256_stream_si256((__m256i*)(ptr), (v)); \
        } else {                                       \
            _mm256_storeu_si256((__m256i*)(ptr), (v)); \
        }                                              \
    } while (0)

#define AVX_STORE_ROW_1(ptr, value) AVX_STORE_OUTPUT_1(ptr, value)
#define AVX_STORE_ROW_2(ptr, value) AVX_STORE_OUTPUT_2(ptr, value)
#define AVX_STORE_ROW_3(ptr, value) AVX_STORE_ROW_WIDE(ptr, AVX_DEMOTE_U16(value))
#define AVX_STORE_ROW_4(ptr, value) AVX_STORE_ROW_WIDE(ptr, value)
#define AVX_STORE_ROW(ptr, value) AVX_JOIN(AVX_STORE_ROW_, OUT_FIXED_POINT)(ptr, value)

// Deinterleave a 256-bit register of 16 interleaved samples into one register
// with the 8 U samples in the low lane and the 8 V samples in the high lane
#define DEINTERLEAVE_256(reg, out)                                                                    \
    do {                                                                                              \
        (out) =                                                                                       \
            _mm256_permute4x64_epi64(_mm256_shuffle_epi8((reg), deintShuf), _MM_SHUFFLE(3, 1, 2, 0)); \
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

// Two-register byte shuffle applied identically to both 128-bit lanes
//
static inline __m256i shuffle2_256(__m256i v0, __m256i v1, __m256i control)
{
    const __m256i blend_mask = _mm256_cmpgt_epi8(control, _mm256_set1_epi8(15));
    const __m256i from_v0 = _mm256_shuffle_epi8(v0, control);
    const __m256i from_v1 = _mm256_shuffle_epi8(v1, _mm256_sub_epi8(control, _mm256_set1_epi8(16)));
    return _mm256_blendv_epi8(from_v0, from_v1, blend_mask);
}
