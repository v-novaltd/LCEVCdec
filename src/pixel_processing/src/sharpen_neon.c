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

#include "sharpen_common.h"

#include <LCEVC/build_config.h>
#include <stddef.h>

#if VN_SDK_FEATURE(NEON)

#include <LCEVC/common/limit.h>
#include <LCEVC/common/neon.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include "fp_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*------------------------------------------------------------------------------*/

static const uint32_t kStepU8 = 16;
static const uint32_t kStepU16 = 8;

static inline int32x4_t fromU16S32(int32x4_t val)
{
    return vshrq_n_s32(vaddq_s32(val, vdupq_n_s32(1 << 15)), 16);
}

static inline int32x4_t u16x4ToS32(uint16x4_t val) { return vreinterpretq_s32_u32(vmovl_u16(val)); }

static inline int16x8_t neonGenerateDither(const uint16_t** ditherBuffer,
                                           const int16x8_t ditherOffset, const uint16_t ditherMul)
{
    uint16x8_t buffer = vld1q_u16(*ditherBuffer);
    *ditherBuffer += 8;

    const uint32x4_t lo = vmull_n_u16(vget_low_u16(buffer), ditherMul);

#if defined(__aarch64__)
    const uint32x4_t hi = vmull_high_n_u16(buffer, ditherMul);
    buffer = vuzp2q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
#else
    const uint32x4_t hi = vmull_n_u16(vget_high_u16(buffer), ditherMul);
    buffer = vuzpq_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi)).val[1];
#endif

    return vqsubq_s16(ditherOffset, vreinterpretq_s16_u16(buffer));
}

static inline uint8x16_t neonApplyDitherU8(const uint8x16_t values, const uint16_t** ditherBuffer,
                                           const int16x8_t ditherOffset, const uint16_t ditherMul)
{
    const int16x8_t ditherLo = neonGenerateDither(ditherBuffer, ditherOffset, ditherMul);
    const int16x8_t ditherHi = neonGenerateDither(ditherBuffer, ditherOffset, ditherMul);

    int16x8_t valueLo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(values)));
    int16x8_t valueHi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(values)));

    valueLo = vqaddq_s16(valueLo, ditherLo);
    valueHi = vqaddq_s16(valueHi, ditherHi);

    return vcombine_u8(vqmovun_s16(valueLo), vqmovun_s16(valueHi));
}

static inline uint16x8_t neonApplyDitherU16(const uint16x8_t values, const uint16_t** ditherBuffer,
                                            const int16x8_t ditherOffset, const uint16_t ditherMul,
                                            const int32x4_t clampS32)
{
    const int16x8_t dither = neonGenerateDither(ditherBuffer, ditherOffset, ditherMul);

    int32x4_t valueLo = u16x4ToS32(vget_low_u16(values));
    int32x4_t valueHi = u16x4ToS32(vget_high_u16(values));
    const int32x4_t ditherLo = vmovl_s16(vget_low_s16(dither));
    const int32x4_t ditherHi = vmovl_s16(vget_high_s16(dither));

    valueLo = vaddq_s32(valueLo, ditherLo);
    valueHi = vaddq_s32(valueHi, ditherHi);

    valueLo = vmaxq_s32(vminq_s32(valueLo, clampS32), vdupq_n_s32(0));
    valueHi = vmaxq_s32(vminq_s32(valueHi, clampS32), vdupq_n_s32(0));

    return vcombine_u16(vqmovun_s32(valueLo), vqmovun_s32(valueHi));
}

static inline int16x8_t sharpenCoeffU8x8(const uint8x8_t center, const uint8x8_t left,
                                         const uint8x8_t right, const uint8x8_t top, const uint8x8_t bottom,
                                         const int16_t strength, int16x8_t* weightOut)
{
    const uint16x8_t center16 = vmovl_u8(center);
    const uint16x8_t left16 = vmovl_u8(left);
    const uint16x8_t right16 = vmovl_u8(right);
    const uint16x8_t top16 = vmovl_u8(top);
    const uint16x8_t bottom16 = vmovl_u8(bottom);

    const uint16x8_t neighbours = vaddq_u16(vaddq_u16(left16, right16), vaddq_u16(top16, bottom16));
    const int16x8_t weight16 = vreinterpretq_s16_u16(vsubq_u16(vshlq_n_u16(center16, 2), neighbours));
    if (weightOut) {
        *weightOut = weight16;
    }

    const int32x4_t prod0 = vmull_n_s16(vget_low_s16(weight16), strength);
    const int32x4_t prod1 = vmull_n_s16(vget_high_s16(weight16), strength);
    const int16x4_t coeff0 = vrshrn_n_s32(prod0, 16);
    const int16x4_t coeff1 = vrshrn_n_s32(prod1, 16);
    return vcombine_s16(coeff0, coeff1);
}

