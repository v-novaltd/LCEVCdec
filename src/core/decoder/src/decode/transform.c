/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "decode/transform.h"

#include "common/simd.h"
#include "decode/dequant.h"

/* clang-format off */

/*------------------------------------------------------------------------------*/

/* Performs dequantisation on an array of coefficients. This is the standard
 * LCEVC implementation. */
static inline void dequantScalarImpl(const Dequant_t* dequant, TemporalSignal_t temporalSignal, int32_t numLayers,
                      const int16_t* coeffs, int16_t* dequantizedCoeffs)
{
    for (int32_t i = 0; i < numLayers; i++) {
        const int16_t coeffSign = (coeffs[i] > 0) ? 1 : ((coeffs[i] < 0) ? (-1) : 0);

        /* Simple dequant */
        dequantizedCoeffs[i] = (int16_t)(coeffs[i] * dequant->stepWidth[temporalSignal][i]);

        /* Applying dead zone */
        dequantizedCoeffs[i] = (int16_t)(dequantizedCoeffs[i] + (int16_t)(coeffSign * dequant->offset[temporalSignal][i]));
    }
}

/* Non-static wrapper to expose this functionality outside of this module for testing purposes. */
void dequantScalar(const Dequant_t* dequant, TemporalSignal_t temporalSignal, int32_t numLayers,
                      const int16_t* coeffs, int16_t* dequantizedCoeffs)
{
    dequantScalarImpl(dequant, temporalSignal, numLayers, coeffs, dequantizedCoeffs);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDD1D(const int16_t* coeffs, int16_t* residuals)
{
    residuals[0] = saturateS16(coeffs[0] + coeffs[1] + coeffs[2]);
    residuals[1] = saturateS16(coeffs[0] - coeffs[1] - coeffs[2]);
    residuals[2] = saturateS16(coeffs[3] + coeffs[1] - coeffs[2]);
    residuals[3] = saturateS16(coeffs[3] - coeffs[1] + coeffs[2]);
}

void dequantInverseDD1D(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                        const int16_t* coeffs, int16_t* residuals)
{
    int16_t dqCoeffs[RCLayerCountDD];
    dequantScalarImpl(dequant, temporalSignal, RCLayerCountDD, coeffs, dqCoeffs);
    inverseDD1D(dqCoeffs, residuals);
}

static inline void inverseDD2D(const int16_t* coeffs, int16_t* residuals)
{
    residuals[0] = saturateS16(coeffs[0] + coeffs[1] + coeffs[2] + coeffs[3]);
    residuals[1] = saturateS16(coeffs[0] - coeffs[1] + coeffs[2] - coeffs[3]);
    residuals[2] = saturateS16(coeffs[0] + coeffs[1] - coeffs[2] - coeffs[3]);
    residuals[3] = saturateS16(coeffs[0] - coeffs[1] - coeffs[2] + coeffs[3]);
}

void dequantInverseDD2D(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                        const int16_t* coeffs, int16_t* residuals)
{
    int16_t dqCoeffs[RCLayerCountDD];
    dequantScalarImpl(dequant, temporalSignal, RCLayerCountDD, coeffs, dqCoeffs);
    inverseDD2D(dqCoeffs, residuals);
}

void inverseDDS1D(const int16_t* coeffs, int16_t* residuals)
{
    const int32_t a[4] =
    {
        + coeffs[0]  + coeffs[1]  + coeffs[2]  + coeffs[3],
        + coeffs[4]  + coeffs[5]  + coeffs[6]  + coeffs[7],
        + coeffs[8]  + coeffs[9]  + coeffs[10] + coeffs[11],
        + coeffs[12] + coeffs[13] + coeffs[14] + coeffs[15],
    };

    const int32_t h[4] =
    {
        + coeffs[0]  - coeffs[1]  + coeffs[2]  - coeffs[3],
        + coeffs[4]  - coeffs[5]  + coeffs[6]  - coeffs[7],
        + coeffs[8]  - coeffs[9]  + coeffs[10] - coeffs[11],
        + coeffs[12] - coeffs[13] + coeffs[14] - coeffs[15],
    };

    const int32_t v[4] =
    {
        + coeffs[0]  + coeffs[1]  - coeffs[2]  - coeffs[3],
        + coeffs[4]  + coeffs[5]  - coeffs[6]  - coeffs[7],
        + coeffs[8]  + coeffs[9]  - coeffs[10] - coeffs[11],
        + coeffs[12] + coeffs[13] - coeffs[14] - coeffs[15],
    };

    const int32_t d[4] =
    {
        + coeffs[0]  - coeffs[1]  - coeffs[2]  + coeffs[3],
        + coeffs[4]  - coeffs[5]  - coeffs[6]  + coeffs[7],
        + coeffs[8]  - coeffs[9]  - coeffs[10] + coeffs[11],
        + coeffs[12] - coeffs[13] - coeffs[14] + coeffs[15],
    };

    /* AA, AH, AV, AD */
    residuals[0]  = saturateS16(a[0] + a[1] + a[3]);
    residuals[1]  = saturateS16(a[0] - a[1] - a[3]);
    residuals[2]  = saturateS16(a[1] + a[2] - a[3]);
    residuals[3]  = saturateS16(a[2] - a[1] + a[3]);

    /* HA, HH, HV, HD */
    residuals[4]  = saturateS16(h[0] + h[1] + h[3]);
    residuals[5]  = saturateS16(h[0] - h[1] - h[3]);
    residuals[6]  = saturateS16(h[1] + h[2] - h[3]);
    residuals[7]  = saturateS16(h[2] - h[1] + h[3]);

    /* VA, VH, VV, VD */
    residuals[8]  = saturateS16(v[0] + v[1] + v[3]);
    residuals[9]  = saturateS16(v[0] - v[1] - v[3]);
    residuals[10] = saturateS16(v[1] + v[2] - v[3]);
    residuals[11] = saturateS16(v[2] - v[1] + v[3]);

    /* DA, DH, DV, DD */
    residuals[12] = saturateS16(d[0] + d[1] + d[3]);
    residuals[13] = saturateS16(d[0] - d[1] - d[3]);
    residuals[14] = saturateS16(d[1] + d[2] - d[3]);
    residuals[15] = saturateS16(d[2] - d[1] + d[3]);
}

void dequantInverseDDS1D(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                        const int16_t* coeffs, int16_t* residuals)
{
    int16_t dqCoeffs[RCLayerCountDDS];
    dequantScalarImpl(dequant, temporalSignal, RCLayerCountDDS, coeffs, dqCoeffs);
    inverseDDS1D(dqCoeffs, residuals);
}

void inverseDDS2D(const int16_t* coeffs, int16_t* residuals)
{
    const int32_t a[4] =
    {
        coeffs[0]  + coeffs[1]  + coeffs[2]  + coeffs[3],
        coeffs[4]  + coeffs[5]  + coeffs[6]  + coeffs[7],
        coeffs[8]  + coeffs[9]  + coeffs[10] + coeffs[11],
        coeffs[12] + coeffs[13] + coeffs[14] + coeffs[15]
    };

    const int32_t h[4] = 
    {
        coeffs[0]  - coeffs[1]  + coeffs[2]  - coeffs[3],
        coeffs[4]  - coeffs[5]  + coeffs[6]  - coeffs[7],
        coeffs[8]  - coeffs[9]  + coeffs[10] - coeffs[11],
        coeffs[12] - coeffs[13] + coeffs[14] - coeffs[15]
    };

    const int32_t v[4] =
    {
        coeffs[0]  + coeffs[1]  - coeffs[2]  - coeffs[3],
        coeffs[4]  + coeffs[5]  - coeffs[6]  - coeffs[7],
        coeffs[8]  + coeffs[9]  - coeffs[10] - coeffs[11],
        coeffs[12] + coeffs[13] - coeffs[14] - coeffs[15]
    };

    const int32_t d[4] = 
    {
        coeffs[0]  - coeffs[1]  - coeffs[2]  + coeffs[3],
        coeffs[4]  - coeffs[5]  - coeffs[6]  + coeffs[7],
        coeffs[8]  - coeffs[9]  - coeffs[10] + coeffs[11],
        coeffs[12] - coeffs[13] - coeffs[14] + coeffs[15]
    };

    /* AA, AH, AV, AD */
    residuals[0]  = saturateS16(a[0] + a[1] + a[2] + a[3]);
    residuals[1]  = saturateS16(a[0] - a[1] + a[2] - a[3]);
    residuals[2]  = saturateS16(a[0] + a[1] - a[2] - a[3]);
    residuals[3]  = saturateS16(a[0] - a[1] - a[2] + a[3]);

    /* HA, HH, HV, HD */
    residuals[4]  = saturateS16(h[0] + h[1] + h[2] + h[3]);
    residuals[5]  = saturateS16(h[0] - h[1] + h[2] - h[3]);
    residuals[6]  = saturateS16(h[0] + h[1] - h[2] - h[3]);
    residuals[7]  = saturateS16(h[0] - h[1] - h[2] + h[3]);

    /* VA, VH, VV, VD */
    residuals[8]  = saturateS16(v[0] + v[1] + v[2] + v[3]);
    residuals[9]  = saturateS16(v[0] - v[1] + v[2] - v[3]);
    residuals[10] = saturateS16(v[0] + v[1] - v[2] - v[3]);
    residuals[11] = saturateS16(v[0] - v[1] - v[2] + v[3]);

    /* DA, DH, DV, DD */
    residuals[12] = saturateS16(d[0] + d[1] + d[2] + d[3]);
    residuals[13] = saturateS16(d[0] - d[1] + d[2] - d[3]);
    residuals[14] = saturateS16(d[0] + d[1] - d[2] - d[3]);
    residuals[15] = saturateS16(d[0] - d[1] - d[2] + d[3]);
}


void dequantInverseDDS2D(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                        const int16_t* coeffs, int16_t* residuals)
{
	int16_t dqCoeffs[RCLayerCountDDS];
	dequantScalarImpl(dequant, temporalSignal, RCLayerCountDDS, coeffs, dqCoeffs);
    inverseDDS2D(dqCoeffs, residuals);
}

#if VN_CORE_FEATURE(SSE)

/*------------------------------------------------------------------------------*/

static inline __m128i dequantDD_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                                 const int16_t* coeffs)
{
    const __m128i data = _mm_loadl_epi64((const __m128i*)coeffs);

    /* value *= stepWidth */
    __m128i tmp = _mm_mullo_epi16(data, dequant->stepWidthVector[temporalSignal][0]);

    /* value += sign * offset */
    return _mm_add_epi16(tmp, _mm_sign_epi16(dequant->offsetVector[temporalSignal][0], data));
}

