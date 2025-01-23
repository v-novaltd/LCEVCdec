/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_DEC_CORE_NEON_H_
#define VN_DEC_CORE_NEON_H_

#if VN_CORE_FEATURE(NEON)

#include "common/types.h"

#include <assert.h>

#ifdef _WIN32
#include <arm64_neon.h>
#else /* !_WIN32 */
#include <arm_neon.h>
#endif /* _WIN32 */

/*------------------------------------------------------------------------------*/

/*! \brief Safely read up to 16 s8 lanes from the src location */
static inline int8x16_t loadVectorS8NEON(const void* src, uint32_t maxNumLanes)
{
    if (maxNumLanes >= 16) {
        return vld1q_s8((const int8_t*)src);
    }

    if (maxNumLanes == 8) {
        return vcombine_s8(vld1_s8((const int8_t*)src), vdup_n_s8(0));
    }

    VN_ALIGN(int8_t temp[16], 16) = {0};
    memcpy(temp, src, maxNumLanes);
    return vld1q_s8(temp);
}

/*! \brief Safely read up to 16 u8 lanes from the src location */
static inline uint8x16_t loadVectorU8NEON(const void* src, uint32_t maxNumLanes)
{
    if (maxNumLanes >= 16) {
        return vld1q_u8((const uint8_t*)src);
    }

    if (maxNumLanes == 8) {
        return vcombine_u8(vld1_u8((const uint8_t*)src), vdup_n_u8(0));
    }

    VN_ALIGN(uint8_t temp[16], 16) = {0};
    memcpy(temp, src, maxNumLanes);
    return vld1q_u8(temp);
}

/*! \brief Safely read up to 8 S16 lanes from the src location
 *
 *  \note This is safe to read up to 15-bits per lane as unsigned
 *        data - the cast will be implicit. */
static inline int16x8_t loadVectorS16NEON(const void* src, uint32_t maxNumLanes)
{
    if (maxNumLanes >= 8) {
        return vld1q_s16((const int16_t*)src);
    }

    if (maxNumLanes == 4) {
        return vcombine_s16(vld1_s16((const int16_t*)src), vdup_n_s16(0));
    }

    VN_ALIGN(int16_t temp[8], 16) = {0};
    memcpy(temp, src, maxNumLanes * sizeof(int16_t));
    return vld1q_s16(temp);
}

// All of the below will write the entire src vector, UNLESS maxNumLanes is greater than the number
// of lanes in src.