static inline uint8x8_t sharpenKernelU8x8(const uint8x8_t center, const uint8x8_t left,
                                          const uint8x8_t right, const uint8x8_t top,
                                          const uint8x8_t bottom, const int16_t strength)
{
    const int16x8_t coeff16 = sharpenCoeffU8x8(center, left, right, top, bottom, strength, NULL);
    const int16x8_t centerS16 = vreinterpretq_s16_u16(vmovl_u8(center));
    const int16x8_t result16 = vaddq_s16(centerS16, coeff16);

    return vqmovun_s16(result16);
}

static inline uint8x8_t sharpenKernelU8x8AddWeight(const uint8x8_t center, const uint8x8_t left,
                                                   const uint8x8_t right, const uint8x8_t top,
                                                   const uint8x8_t bottom, const int16_t strength)
{
    int16x8_t weight16;
    const int16x8_t coeff16 = sharpenCoeffU8x8(center, left, right, top, bottom, strength, &weight16);
    const int16x8_t centerS16 = vreinterpretq_s16_u16(vmovl_u8(center));
    const int16x8_t result16 = vaddq_s16(vaddq_s16(centerS16, coeff16), weight16);

    return vqmovun_s16(result16);
}

static inline int32x4x2_t sharpenWeightU16x8(const uint16x8_t center, const uint16x8_t left,
                                             const uint16x8_t right, const uint16x8_t top,
                                             const uint16x8_t bottom)
{
    int32x4x2_t weight;

    const int32x4_t centerLo = u16x4ToS32(vget_low_u16(center));
    const int32x4_t centerHi = u16x4ToS32(vget_high_u16(center));
    const int32x4_t leftLo = u16x4ToS32(vget_low_u16(left));
    const int32x4_t leftHi = u16x4ToS32(vget_high_u16(left));
    const int32x4_t rightLo = u16x4ToS32(vget_low_u16(right));
    const int32x4_t rightHi = u16x4ToS32(vget_high_u16(right));
    const int32x4_t topLo = u16x4ToS32(vget_low_u16(top));
    const int32x4_t topHi = u16x4ToS32(vget_high_u16(top));
    const int32x4_t bottomLo = u16x4ToS32(vget_low_u16(bottom));
    const int32x4_t bottomHi = u16x4ToS32(vget_high_u16(bottom));

    const int32x4_t neighboursLo = vaddq_s32(vaddq_s32(leftLo, rightLo), vaddq_s32(topLo, bottomLo));
    const int32x4_t neighboursHi = vaddq_s32(vaddq_s32(leftHi, rightHi), vaddq_s32(topHi, bottomHi));

    weight.val[0] = vsubq_s32(vshlq_n_s32(centerLo, 2), neighboursLo);
    weight.val[1] = vsubq_s32(vshlq_n_s32(centerHi, 2), neighboursHi);
    return weight;
}

static inline uint16x8_t sharpenKernelU16x8(const uint16x8_t center, const uint16x8_t left,
                                            const uint16x8_t right, const uint16x8_t top,
                                            const uint16x8_t bottom, const int16_t strength,
                                            const int32x4_t clampS32)
{
    const int32x4_t centerLo = u16x4ToS32(vget_low_u16(center));
    const int32x4_t centerHi = u16x4ToS32(vget_high_u16(center));
    const int32x4x2_t weight = sharpenWeightU16x8(center, left, right, top, bottom);
    int32x4_t resultLo = vaddq_s32(centerLo, fromU16S32(vmulq_n_s32(weight.val[0], strength)));
    int32x4_t resultHi = vaddq_s32(centerHi, fromU16S32(vmulq_n_s32(weight.val[1], strength)));

    resultLo = vmaxq_s32(vminq_s32(resultLo, clampS32), vdupq_n_s32(0));
    resultHi = vmaxq_s32(vminq_s32(resultHi, clampS32), vdupq_n_s32(0));

    return vcombine_u16(vqmovun_s32(resultLo), vqmovun_s32(resultHi));
}