static inline void dequantDDS_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                                  const int16_t* coeffs, __m128i dequantizedCoeffs[2])
{
    const __m128i coeffs0 = _mm_load_si128((const __m128i*)&coeffs[0]);
    const __m128i coeffs1 = _mm_load_si128((const __m128i*)&coeffs[8]);

    /* value *= stepWidth */
    __m128i tmp0 = _mm_mullo_epi16(coeffs0, dequant->stepWidthVector[temporalSignal][0]);
    __m128i tmp1 = _mm_mullo_epi16(coeffs1, dequant->stepWidthVector[temporalSignal][1]);

    /* value += sign * offset */
    dequantizedCoeffs[0] = _mm_add_epi16(tmp0, _mm_sign_epi16(dequant->offsetVector[temporalSignal][0], coeffs0));
    dequantizedCoeffs[1] = _mm_add_epi16(tmp1, _mm_sign_epi16(dequant->offsetVector[temporalSignal][1], coeffs1));
}

/*------------------------------------------------------------------------------*/

static inline void inverseDD1DImpl_SSE(__m128i ahvd, int16_t* residuals)
{
    /* Re-order coefficients and negate some of them so we can
       add across all lanes and perform lane specific behavior. */

    const __m128i signMask0 = _mm_setr_epi32(1, -1, 1, 1);
    const __m128i signMask1 = _mm_setr_epi32(1, -1, -1, -1);

    ahvd = _mm_cvtepi16_epi32(ahvd);
                                                                                   /* [A,  H,  V,  D] */
    const __m128i col0 = _mm_shuffle_epi32(ahvd, 0xF0);                            /* [0,  0,  3,  3] */
    const __m128i col1 = _mm_sign_epi32(_mm_shuffle_epi32(ahvd, 0x95), signMask0); /* [1, -1,  1,  2] */
    const __m128i col2 = _mm_sign_epi32(_mm_shuffle_epi32(ahvd, 0x6A), signMask1); /* [2, -2, -2, -1] */
    const __m128i result = _mm_add_epi32(_mm_add_epi32(col0, col1), col2);

    _mm_storel_epi64((__m128i*)residuals, _mm_packs_epi32(result, result));
}

