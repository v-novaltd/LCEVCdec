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

#ifndef VN_LCEVC_ENHANCEMENT_DECODE_UTILITIES_H
#define VN_LCEVC_ENHANCEMENT_DECODE_UTILITIES_H

/* Helpers for the enhancement decode loop (decode.c): coefficient dequantization and
 * next-layer-event tracking, with NEON (AArch64 and 32-bit), SSE, and scalar variants. Every
 * SIMD variant is bit-exact with the scalar one. */

#include "dequant.h"

#include <LCEVC/build_config.h>
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/neon.h>
#include <LCEVC/common/sse.h>
#include <LCEVC/common/stdlib.h>
#include <LCEVC/enhancement/config_types.h>

/* The horizontal reductions (vaddv/vminvq) only exist on 64-bit NEON; 32-bit NEON falls back to
 * pairwise reductions. */
#if VN_SDK_FEATURE(NEON) && (defined(__aarch64__) || defined(_M_ARM64))
#define VN_DECODE_NEON64 1
#else
#define VN_DECODE_NEON64 0
#endif

/*- User data -----------------------------------------------------------------------------------*/

static inline void applyUserData(const LdeUserDataConfig* userData, int16_t coeffs[RCLayerCountDDS])
{
    const uint16_t packedCoeff = (uint16_t)coeffs[userData->layerIndex];
    const uint16_t signedMagnitude = packedCoeff >> userData->shift;
    const int32_t sign = (signedMagnitude & 0x01) ? -1 : 1;
    coeffs[userData->layerIndex] = (int16_t)((signedMagnitude >> 1) * sign);
}

/*- Dequantization ------------------------------------------------------------------------------*/

#if VN_SDK_FEATURE(NEON)

/* Dequantize 8 coefficients. Bit-exact with the scalar path: 32-bit product, offset added or
 * subtracted by the sign of the coefficient (untouched when zero), saturated to int16. */
static inline int16x8_t dequantCoeffs8NEON(const int16x8_t coeffs, const int16x8_t stepWidth,
                                           const int16x8_t offset)
{
    const int32x4_t zero = vdupq_n_s32(0);

    const int32x4_t prodLo = vmull_s16(vget_low_s16(coeffs), vget_low_s16(stepWidth));
    const int32x4_t prodHi = vmull_s16(vget_high_s16(coeffs), vget_high_s16(stepWidth));

    const int32x4_t coeffLo = vmovl_s16(vget_low_s16(coeffs));
    const int32x4_t coeffHi = vmovl_s16(vget_high_s16(coeffs));
    const int32x4_t offLo = vmovl_s16(vget_low_s16(offset));
    const int32x4_t offHi = vmovl_s16(vget_high_s16(offset));

    /* offset * sign(coeff), with no negation (so INT16_MIN offsets are handled exactly). */
    const int32x4_t offAdjLo =
        vsubq_s32(vandq_s32(offLo, vreinterpretq_s32_u32(vcgtq_s32(coeffLo, zero))),
                  vandq_s32(offLo, vreinterpretq_s32_u32(vcltq_s32(coeffLo, zero))));
    const int32x4_t offAdjHi =
        vsubq_s32(vandq_s32(offHi, vreinterpretq_s32_u32(vcgtq_s32(coeffHi, zero))),
                  vandq_s32(offHi, vreinterpretq_s32_u32(vcltq_s32(coeffHi, zero))));

    return vcombine_s16(vqmovn_s32(vaddq_s32(prodLo, offAdjLo)), vqmovn_s32(vaddq_s32(prodHi, offAdjHi)));
}

/* Dequantize 4 coefficients (the DD layer count); same semantics as dequantCoeffs8NEON. */
static inline int16x4_t dequantCoeffs4NEON(const int16x4_t coeffs, const int16x4_t stepWidth,
                                           const int16x4_t offset)
{
    const int32x4_t zero = vdupq_n_s32(0);
    const int32x4_t prod = vmull_s16(coeffs, stepWidth);
    const int32x4_t coeff32 = vmovl_s16(coeffs);
    const int32x4_t off32 = vmovl_s16(offset);
    const int32x4_t offAdj =
        vsubq_s32(vandq_s32(off32, vreinterpretq_s32_u32(vcgtq_s32(coeff32, zero))),
                  vandq_s32(off32, vreinterpretq_s32_u32(vcltq_s32(coeff32, zero))));
    return vqmovn_s32(vaddq_s32(prod, offAdj));
}

#elif VN_SDK_FEATURE(SSE)