static inline uint16x8_t sharpenKernelU16x8AddWeight(const uint16x8_t center, const uint16x8_t left,
                                                     const uint16x8_t right, const uint16x8_t top,
                                                     const uint16x8_t bottom, const int16_t strength,
                                                     const int32x4_t clampS32)
{
    const int32x4_t centerLo = u16x4ToS32(vget_low_u16(center));
    const int32x4_t centerHi = u16x4ToS32(vget_high_u16(center));
    const int32x4x2_t weight = sharpenWeightU16x8(center, left, right, top, bottom);
    int32x4_t resultLo =
        vaddq_s32(vaddq_s32(centerLo, fromU16S32(vmulq_n_s32(weight.val[0], strength))), weight.val[0]);
    int32x4_t resultHi =
        vaddq_s32(vaddq_s32(centerHi, fromU16S32(vmulq_n_s32(weight.val[1], strength))), weight.val[1]);

    resultLo = vmaxq_s32(vminq_s32(resultLo, clampS32), vdupq_n_s32(0));
    resultHi = vmaxq_s32(vminq_s32(resultHi, clampS32), vdupq_n_s32(0));

    return vcombine_u16(vqmovun_s32(resultLo), vqmovun_s32(resultHi));
}

static inline void sharpenU8_NEON(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                  float strengthFloat, LdppDitherSlice* dither)
{
    const uint16_t strengthInt = f32ToU16(strengthFloat);
    const int16_t strength = (int16_t)strengthInt;
    const bool addWeight = (strengthInt & 0x8000) != 0;
    const bool ditherEnabled = (dither != NULL);

    const uint32_t tmpSize = width - 2;
    uint8_t* tmpRow[2] = {alloca(tmpSize), alloca(tmpSize)};
    memcpy(tmpRow[0], getRowU8(src, stride, 0) + 1, tmpSize);
    const uint32_t simdWidth = tmpSize & ~(kStepU8 - 1);
    const uint32_t simdEnd = 1 + simdWidth;
    const uint32_t ditherSpan = simdWidth + kStepU8;
    const int16x8_t ditherOffset = vdupq_n_s16((int16_t)(ditherEnabled ? dither->strength : 0));
    const uint16_t ditherMul = (uint16_t)(ditherEnabled ? (dither->strength * 2u + 1u) : 0u);

    for (uint32_t y = 1; y < height - 1; ++y) {
        uint8_t* srcRows[3] = {
            getRowU8(src, stride, y - 1),
            getRowU8(src, stride, y),
            getRowU8(src, stride, y + 1),
        };
        const uint16_t* ditherBuffer = NULL;
        if (ditherEnabled) {
            ditherBuffer = ldppDitherGetBuffer(dither, ditherSpan);
        }

        uint32_t x = 1;
        if (addWeight) {
            for (; x < simdEnd; x += kStepU8) {
                const uint8x16_t top = vld1q_u8(srcRows[0] + x);
                const uint8x16_t left = vld1q_u8(srcRows[1] + x - 1);
                const uint8x16_t center = vld1q_u8(srcRows[1] + x);
                const uint8x16_t right = vld1q_u8(srcRows[1] + x + 1);
                const uint8x16_t bottom = vld1q_u8(srcRows[2] + x);

                uint8x8_t dstLo = sharpenKernelU8x8AddWeight(vget_low_u8(center), vget_low_u8(left),
                                                             vget_low_u8(right), vget_low_u8(top),
                                                             vget_low_u8(bottom), strength);
                uint8x8_t dstHi = sharpenKernelU8x8AddWeight(vget_high_u8(center), vget_high_u8(left),
                                                             vget_high_u8(right), vget_high_u8(top),
                                                             vget_high_u8(bottom), strength);

                uint8x16_t dst = vcombine_u8(dstLo, dstHi);
                if (ditherBuffer) {
                    dst = neonApplyDitherU8(dst, &ditherBuffer, ditherOffset, ditherMul);
                }
                vst1q_u8(tmpRow[1] + x - 1, dst);
            }
        } else {
            for (; x < simdEnd; x += kStepU8) {
                const uint8x16_t top = vld1q_u8(srcRows[0] + x);
                const uint8x16_t left = vld1q_u8(srcRows[1] + x - 1);
                const uint8x16_t center = vld1q_u8(srcRows[1] + x);
                const uint8x16_t right = vld1q_u8(srcRows[1] + x + 1);
                const uint8x16_t bottom = vld1q_u8(srcRows[2] + x);

                uint8x8_t dstLo =
                    sharpenKernelU8x8(vget_low_u8(center), vget_low_u8(left), vget_low_u8(right),
                                      vget_low_u8(top), vget_low_u8(bottom), strength);
                uint8x8_t dstHi =
                    sharpenKernelU8x8(vget_high_u8(center), vget_high_u8(left), vget_high_u8(right),
                                      vget_high_u8(top), vget_high_u8(bottom), strength);

                uint8x16_t dst = vcombine_u8(dstLo, dstHi);
                if (ditherBuffer) {
                    dst = neonApplyDitherU8(dst, &ditherBuffer, ditherOffset, ditherMul);
                }
                vst1q_u8(tmpRow[1] + x - 1, dst);
            }
        }

        // Final right edge
        x = width - kStepU8 - 1;
        const uint8x16_t top = vld1q_u8(srcRows[0] + x);
        const uint8x16_t left = vld1q_u8(srcRows[1] + x - 1);
        const uint8x16_t center = vld1q_u8(srcRows[1] + x);
        const uint8x16_t right = vld1q_u8(srcRows[1] + x + 1);
        const uint8x16_t bottom = vld1q_u8(srcRows[2] + x);

        uint8x8_t dstLo;
        uint8x8_t dstHi;
        if (addWeight) {
            dstLo = sharpenKernelU8x8AddWeight(vget_low_u8(center), vget_low_u8(left), vget_low_u8(right),
                                               vget_low_u8(top), vget_low_u8(bottom), strength);
            dstHi = sharpenKernelU8x8AddWeight(vget_high_u8(center), vget_high_u8(left),
                                               vget_high_u8(right), vget_high_u8(top),
                                               vget_high_u8(bottom), strength);
        } else {
            dstLo = sharpenKernelU8x8(vget_low_u8(center), vget_low_u8(left), vget_low_u8(right),
                                      vget_low_u8(top), vget_low_u8(bottom), strength);
            dstHi = sharpenKernelU8x8(vget_high_u8(center), vget_high_u8(left), vget_high_u8(right),
                                      vget_high_u8(top), vget_high_u8(bottom), strength);
        }

        uint8x16_t dst = vcombine_u8(dstLo, dstHi);
        if (ditherBuffer) {
            dst = neonApplyDitherU8(dst, &ditherBuffer, ditherOffset, ditherMul);
        }
        vst1q_u8(tmpRow[1] + x - 1, dst);

        memcpy(getRowU8(src, stride, y - 1) + 1, tmpRow[0], tmpSize);
        uint8_t* const swap = tmpRow[0];
        tmpRow[0] = tmpRow[1];
        tmpRow[1] = swap;
    }

    memcpy(getRowU8(src, stride, height - 2) + 1, tmpRow[0], tmpSize);
}