void inverseDD1D_SSE(const int16_t* coeffs, int16_t* residuals)
{
    inverseDD1DImpl_SSE(_mm_loadl_epi64((const __m128i*)coeffs), residuals);
}

void dequantInverseDD1D_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                            const int16_t* coeffs, int16_t* residuals)
{
    inverseDD1DImpl_SSE(dequantDD_SSE(dequant, temporalSignal, coeffs), residuals);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDD2DImpl_SSE(__m128i ahvd, int16_t* residuals)
{
    /* Re-order coefficients and negate some of them so we can
       add across all lanes and perform lane specific behavior.
       In this case negate the last 2 arguments for the A calculation
       as we then perform subs on them, thus making them act as though
       it's 3 adds. */
    const __m128i signMask = _mm_setr_epi32(-1, 1, 1, 1);
    ahvd = _mm_cvtepi16_epi32(ahvd);
                                                                                  /* [ A, H, V, D] */
    const __m128i col0 = _mm_shuffle_epi32(ahvd, 0x00);                           /* [ 0, 0, 0, 0] */
    const __m128i col1 = _mm_shuffle_epi32(ahvd, 0xD9);                           /* [ 1, 2, 1, 3] */
    const __m128i col2 = _mm_sign_epi32(_mm_shuffle_epi32(ahvd, 0x66), signMask); /* [-2, 1, 2, 1] */
    const __m128i col3 = _mm_sign_epi32(_mm_shuffle_epi32(ahvd, 0xBF), signMask); /* [-3, 3, 3, 2] */
    const __m128i result = _mm_sub_epi32(_mm_sub_epi32(_mm_add_epi32(col0, col1), col2), col3);

    _mm_storel_epi64((__m128i*)residuals, _mm_packs_epi32(result, result));
}

void inverseDD2D_SSE(const int16_t* coeffs, int16_t* residuals)
{
    inverseDD2DImpl_SSE(_mm_loadl_epi64((const __m128i*)coeffs), residuals);
}

void dequantInverseDD2D_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                            const int16_t* coeffs, int16_t* residuals)
{
    inverseDD2DImpl_SSE(dequantDD_SSE(dequant, temporalSignal, coeffs), residuals);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDDS1DImpl_SSE(const __m128i coeffs[2], int16_t* residuals)
{
    const __m128i a = _mm_cvtepi16_epi32(coeffs[0]);
    const __m128i h = _mm_cvtepi16_epi32(_mm_srli_si128(coeffs[0], 8));
    const __m128i v = _mm_cvtepi16_epi32(coeffs[1]);
    const __m128i d = _mm_cvtepi16_epi32(_mm_srli_si128(coeffs[1], 8));

    /* 1st pass */
    __m128i row0 = _mm_add_epi32(_mm_add_epi32(a, h), d); /* A00 A01 A10 A11 */
    __m128i row1 = _mm_sub_epi32(_mm_sub_epi32(a, h), d); /* H00 H01 H10 H11 */
    __m128i row2 = _mm_sub_epi32(_mm_add_epi32(h, v), d); /* V00 V01 V10 V11 */
    __m128i row3 = _mm_sub_epi32(_mm_add_epi32(v, d), h); /* D00 D01 D10 D11 */

    /* Transpose */
    __m128i temp0 = _mm_unpacklo_epi32(row0, row1); /* A00 H00 A01 H01 */
    __m128i temp1 = _mm_unpackhi_epi32(row0, row1); /* A10 H10 A11 H11 */
    __m128i temp2 = _mm_unpacklo_epi32(row2, row3); /* V00 D00 V01 D01 */
    __m128i temp3 = _mm_unpackhi_epi32(row2, row3); /* V10 D10 V11 D11 */

    row0 = _mm_unpacklo_epi64(temp0, temp2); /* A00 H00 V00 D00 */
    row1 = _mm_unpackhi_epi64(temp0, temp2); /* A01 H01 V01 D01 */
    row2 = _mm_unpacklo_epi64(temp1, temp3); /* A10 H10 V10 D10 */
    row3 = _mm_unpackhi_epi64(temp1, temp3); /* A11 H11 V11 D11 */

    /* 2nd pass */
    temp0 = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(row0, row1), row2), row3); /* AA AH AV AD */
    temp1 = _mm_sub_epi32(_mm_add_epi32(_mm_sub_epi32(row0, row1), row2), row3); /* HA HH HV HD */
    temp2 = _mm_sub_epi32(_mm_sub_epi32(_mm_add_epi32(row0, row1), row2), row3); /* VA VH VV VD */
    temp3 = _mm_add_epi32(_mm_sub_epi32(_mm_sub_epi32(row0, row1), row2), row3); /* DA DH DV DD */

    /* Output */
    _mm_store_si128((__m128i*)&residuals[0], _mm_packs_epi32(temp0, temp1));
    _mm_store_si128((__m128i*)&residuals[8], _mm_packs_epi32(temp2, temp3));
}