/* Dequantize 8 coefficients; same semantics as dequantCoeffs8NEON. */
static inline __m128i dequantCoeffs8SSE(const __m128i coeffs, const __m128i stepWidth, const __m128i offset)
{
    const __m128i zero = _mm_setzero_si128();

    /* 32-bit products, assembled from the low and high 16-bit multiply halves. */
    const __m128i prodLo16 = _mm_mullo_epi16(coeffs, stepWidth);
    const __m128i prodHi16 = _mm_mulhi_epi16(coeffs, stepWidth);
    const __m128i prod0 = _mm_unpacklo_epi16(prodLo16, prodHi16);
    const __m128i prod1 = _mm_unpackhi_epi16(prodLo16, prodHi16);

    /* offset * sign(coeff), with no negation (so INT16_MIN offsets are handled exactly). The
     * unpack-high/shift-right pairs sign-extend the masked 16-bit offsets to 32 bits. */
    const __m128i offPos = _mm_and_si128(offset, _mm_cmpgt_epi16(coeffs, zero));
    const __m128i offNeg = _mm_and_si128(offset, _mm_cmplt_epi16(coeffs, zero));
    const __m128i offPos0 = _mm_srai_epi32(_mm_unpacklo_epi16(zero, offPos), 16);
    const __m128i offPos1 = _mm_srai_epi32(_mm_unpackhi_epi16(zero, offPos), 16);
    const __m128i offNeg0 = _mm_srai_epi32(_mm_unpacklo_epi16(zero, offNeg), 16);
    const __m128i offNeg1 = _mm_srai_epi32(_mm_unpackhi_epi16(zero, offNeg), 16);

    const __m128i res0 = _mm_sub_epi32(_mm_add_epi32(prod0, offPos0), offNeg0);
    const __m128i res1 = _mm_sub_epi32(_mm_add_epi32(prod1, offPos1), offNeg1);

    /* Saturating pack == clampS32 to [INT16_MIN, INT16_MAX]. */
    return _mm_packs_epi32(res0, res1);
}

#endif

#if VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)

/* Apply the dequantization step-width and dead-zone offset to all layers' coefficients (SIMD). */
static inline void dequantCoeffsSIMD(const Dequant* dequant, const TemporalSignal temporal,
                                     const uint8_t numLayers, int16_t coeffs[RCLayerCountDDS])
{
#if VN_SDK_FEATURE(NEON)
    if (numLayers == RCLayerCountDDS) {
        vst1q_s16(coeffs, dequantCoeffs8NEON(vld1q_s16(coeffs), dequant->stepWidthVector[temporal][0],
                                             dequant->offsetVector[temporal][0]));
        vst1q_s16(coeffs + 8,
                  dequantCoeffs8NEON(vld1q_s16(coeffs + 8), dequant->stepWidthVector[temporal][1],
                                     dequant->offsetVector[temporal][1]));
    } else {
        vst1_s16(coeffs, dequantCoeffs4NEON(vld1_s16(coeffs), vld1_s16(dequant->stepWidth[temporal]),
                                            vld1_s16(dequant->offset[temporal])));
    }
#elif VN_SDK_FEATURE(SSE)
    if (numLayers == RCLayerCountDDS) {
        _mm_storeu_si128((__m128i*)coeffs, dequantCoeffs8SSE(_mm_loadu_si128((const __m128i*)coeffs),
                                                             dequant->stepWidthVector[temporal][0],
                                                             dequant->offsetVector[temporal][0]));
        _mm_storeu_si128((__m128i*)(coeffs + 8),
                         dequantCoeffs8SSE(_mm_loadu_si128((const __m128i*)(coeffs + 8)),
                                           dequant->stepWidthVector[temporal][1],
                                           dequant->offsetVector[temporal][1]));
    } else {
        /* 4 coefficients in the low half; the (zero) upper lanes dequantize to zero. */
        _mm_storel_epi64((__m128i*)coeffs,
                         dequantCoeffs8SSE(_mm_loadl_epi64((const __m128i*)coeffs),
                                           _mm_loadl_epi64((const __m128i*)dequant->stepWidth[temporal]),
                                           _mm_loadl_epi64((const __m128i*)dequant->offset[temporal])));
    }
#endif
}

#endif // VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)

/* Apply the dequantization step-width and dead-zone offset to all layers' coefficients (scalar). */
static inline void dequantCoeffsScalar(const Dequant* dequant, const TemporalSignal temporal,
                                       const uint8_t numLayers, int16_t coeffs[RCLayerCountDDS])
{
    for (uint8_t layer = 0; layer < numLayers; layer++) {
        if (coeffs[layer] > 0) {
            coeffs[layer] =
                (int16_t)clampS32((int32_t)coeffs[layer] * dequant->stepWidth[temporal][layer] +
                                      dequant->offset[temporal][layer],
                                  INT16_MIN, INT16_MAX);
        } else if (coeffs[layer] < 0) {
            coeffs[layer] =
                (int16_t)clampS32((int32_t)coeffs[layer] * dequant->stepWidth[temporal][layer] -
                                      dequant->offset[temporal][layer],
                                  INT16_MIN, INT16_MAX);
        }
    }
}