static inline void sharpenU16_NEON(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                   float strengthFloat, int32_t resultClamp, LdppDitherSlice* dither)
{
    const uint16_t strengthInt = f32ToU16(strengthFloat);
    const int16_t strength = (int16_t)strengthInt;
    const bool addWeight = (strengthInt & 0x8000) != 0;
    const bool ditherEnabled = (dither != NULL);
    const int32x4_t clampS32 = vdupq_n_s32(resultClamp);

    const uint32_t tmpSize = (width - 2) * sizeof(uint16_t);
    uint16_t* tmpRow[2] = {(uint16_t*)alloca(tmpSize), (uint16_t*)alloca(tmpSize)};

    memcpy(tmpRow[0], ((uint16_t*)getRowU8(src, stride, 0)) + 1, tmpSize);
    const uint32_t simdWidth = (width - 2) & ~(kStepU16 - 1);
    const uint32_t simdEnd = 1 + simdWidth;
    const uint32_t ditherSpan = simdWidth + kStepU16;
    const int16x8_t ditherOffset = vdupq_n_s16((int16_t)(ditherEnabled ? dither->strength : 0));
    const uint16_t ditherMul = (uint16_t)(ditherEnabled ? (dither->strength * 2u + 1u) : 0u);

    for (uint32_t y = 1; y < height - 1; ++y) {
        uint16_t* srcRows[3] = {
            (uint16_t*)getRowU8(src, stride, y - 1),
            (uint16_t*)getRowU8(src, stride, y),
            (uint16_t*)getRowU8(src, stride, y + 1),
        };
        const uint16_t* ditherBuffer = NULL;
        if (ditherEnabled) {
            ditherBuffer = ldppDitherGetBuffer(dither, ditherSpan);
        }

        uint32_t x = 1;
        if (addWeight) {
            for (; x < simdEnd; x += kStepU16) {
                const uint16x8_t top = vld1q_u16(srcRows[0] + x);
                const uint16x8_t left = vld1q_u16(srcRows[1] + x - 1);
                const uint16x8_t center = vld1q_u16(srcRows[1] + x);
                const uint16x8_t right = vld1q_u16(srcRows[1] + x + 1);
                const uint16x8_t bottom = vld1q_u16(srcRows[2] + x);

                uint16x8_t dst =
                    sharpenKernelU16x8AddWeight(center, left, right, top, bottom, strength, clampS32);
                if (ditherBuffer) {
                    dst = neonApplyDitherU16(dst, &ditherBuffer, ditherOffset, ditherMul, clampS32);
                }
                vst1q_u16(tmpRow[1] + x - 1, dst);
            }
        } else {
            for (; x < simdEnd; x += kStepU16) {
                const uint16x8_t top = vld1q_u16(srcRows[0] + x);
                const uint16x8_t left = vld1q_u16(srcRows[1] + x - 1);
                const uint16x8_t center = vld1q_u16(srcRows[1] + x);
                const uint16x8_t right = vld1q_u16(srcRows[1] + x + 1);
                const uint16x8_t bottom = vld1q_u16(srcRows[2] + x);

                uint16x8_t dst = sharpenKernelU16x8(center, left, right, top, bottom, strength, clampS32);
                if (ditherBuffer) {
                    dst = neonApplyDitherU16(dst, &ditherBuffer, ditherOffset, ditherMul, clampS32);
                }
                vst1q_u16(tmpRow[1] + x - 1, dst);
            }
        }

        // Final right edge
        x = width - kStepU16 - 1;
        const uint16x8_t top = vld1q_u16(srcRows[0] + x);
        const uint16x8_t left = vld1q_u16(srcRows[1] + x - 1);
        const uint16x8_t center = vld1q_u16(srcRows[1] + x);
        const uint16x8_t right = vld1q_u16(srcRows[1] + x + 1);
        const uint16x8_t bottom = vld1q_u16(srcRows[2] + x);

        if (addWeight) {
            uint16x8_t dst =
                sharpenKernelU16x8AddWeight(center, left, right, top, bottom, strength, clampS32);
            if (ditherBuffer) {
                dst = neonApplyDitherU16(dst, &ditherBuffer, ditherOffset, ditherMul, clampS32);
            }
            vst1q_u16(tmpRow[1] + x - 1, dst);
        } else {
            uint16x8_t dst = sharpenKernelU16x8(center, left, right, top, bottom, strength, clampS32);
            if (ditherBuffer) {
                dst = neonApplyDitherU16(dst, &ditherBuffer, ditherOffset, ditherMul, clampS32);
            }
            vst1q_u16(tmpRow[1] + x - 1, dst);
        }

        memcpy(((uint16_t*)getRowU8(src, stride, y - 1)) + 1, tmpRow[0], tmpSize);
        uint16_t* const swap = tmpRow[0];
        tmpRow[0] = tmpRow[1];
        tmpRow[1] = swap;
    }

    memcpy(((uint16_t*)getRowU8(src, stride, height - 2)) + 1, tmpRow[0], tmpSize);
}