void inverseDDS1D_SSE(const int16_t* coeffs, int16_t* residuals)
{
    const __m128i coeffsV[2] = { _mm_load_si128((const __m128i*)&coeffs[0]),
                                 _mm_load_si128((const __m128i*)&coeffs[8]) };
    inverseDDS1DImpl_SSE(coeffsV, residuals);
}

void dequantInverseDDS1D_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                             const int16_t* coeffs, int16_t* residuals)
{
    __m128i dqCoeffs[2];
    dequantDDS_SSE(dequant, temporalSignal, coeffs, dqCoeffs);
    inverseDDS1DImpl_SSE(dqCoeffs, residuals);
}

/*------------------------------------------------------------------------------*/

void inverseDDS2DImpl_SSE(const __m128i coeffs[2], int16_t* residuals)
{
    const __m128i a = _mm_cvtepi16_epi32(coeffs[0]);
    const __m128i h = _mm_cvtepi16_epi32(_mm_srli_si128(coeffs[0], 8));
    const __m128i v = _mm_cvtepi16_epi32(coeffs[1]);
    const __m128i d = _mm_cvtepi16_epi32(_mm_srli_si128(coeffs[1], 8));

    /* 1st pass */
    __m128i row0 = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(a, h), v), d); /* A00 A01 A10 A11 */
    __m128i row1 = _mm_sub_epi32(_mm_add_epi32(_mm_sub_epi32(a, h), v), d); /* H00 H01 H10 H11 */
    __m128i row2 = _mm_sub_epi32(_mm_sub_epi32(_mm_add_epi32(a, h), v), d); /* V00 V01 V10 V11 */
    __m128i row3 = _mm_add_epi32(_mm_sub_epi32(_mm_sub_epi32(a, h), v), d); /* D00 D01 D10 D11 */

    /* Transpose */
    __m128i temp0 = _mm_unpacklo_epi32(row0, row1); /* A00 H00 A01 H01 */
    __m128i temp1 = _mm_unpackhi_epi32(row0, row1); /* A10 H10 A11 H11 */
    __m128i temp2 = _mm_unpacklo_epi32(row2, row3); /* V00 D00 V01 D01 */
    __m128i temp3 = _mm_unpackhi_epi32(row2, row3); /* V10 D10 V11 D11 */

    row0 = _mm_unpacklo_epi64(temp0, temp2); /* A00 H00 V00 D00 */
    row1 = _mm_unpackhi_epi64(temp0, temp2); /* A01 H01 V01 D01 */
    row2 = _mm_unpacklo_epi64(temp1, temp3); /* A10 H10 V10 D10 */
    row3 = _mm_unpackhi_epi64(temp1, temp3); /* A11 H11 V11 D11 */

    /* 2nd pass */
    temp0 = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(row0, row1), row2), row3); /* AA AH AV AD */
    temp1 = _mm_sub_epi32(_mm_add_epi32(_mm_sub_epi32(row0, row1), row2), row3); /* HA HH HV HD */
    temp2 = _mm_sub_epi32(_mm_sub_epi32(_mm_add_epi32(row0, row1), row2), row3); /* VA VH VV VD */
    temp3 = _mm_add_epi32(_mm_sub_epi32(_mm_sub_epi32(row0, row1), row2), row3); /* DA DH DV DD */

    /* Output */
    _mm_store_si128((__m128i*)&residuals[0], _mm_packs_epi32(temp0, temp1));
    _mm_store_si128((__m128i*)&residuals[8], _mm_packs_epi32(temp2, temp3));
}