#endif // !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)

/* Apply the dequantization step-width and dead-zone offset to all layers' coefficients. Uses SIMD
 * when available at runtime, otherwise the scalar fallback. */
static inline void dequantCoeffs(const Dequant* dequant, const TemporalSignal temporal,
                                 const uint8_t numLayers, int16_t coeffs[RCLayerCountDDS])
{
#if VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)
    if (ldcAccelerationGet()->hasSIMD) {
        dequantCoeffsSIMD(dequant, temporal, numLayers, coeffs);
        return;
    }
#endif
#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    dequantCoeffsScalar(dequant, temporal, numLayers, coeffs);
#endif
}

/*- Next-layer-event tracking -------------------------------------------------------------------*/

#if VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)

/* Recompute the earliest next-event TU across all layers, and the mask of layers whose next event
 * falls on it (1 bit per layer) - SIMD. */
static inline void findNextLayerEventSIMD(const uint8_t numLayers,
                                          const int32_t nextEventTu[RCLayerCountDDS],
                                          int32_t* minNextTuOut, uint32_t* expiredMaskOut)
{
#if VN_SDK_FEATURE(NEON)
    if (numLayers == RCLayerCountDDS) {
        const int32x4_t next0 = vld1q_s32(nextEventTu);
        const int32x4_t next1 = vld1q_s32(nextEventTu + 4);
        const int32x4_t next2 = vld1q_s32(nextEventTu + 8);
        const int32x4_t next3 = vld1q_s32(nextEventTu + 12);
        const int32x4_t min4 = vminq_s32(vminq_s32(next0, next1), vminq_s32(next2, next3));

#if VN_DECODE_NEON64
        const int32_t minNext = vminvq_s32(min4);
#else
        int32x2_t min2 = vpmin_s32(vget_low_s32(min4), vget_high_s32(min4));
        min2 = vpmin_s32(min2, min2);
        const int32_t minNext = vget_lane_s32(min2, 0);
#endif

        /* Mask of the layers whose next event is at minNext. */
        const int32x4_t minSplat = vdupq_n_s32(minNext);
        const uint16x8_t atMin01 =
            vcombine_u16(vmovn_u32(vceqq_s32(next0, minSplat)), vmovn_u32(vceqq_s32(next1, minSplat)));
        const uint16x8_t atMin23 =
            vcombine_u16(vmovn_u32(vceqq_s32(next2, minSplat)), vmovn_u32(vceqq_s32(next3, minSplat)));
        const uint8x16_t atMinBytes = vcombine_u8(vmovn_u16(atMin01), vmovn_u16(atMin23));
        static const uint8_t kBitWeights[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                                1, 2, 4, 8, 16, 32, 64, 128};
        const uint8x16_t weightedBits = vandq_u8(atMinBytes, vld1q_u8(kBitWeights));

#if VN_DECODE_NEON64
        const uint32_t expiredMask = (uint32_t)vaddv_u8(vget_low_u8(weightedBits)) |
                                     ((uint32_t)vaddv_u8(vget_high_u8(weightedBits)) << 8);
#else
        /* Pairwise-sum the weighted bytes down to one byte per 8 layers. */
        uint8x8_t sums = vpadd_u8(vget_low_u8(weightedBits), vget_high_u8(weightedBits));
        sums = vpadd_u8(sums, sums);
        sums = vpadd_u8(sums, sums);
        const uint32_t expiredMask = vget_lane_u16(vreinterpret_u16_u8(sums), 0);
#endif

        *minNextTuOut = minNext;
        *expiredMaskOut = expiredMask;
        return;
    }

    /* DD (4 layers). */
    const int32x4_t next = vld1q_s32(nextEventTu);
#if VN_DECODE_NEON64
    const int32_t minNext = vminvq_s32(next);
#else
    int32x2_t min2 = vpmin_s32(vget_low_s32(next), vget_high_s32(next));
    min2 = vpmin_s32(min2, min2);
    const int32_t minNext = vget_lane_s32(min2, 0);
#endif

    static const uint32_t kBitWeights4[4] = {1, 2, 4, 8};
    const uint32x4_t weightedBits =
        vandq_u32(vceqq_s32(next, vdupq_n_s32(minNext)), vld1q_u32(kBitWeights4));
#if VN_DECODE_NEON64
    const uint32_t expiredMask = vaddvq_u32(weightedBits);
#else
    uint32x2_t sums = vadd_u32(vget_low_u32(weightedBits), vget_high_u32(weightedBits));
    sums = vpadd_u32(sums, sums);
    const uint32_t expiredMask = vget_lane_u32(sums, 0);
#endif

    *minNextTuOut = minNext;
    *expiredMaskOut = expiredMask;
#elif VN_SDK_FEATURE(SSE)
    if (numLayers == RCLayerCountDDS) {
        const __m128i next0 = _mm_loadu_si128((const __m128i*)nextEventTu);
        const __m128i next1 = _mm_loadu_si128((const __m128i*)(nextEventTu + 4));
        const __m128i next2 = _mm_loadu_si128((const __m128i*)(nextEventTu + 8));
        const __m128i next3 = _mm_loadu_si128((const __m128i*)(nextEventTu + 12));

        /* Fold the minimum into every lane. */
        __m128i min4 = _mm_min_epi32(_mm_min_epi32(next0, next1), _mm_min_epi32(next2, next3));
        min4 = _mm_min_epi32(min4, _mm_shuffle_epi32(min4, _MM_SHUFFLE(1, 0, 3, 2)));
        min4 = _mm_min_epi32(min4, _mm_shuffle_epi32(min4, _MM_SHUFFLE(2, 3, 0, 1)));

        /* Mask of the layers whose next event is at the minimum: pack the 32-bit compare masks
         * down to bytes (saturation preserves 0/-1), then movemask gives 1 bit per layer. */
        const __m128i atMin01 =
            _mm_packs_epi32(_mm_cmpeq_epi32(next0, min4), _mm_cmpeq_epi32(next1, min4));
        const __m128i atMin23 =
            _mm_packs_epi32(_mm_cmpeq_epi32(next2, min4), _mm_cmpeq_epi32(next3, min4));

        *minNextTuOut = _mm_cvtsi128_si32(min4);
        *expiredMaskOut = (uint32_t)_mm_movemask_epi8(_mm_packs_epi16(atMin01, atMin23));
        return;
    }

    /* DD (4 layers). */
    const __m128i next = _mm_loadu_si128((const __m128i*)nextEventTu);
    __m128i min4 = _mm_min_epi32(next, _mm_shuffle_epi32(next, _MM_SHUFFLE(1, 0, 3, 2)));
    min4 = _mm_min_epi32(min4, _mm_shuffle_epi32(min4, _MM_SHUFFLE(2, 3, 0, 1)));

    /* movemask over the 32-bit compare mask gives 1 bit per layer. */
    *minNextTuOut = _mm_cvtsi128_si32(min4);
    *expiredMaskOut = (uint32_t)_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(next, min4)));
