/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_SSE_H_
#define VN_DEC_CORE_SSE_H_

#include "lcevc_config.h"

#if VN_CORE_FEATURE(SSE)

#include "common/types.h"

#include <assert.h>
#include <immintrin.h>
#include <smmintrin.h>

/*------------------------------------------------------------------------------*/

typedef struct Vector2
{
    __m128i val[2];
} Vector2_t;

/*------------------------------------------------------------------------------*/

/*! \brief Safely read up to 16 lanes of 8-bit data from the src location. */
static inline __m128i loadVectorU8SSE(const void* src, const uint32_t lanes)
{
    if (lanes >= 16) {
        return _mm_loadu_si128((const __m128i*)src);
    }

    if (lanes == 8) {
        return _mm_loadl_epi64((const __m128i*)src);
    }

    VN_ALIGN(uint8_t temp[16], 16) = {0};
    memcpy(temp, src, lanes);
    return _mm_load_si128((const __m128i*)temp);
}

/*! \brief Safely read up to 8 lanes of 16-bit data from the src location. */
static inline __m128i loadVectorU16SSE(const void* src, const uint32_t lanes)
{
    if (lanes >= 8) {
        return _mm_loadu_si128((const __m128i*)src);
    }

    if (lanes == 4) {
        return _mm_loadl_epi64((const __m128i*)src);
    }

    VN_ALIGN(uint16_t temp[8], 16) = {0};
    memcpy(temp, src, lanes * sizeof(uint16_t));
    return _mm_load_si128((const __m128i*)temp);
}

/*! \brief Safely write up to 16 lanes of 8-bit data to the dst location. */
static inline void writeVectorU8SSE(void* dst, __m128i src, const uint32_t lanes)
{
    if (lanes >= 16) {
        _mm_storeu_si128((__m128i*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < minU32(lanes, 16); i++) {
        ((uint8_t*)dst)[i] = (uint8_t)_mm_extract_epi8(src, 0);
        src = _mm_alignr_epi8(src, src, 1);
    }
}

/*! \brief Safely write up to 8 lanes of 16-bit data to the dst location. */
static inline void writeVectorU16SSE(void* dst, __m128i src, const uint32_t lanes)
{
    if (lanes >= 8) {
        _mm_storeu_si128((__m128i*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < minU32(lanes, 8); i++) {
        ((uint16_t*)dst)[i] = (uint16_t)_mm_extract_epi16(src, 0);
        src = _mm_alignr_epi8(src, src, 2);
    }
}

/*------------------------------------------------------------------------------*/

/*! \brief Convert 16xU8 values to 16xS16 values, expanded from
 *         a single register to 2 registers. */
static inline Vector2_t expandU8ToS16SSE(const __m128i vec)
{
    const Vector2_t temp = {{
        _mm_cvtepu8_epi16(vec),
        _mm_cvtepu8_epi16(_mm_srli_si128(vec, 8)),
    }};

    return temp;
}

/*! \brief Convert 16xS8 values to 16xS16 values, expanded from
 *         a single register to 2 registers. */
static inline Vector2_t expandS8ToS16SSE(const __m128i vec)
{
    const Vector2_t temp = {{
        _mm_cvtepi8_epi16(vec),
        _mm_cvtepi8_epi16(_mm_srli_si128(vec, 8)),
    }};

    return temp;
}

/*! \brief Convert 8xS16 values to 8xS32 values, expanded from
 *         a single register to 2 registers. */
static inline Vector2_t expandS16ToS32SSE(const __m128i vec)
{
    const Vector2_t temp = {{
        _mm_cvtepi16_epi32(vec),
        _mm_cvtepi16_epi32(_mm_srli_si128(vec, 8)),
    }};

    return temp;
}

/*------------------------------------------------------------------------------*/

/*! \brief Safely read up to 16 lanes of unsigned values from the data,
 *         whereby the size of each lane is parametrized. These are then
 *         cast to S16.
 *
 * \param src           The location to read from.
 * \param lanes         The number of lanes to load.
 * \param loadLaneSize  The byte size of each source lane.
 */
static inline Vector2_t loadVector2UNAsS16SSE(const uint8_t* src, uint32_t lanes, uint32_t loadLaneSize)
{
    if (loadLaneSize == 1) {
        return expandU8ToS16SSE(loadVectorU8SSE(src, lanes));
    }

    assert(loadLaneSize == 2);
    const uint32_t lanes1 = lanes >= 8 ? lanes - 8 : 0;
    const Vector2_t result = {{loadVectorU16SSE(src, lanes), loadVectorU16SSE(src + 16, lanes1)}};
    return result;
}

/*! \brief Safely write up to 16 lanes of S16 data as unsigned values,
 *         performing the appropriate clamping based upon both the destination
 *         lane width and a clamp value.
 *
 * \param dst             The location to write to.
 * \param vector          The registers containing 16 lanes of S16 data.
 * \param lanes           The number of lanes to write.
 * \param writePixelSize  The byte size of each destination lane.
 * \param clamp           A register containing the upper bound to clamp to.
 */
static inline void writeVector2S16AsUNSSE(uint8_t* dst, const Vector2_t* vector, uint32_t lanes,
                                          uint32_t writeLaneSize, __m128i clamp)
{
    if (writeLaneSize == 1) {
        writeVectorU8SSE(dst, _mm_packus_epi16(vector->val[0], vector->val[1]), lanes);
        return;
    }

    assert(writeLaneSize == 2);
    const uint32_t lanes1 = lanes >= 8 ? lanes - 8 : 0;

    writeVectorU16SSE(dst, _mm_max_epi16(_mm_min_epi16(vector->val[0], clamp), _mm_setzero_si128()), lanes);
    writeVectorU16SSE(dst + 16,
                      _mm_max_epi16(_mm_min_epi16(vector->val[1], clamp), _mm_setzero_si128()), lanes1);
}

/*------------------------------------------------------------------------------*/

#if (VN_COMPILER(GCC) && (__GNUC__ < 11)) || (VN_COMPILER(CLANG) && (__clang_major__ <= 8))

static inline __m128i _mm_loadu_si16(const void* src) { return _mm_cvtsi32_si128(*(int16_t*)src); }

static inline void _mm_storeu_si16(void* dst, __m128i value)
{
    *(int16_t*)dst = (int16_t)_mm_cvtsi128_si32(value);
}

static inline __m128i _mm_loadu_si32(const void* src) { return _mm_cvtsi32_si128(*(int32_t*)src); }

static inline void _mm_storeu_si32(void* dst, __m128i value)
{
    *(int32_t*)dst = _mm_cvtsi128_si32(value);
}

#endif

/*------------------------------------------------------------------------------*/

#endif
#endif /* VN_DEC_CORE_SSE_H_ */