void inverseDDS2D_SSE(const int16_t* coeffs, int16_t* residuals)
{
    const __m128i coeffsV[2] = { _mm_load_si128((const __m128i*)&coeffs[0]),
                                 _mm_load_si128((const __m128i*)&coeffs[8]) };
    inverseDDS2DImpl_SSE(coeffsV, residuals);
}

void dequantInverseDDS2D_SSE(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                             const int16_t* coeffs, int16_t* residuals)
{
    __m128i coeffsV[2];
    dequantDDS_SSE(dequant, temporalSignal, coeffs, coeffsV);
    inverseDDS2DImpl_SSE(coeffsV, residuals);
}

/*------------------------------------------------------------------------------*/

#endif

#if VN_CORE_FEATURE(NEON)

/*------------------------------------------------------------------------------*/

static int16x4_t vsign_s16(int16x4_t a, int16x4_t b)
{
    /* (b < 0) ? 0xFFFF : 0 */
    const uint16x4_t ltMask = vreinterpret_u16_s16(vshr_n_s16(b, 15));

    /* (b == 0) ? 0xFFFF : 0 */
#if defined(__aarch64__)
    const int16x4_t zeroMask = vreinterpret_s16_u16(vceqz_s16(b));
#else
    const int16x4_t zeroMask = vreinterpret_s16_u16(vceq_s16(b, vdup_n_s16(0)));
#endif

    /* bitwise select either a or negative 'a' based on ltMask */
    const int16x4_t masked = vbsl_s16(ltMask, vneg_s16(a), a);

    /* res = masked & (~zeroMask) */
    return vbic_s16(masked, zeroMask);
}

static int16x8_t vsignq_s16(int16x8_t a, int16x8_t b)
{
    /* (b < 0) ? 0xFFFF : 0 */
    const uint16x8_t ltMask = vreinterpretq_u16_s16(vshrq_n_s16(b, 15));

    /* (b == 0) ? 0xFFFF : 0 */
#if defined(__aarch64__)
    const int16x8_t zeroMask = vreinterpretq_s16_u16(vceqzq_s16(b));
#else
    const int16x8_t zeroMask = vreinterpretq_s16_u16(vceqq_s16(b, vdupq_n_s16(0)));
#endif

    /* bitwise select either a or negative 'a' based on ltMask */
    const int16x8_t masked = vbslq_s16(ltMask, vnegq_s16(a), a);

    /* res = masked & (~zeroMask) */
    return vbicq_s16(masked, zeroMask);
}

static inline int16x4_t dequantDD_NEON(const Dequant_t* dequant, TemporalSignal_t temporal,
                                   const int16_t* coeffs)
{
    const int16x4_t data = vld1_s16(coeffs);

    /* value *= stepwidth */
    const int16x4_t tmp = vmul_s16(data, vget_low_s16(dequant->stepWidthVector[temporal][0]));

    /* value += sign * offset */
    return vadd_s16(tmp, vsign_s16(vget_low_s16(dequant->offsetVector[temporal][0]), data));
}