/*! \brief Safely write up to 16 lanes of 8-bit data to the dst location. */
static inline void writeVectorU8x16NEON(void* dst, uint8x16_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 16) {
        vst1q_u8((uint8_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((uint8_t*)dst)[i] = vgetq_lane_u8(src, 0);
        src = vextq_u8(src, src, 1);
    }
}

/*! \brief Safely write up to 8 lanes of 8-bit data to the dst location. */
static inline void writeVectorU8x8NEON(void* dst, uint8x8_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 8) {
        vst1_u8((uint8_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((uint8_t*)dst)[i] = vget_lane_u8(src, 0);
        src = vext_u8(src, src, 1);
    }
}

/*! \brief Safely write and interleave up to 8 lanes of 8-bit data to the dst location. */
static inline void writeVectorU8x8x2NEON(void* dst, uint8x8x2_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 8) {
        vst2_u8((uint8_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((uint8_t*)dst)[2 * i] = vget_lane_u8(src.val[0], 0);
        src.val[0] = vext_u8(src.val[0], src.val[0], 1);
        ((uint8_t*)dst)[2 * i + 1] = vget_lane_u8(src.val[1], 0);
        src.val[1] = vext_u8(src.val[1], src.val[1], 1);
    }
}

/*! \brief Safely write up to 8 lanes of 16-bit data to the dst location. */
static inline void writeVectorS16NEON(void* dst, int16x8_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 8) {
        vst1q_s16((int16_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((int16_t*)dst)[i] = vgetq_lane_s16(src, 0);
        src = vextq_s16(src, src, 1);
    }
}

/*! \brief Safely write up to 4 lanes of 16-bit data to the dst location. */
static inline void writeVectorU16x4NEON(void* dst, uint16x4_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 4) {
        vst1_u16((uint16_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((uint16_t*)dst)[i] = vget_lane_u16(src, 0);
        src = vext_u16(src, src, 1);
    }
}

/*! \brief Safely write and interleave up to 4 lanes of 16-bit NV12 data to the dst location. */
static inline void writeVectorU16x4x2NEON(void* dst, uint16x4x2_t src, const uint32_t maxNumLanes)
{
    if (maxNumLanes >= 4) {
        vst2_u16((uint16_t*)dst, src);
        return;
    }

    for (uint32_t i = 0; i < maxNumLanes; i++) {
        ((uint16_t*)dst)[2 * i] = vget_lane_u16(src.val[0], 0);
        src.val[0] = vext_u16(src.val[0], src.val[0], 1);
        ((uint16_t*)dst)[2 * i + 1] = vget_lane_u16(src.val[1], 0);
        src.val[1] = vext_u16(src.val[1], src.val[1], 1);
    }
}

/*------------------------------------------------------------------------------*/

/*! \brief Casts 16 U8 values to 8 S16s. */
static inline int16x8x2_t expandU8ToS16NEON(const uint8x16_t vec)
{
    const int16x8x2_t temp = {{
        vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(vec))),
        vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(vec))),
    }};

    return temp;
}

/*! \brief Casts 16 S8 values to 8 S16s. */
static inline int16x8x2_t expandS8ToS16NEON(const int8x16_t vec)
{
    const int16x8x2_t temp = {{
        vmovl_s8(vget_low_s8(vec)),
        vmovl_s8(vget_high_s8(vec)),
    }};

    return temp;
}

/*! \brief Casts 8 S16 values to 4 S32s. */
static inline int32x4x2_t expandS16ToS32NEON(const int16x8_t vec)
{
    const int32x4x2_t temp = {{
        vmovl_s16(vget_low_s16(vec)),
        vmovl_s16(vget_high_s16(vec)),
    }};

    return temp;
}

/*! \brief Casts 16 S16 values to U8 with saturation.  */
static inline uint8x16_t packS16ToU8NEON(const int16x8x2_t vec)
{
    return vcombine_u8(vqmovun_s16(vec.val[0]), vqmovun_s16(vec.val[1]));
}

static inline uint16x8_t clampS16ToU16(const int16x8_t value, const int16x8_t minValue, const int16x8_t maxValue)
{
    return vreinterpretq_u16_s16(vmaxq_s16(vminq_s16(value, maxValue), minValue));
}

/*------------------------------------------------------------------------------*/

static inline int16x8x2_t loadUNAsS16NEON(const uint8_t* src, uint32_t lanes, uint32_t loadLaneSize)
{
    if (loadLaneSize == 1) {
        return expandU8ToS16NEON(loadVectorU8NEON(src, lanes));
    }

    assert(loadLaneSize == 2);
    const uint32_t lanes1 = lanes >= 8 ? lanes - 8 : 0;
    const int16x8x2_t result = {{loadVectorS16NEON(src, lanes), loadVectorS16NEON(src + 16, lanes1)}};
    return result;
}

static inline void writeS16AsUNSSE(uint8_t* dst, const int16x8x2_t vec, uint32_t lanes,
                                   uint32_t writeLaneSize, int16x8_t clamp)
{
    if (writeLaneSize == 1) {
        writeVectorU8x16NEON(dst, packS16ToU8NEON(vec), lanes);
        return;
    }

    assert(writeLaneSize == 2);
    const uint32_t lanes1 = lanes >= 8 ? lanes - 8 : 0;

    writeVectorS16NEON(dst, vmaxq_s16(vminq_s16(vec.val[0], clamp), vdupq_n_s16(0)), lanes);
    writeVectorS16NEON(dst + 16, vmaxq_s16(vminq_s16(vec.val[1], clamp), vdupq_n_s16(0)), lanes1);
}

#if VN_ARCH(ARM7A)

static inline int32x4_t vzip1q_s32(const int32x4_t a, const int32x4_t b)
{
    // Produce a0, b0, a1, b2.
    const int32x2_t a1 = vget_low_s32(a);
    const int32x2_t b1 = vget_low_s32(b);
    int32x2x2_t result = vzip_s32(a1, b1);
    return vcombine_s32(result.val[0], result.val[1]);
}

#endif

#endif
#endif /* VN_DEC_CORE_NEON_H_ */