#endif
}

#endif // VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)

/* Recompute the earliest next-event TU across all layers, and the mask of layers whose next event
 * falls on it (1 bit per layer) - scalar. */
static inline void findNextLayerEventScalar(const uint8_t numLayers,
                                            const int32_t nextEventTu[RCLayerCountDDS],
                                            int32_t* minNextTuOut, uint32_t* expiredMaskOut)
{
    int32_t minNext = nextEventTu[0];
    uint32_t expiredMask = 1;
    for (uint8_t layer = 1; layer < numLayers; layer++) {
        const int32_t next = nextEventTu[layer];
        if (next < minNext) {
            minNext = next;
            expiredMask = 1u << layer;
        } else if (next == minNext) {
            expiredMask |= 1u << layer;
        }
    }
    *minNextTuOut = minNext;
    *expiredMaskOut = expiredMask;
}

#endif // !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)

/* Recompute the earliest next-event TU across all layers, and the mask of layers whose next
 * event falls on it (1 bit per layer). Only needed after a layer event (nextEventTu is
 * unchanged otherwise). Uses SIMD when available at runtime, otherwise the scalar fallback. */
static inline void findNextLayerEvent(const uint8_t numLayers, const int32_t nextEventTu[RCLayerCountDDS],
                                      int32_t* minNextTuOut, uint32_t* expiredMaskOut)
{
#if VN_SDK_FEATURE(NEON) || VN_SDK_FEATURE(SSE)
    if (ldcAccelerationGet()->hasSIMD) {
        findNextLayerEventSIMD(numLayers, nextEventTu, minNextTuOut, expiredMaskOut);
        return;
    }
#endif
#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    findNextLayerEventScalar(numLayers, nextEventTu, minNextTuOut, expiredMaskOut);
#endif
}

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_DECODE_UTILITIES_H