static inline void sharpenU10_NEON(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                   float strength, LdppDitherSlice* dither)
{
    sharpenU16_NEON(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU10), dither);
}

static inline void sharpenU12_NEON(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                   float strength, LdppDitherSlice* dither)
{
    sharpenU16_NEON(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU12), dither);
}

static inline void sharpenU14_NEON(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                   float strength, LdppDitherSlice* dither)
{
    sharpenU16_NEON(src, stride, width, height, strength, fixedPointHighlightValue(LdpFPU14), dither);
}

/*------------------------------------------------------------------------------*/

SharpenFunction sharpenGetFunctionNEON(LdpFixedPoint fixedPoint)
{
    static const SharpenFunction kTable[LdpFPCount] = {
        sharpenU8_NEON,  /* U8 */
        sharpenU10_NEON, /* U10 */
        sharpenU12_NEON, /* U12 */
        sharpenU14_NEON, /* U14 */
        NULL,            /* S8.7 */
        NULL,            /* S10.5 */
        NULL,            /* S12.3 */
        NULL,            /* S14.1 */
    };
    return kTable[fixedPoint];
}

/*------------------------------------------------------------------------------*/

#else

SharpenFunction sharpenGetFunctionNEON(LdpFixedPoint fixedPoint)
{
    VNUnused(fixedPoint);
    return NULL;
}

#endif