static inline void dequantDDS_NEON(const Dequant_t* dequant, TemporalSignal_t temporal,
                                   const int16_t* coeffs, int16x8_t dequantizedCoeffs[2])
{
    const int16x8_t coeffs0 = vld1q_s16(&coeffs[0]);
    const int16x8_t coeffs1 = vld1q_s16(&coeffs[8]);

    /* value *= stepWidth */
    const int16x8_t tmp0 = vmulq_s16(coeffs0, dequant->stepWidthVector[temporal][0]);
    const int16x8_t tmp1 = vmulq_s16(coeffs1, dequant->stepWidthVector[temporal][1]);

    /* value += sign * offset */
    dequantizedCoeffs[0] = vaddq_s16(tmp0, vsignq_s16(dequant->offsetVector[temporal][0], coeffs0));
    dequantizedCoeffs[1] = vaddq_s16(tmp1, vsignq_s16(dequant->offsetVector[temporal][1], coeffs1));
}

/*------------------------------------------------------------------------------*/

static inline void inverseDD1DImpl_NEON(const int16x4_t coeffs, int16_t* residuals)
{
    const int32x4_t ahvd = vmovl_s16(coeffs);
    const int32x4_t hvda = vextq_s32(ahvd, ahvd, 1);
    const int32x4_t vdah = vextq_s32(ahvd, ahvd, 2);

    // These cant be static for reasons.
    const int32x4_t negateMask0 = vmovl_s16(vcreate_s16(0xFFFF000100010001));
    const int32x4_t negateMask1 = vmovl_s16(vcreate_s16(0x0001FFFFFFFF0001));
    const int32x4_t negateMask2 = vmovl_s16(vcreate_s16(0x00010001FFFF0001));

    const int32x4_t data0 = vmulq_s32(vzip1q_s32(ahvd, ahvd), negateMask0);
    const int32x4_t data1 = vmulq_s32(vzip1q_s32(hvda, hvda), negateMask1);
    const int32x4_t data2 = vmulq_s32(vzip1q_s32(vdah, vdah), negateMask2);
    
    const int32x4_t result = vaddq_s32(data0, vaddq_s32(data1, data2));
    vst1_s16(residuals, vqmovn_s32(result));
}

void inverseDD1D_NEON(const int16_t* coeffs, int16_t* residuals)
{
    inverseDD1DImpl_NEON(vld1_s16(coeffs), residuals);
}

void dequantInverseDD1D_NEON(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                            const int16_t* coeffs, int16_t* residuals)
{
    inverseDD1DImpl_NEON(dequantDD_NEON(dequant, temporalSignal, coeffs), residuals);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDD2DImpl_NEON(const int16x4_t coeffs, int16_t* residuals)
{
    const int32x4_t ahvd = vmovl_s16(coeffs);
    const int32x4_t hvda = vextq_s32(ahvd, ahvd, 1);
    const int32x4_t vdah = vextq_s32(ahvd, ahvd, 2);
    const int32x4_t dahv = vextq_s32(ahvd, ahvd, 3);

    const int32x4_t negateMask0 = vmovl_s16(vcreate_s16(0x0001FFFFFFFF0001));
    const int32x4_t negateMask1 = vmovl_s16(vcreate_s16(0x0001FFFF00010001));
    const int32x4_t negateMask2 = vmovl_s16(vcreate_s16(0xFFFF0001FFFF0001));
    const int32x4_t negateMask3 = vmovl_s16(vcreate_s16(0xFFFF000100010001));

    const int32x4_t data0 = vmulq_s32(ahvd, negateMask0);
    const int32x4_t data1 = vmulq_s32(hvda, negateMask1);
    const int32x4_t data2 = vmulq_s32(vdah, negateMask2);
    const int32x4_t data3 = vmulq_s32(dahv, negateMask3);
    
    const int32x4_t result = vaddq_s32(data0, vaddq_s32(data1, vaddq_s32(data2, data3)));

    vst1_s16(residuals, vqmovn_s32(result));
}

void inverseDD2D_NEON(const int16_t* coeffs, int16_t* residuals)
{
    inverseDD2DImpl_NEON(vld1_s16(coeffs), residuals);
}

void dequantInverseDD2D_NEON(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                            const int16_t* coeffs, int16_t* residuals)
{
    inverseDD2DImpl_NEON(dequantDD_NEON(dequant, temporalSignal, coeffs), residuals);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDDSPass2_NEON(int32x4x4_t* coeffs, int16_t* residuals)
{
    /* Transpose */
#if VN_ARCH(ARM7A)
    const int32x4x2_t tempA = vtrnq_s32(coeffs->val[0], coeffs->val[1]); /* val[0] = A00 H00 A10 H10
                                                                            val[1] = A01 H01 A11 H11 */
    const int32x4x2_t tempB = vtrnq_s32(coeffs->val[2], coeffs->val[3]); /* val[0] = V00 D00 V10 D10
                                                                            val[1] = V01 D01 V11 D11 */

    const int32x4_t temp0 = vcombine_s32(vget_low_s32(tempA.val[0]), vget_low_s32(tempB.val[0]));   /* A00 H00 V00 D00 */
    const int32x4_t temp1 = vcombine_s32(vget_low_s32(tempA.val[1]), vget_low_s32(tempB.val[1]));   /* A01 H01 V01 D01 */
    const int32x4_t temp2 = vcombine_s32(vget_high_s32(tempA.val[0]), vget_high_s32(tempB.val[0])); /* A10 H10 V10 D10 */
    const int32x4_t temp3 = vcombine_s32(vget_high_s32(tempA.val[1]), vget_high_s32(tempB.val[1])); /* A11 H11 V11 D11 */

#else
    const int32x4_t tempA = vtrn1q_s32(coeffs->val[0], coeffs->val[1]); /* A00 H00 A10 H10 */
    const int32x4_t tempB = vtrn2q_s32(coeffs->val[0], coeffs->val[1]); /* A01 H01 A11 H11 */
    const int32x4_t tempC = vtrn1q_s32(coeffs->val[2], coeffs->val[3]); /* V00 D00 V10 D10 */
    const int32x4_t tempD = vtrn2q_s32(coeffs->val[2], coeffs->val[3]); /* V01 D01 V11 D11 */

    const int32x4_t temp0 = vreinterpretq_s32_s64(vtrn1q_s64(vreinterpretq_s64_s32(tempA), vreinterpretq_s64_s32(tempC))); /* A00 H00 V00 D00 */
    const int32x4_t temp1 = vreinterpretq_s32_s64(vtrn1q_s64(vreinterpretq_s64_s32(tempB), vreinterpretq_s64_s32(tempD))); /* A01 H01 V01 D01 */
    const int32x4_t temp2 = vreinterpretq_s32_s64(vtrn2q_s64(vreinterpretq_s64_s32(tempA), vreinterpretq_s64_s32(tempC))); /* A10 H10 V10 D10 */
    const int32x4_t temp3 = vreinterpretq_s32_s64(vtrn2q_s64(vreinterpretq_s64_s32(tempB), vreinterpretq_s64_s32(tempD))); /* A11 H11 V11 D11 */
#endif

    /* 2nd pass */
    const int32x4_t result0 = vaddq_s32(vaddq_s32(vaddq_s32(temp0, temp1), temp2), temp3); /* AA AH AV AD */
    const int32x4_t result1 = vsubq_s32(vaddq_s32(vsubq_s32(temp0, temp1), temp2), temp3); /* HA HH HV HD */
    const int32x4_t result2 = vsubq_s32(vsubq_s32(vaddq_s32(temp0, temp1), temp2), temp3); /* VA VH VV VD */
    const int32x4_t result3 = vaddq_s32(vsubq_s32(vsubq_s32(temp0, temp1), temp2), temp3); /* DA DH DV DD */

    /* Store */
    vst1q_s16(&residuals[0], vcombine_s16(vqmovn_s32(result0), vqmovn_s32(result1)));
    vst1q_s16(&residuals[8], vcombine_s16(vqmovn_s32(result2), vqmovn_s32(result3)));
}

static inline void inverseDDS1DImpl_NEON(const int16x8_t coeffs[2], int16_t* residuals)
{
    const int32x4_t a = vmovl_s16(vget_low_s16(coeffs[0]));
    const int32x4_t h = vmovl_s16(vget_high_s16(coeffs[0]));
    const int32x4_t v = vmovl_s16(vget_low_s16(coeffs[1]));
    const int32x4_t d = vmovl_s16(vget_high_s16(coeffs[1]));

    int32x4x4_t firstPass;
    firstPass.val[0] = vaddq_s32(vaddq_s32(a, h), d); /* A00 A01 A10 A11 */
    firstPass.val[1] = vsubq_s32(vsubq_s32(a, h), d); /* H00 H01 H10 H11 */
    firstPass.val[2] = vsubq_s32(vaddq_s32(h, v), d); /* V00 V01 V10 V11 */
    firstPass.val[3] = vsubq_s32(vaddq_s32(v, d), h); /* D00 D01 D10 D11 */

    inverseDDSPass2_NEON(&firstPass, residuals);
}

void inverseDDS1D_NEON(const int16_t* coeffs, int16_t* residuals)
{
    const int16x8_t coeffsV[2] = { vld1q_s16(&coeffs[0]),
                                   vld1q_s16(&coeffs[8]) };
    inverseDDS1DImpl_NEON(coeffsV, residuals);
}

void dequantInverseDDS1D_NEON(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                              const int16_t* coeffs, int16_t* residuals)
{
    int16x8_t dqCoeffs[2];
    dequantDDS_NEON(dequant, temporalSignal, coeffs, dqCoeffs);
    inverseDDS1DImpl_NEON(dqCoeffs, residuals);
}

/*------------------------------------------------------------------------------*/

static inline void inverseDDS2DImpl_NEON(const int16x8_t coeffs[2], int16_t* residuals)
{
    const int32x4_t a = vmovl_s16(vget_low_s16(coeffs[0]));
    const int32x4_t h = vmovl_s16(vget_high_s16(coeffs[0]));
    const int32x4_t v = vmovl_s16(vget_low_s16(coeffs[1]));
    const int32x4_t d = vmovl_s16(vget_high_s16(coeffs[1]));

    int32x4x4_t firstPass;
    firstPass.val[0] = vaddq_s32(vaddq_s32(vaddq_s32(a, h), v), d); /* A00 A01 A10 A11 */
    firstPass.val[1] = vsubq_s32(vaddq_s32(vsubq_s32(a, h), v), d); /* H00 H01 H10 H11 */
    firstPass.val[2] = vsubq_s32(vsubq_s32(vaddq_s32(a, h), v), d); /* V00 V01 V10 V11 */
    firstPass.val[3] = vaddq_s32(vsubq_s32(vsubq_s32(a, h), v), d); /* D00 D01 D10 D11 */

    inverseDDSPass2_NEON(&firstPass, residuals);
}

void inverseDDS2D_NEON(const int16_t* coeffs, int16_t* residuals)
{
    const int16x8_t coeffsV[2] = { vld1q_s16(&coeffs[0]),
                                   vld1q_s16(&coeffs[8]) };
    inverseDDS2DImpl_NEON(coeffsV, residuals);
}

void dequantInverseDDS2D_NEON(const Dequant_t* dequant, TemporalSignal_t temporalSignal,
                              const int16_t* coeffs, int16_t* residuals)
{
    int16x8_t dqCoeffs[2];
    dequantDDS_NEON(dequant, temporalSignal, coeffs, dqCoeffs);
    inverseDDS2DImpl_NEON(dqCoeffs, residuals);
}

/*------------------------------------------------------------------------------*/

#endif

/* clang-format on */

/*------------------------------------------------------------------------------*/

static const TransformFunction_t kTable[2][2] = {{&inverseDD2D, &inverseDD1D},
                                                 {&inverseDDS2D, &inverseDDS1D}};

#if VN_CORE_FEATURE(SSE)

static const TransformFunction_t kTableSIMD[2][2] = {{&inverseDD2D_SSE, &inverseDD1D_SSE},
                                                     {&inverseDDS2D_SSE, &inverseDDS1D_SSE}};

#elif VN_CORE_FEATURE(NEON)

static const TransformFunction_t kTableSIMD[2][2] = {{&inverseDD2D_NEON, &inverseDD1D_NEON},
                                                     {&inverseDDS2D_NEON, &inverseDDS1D_NEON}};

#else

static const TransformFunction_t kTableSIMD[2][2] = {{NULL, NULL}, {NULL, NULL}};

#endif

TransformFunction_t transformGetFunction(TransformType_t transform, ScalingMode_t scaling,
                                         CPUAccelerationFeatures_t preferredAccel)
{
    TransformFunction_t res = NULL;

    const int32_t scalingIndex = (scaling == Scale1D) ? 1 : 0;

    if (accelerationFeatureEnabled(preferredAccel, CAFSSE) ||
        accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        res = kTableSIMD[transform][scalingIndex];
    }

    if (!res) {
        res = kTable[transform][scalingIndex];
    }

    return res;
}

/*------------------------------------------------------------------------------*/

static const DequantTransformFunction_t kDequantTable[2][2] = {
    {&dequantInverseDD2D, &dequantInverseDD1D},
    {&dequantInverseDDS2D, &dequantInverseDDS1D}};

#if VN_CORE_FEATURE(SSE)

static const DequantTransformFunction_t kDequantTableSIMD[2][2] = {
    {&dequantInverseDD2D_SSE, &dequantInverseDD1D_SSE},
    {&dequantInverseDDS2D_SSE, &dequantInverseDDS1D_SSE}};

#elif VN_CORE_FEATURE(NEON)

static const DequantTransformFunction_t kDequantTableSIMD[2][2] = {
    {&dequantInverseDD2D_NEON, &dequantInverseDD1D_NEON},
    {&dequantInverseDDS2D_NEON, &dequantInverseDDS1D_NEON}};

#else

static const DequantTransformFunction_t kDequantTableSIMD[2][2] = {{NULL, NULL}, {NULL, NULL}};

#endif

DequantTransformFunction_t dequantTransformGetFunction(TransformType_t transform, ScalingMode_t scaling,
                                                       CPUAccelerationFeatures_t preferredAccel)
{
    const int32_t scalingIndex = (scaling == Scale1D) ? 1 : 0;
    DequantTransformFunction_t res = NULL;

    if (accelerationFeatureEnabled(preferredAccel, CAFSSE) ||
        accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        res = kDequantTableSIMD[transform][scalingIndex];
    }

    if (!res) {
        res = kDequantTable[transform][scalingIndex];
    }

    return res;
}

/*------------------------------------------------------------------------------*/