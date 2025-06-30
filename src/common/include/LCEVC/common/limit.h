/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_LIMIT_H
#define VN_LCEVC_COMMON_LIMIT_H

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* This function is used help convert upsample kernel coeffs for 8bit pipeline. 64 is 2^6. */
static inline int16_t fpS15ToS7(int16_t val) { return (int16_t)((val + 64) >> 7); }

static inline int16_t fpU16ToS16(uint16_t val, int16_t shift)
{
    int16_t res = (int16_t)(val << shift);
    res -= 0x4000;
    return res;
}

static inline uint16_t fpS16ToU16(int32_t val, int16_t shift, int16_t rounding, int16_t signOffset,
                                  uint16_t maxValue)
{
    int16_t res = (int16_t)(((val + rounding) >> shift) + signOffset);

    if (res > (int16_t)maxValue) {
        return maxValue;
    }

    if (res < 0x00) {
        return 0x00;
    }

    return (uint16_t)res;
}

static inline int16_t fpU8ToS8(uint8_t val) { return fpU16ToS16(val, 7); }
static inline int16_t fpU10ToS10(uint16_t val) { return fpU16ToS16(val, 5); }
static inline int16_t fpU12ToS12(uint16_t val) { return fpU16ToS16(val, 3); }
static inline int16_t fpU14ToS14(uint16_t val) { return fpU16ToS16(val, 1); }

static inline uint8_t fpS8ToU8(int32_t val)
{
    return (uint8_t)fpS16ToU16(val, 7, 0x40, 0x80, 0xFF);
}
static inline uint16_t fpS10ToU10(int32_t val) { return fpS16ToU16(val, 5, 0x10, 0x200, 0x3FF); }
static inline uint16_t fpS12ToU12(int32_t val) { return fpS16ToU16(val, 3, 0x4, 0x800, 0xFFF); }
static inline uint16_t fpS14ToU14(int32_t val) { return fpS16ToU16(val, 1, 0x1, 0x2000, 0x3FFF); }

/*------------------------------------------------------------------------------*/

static inline uint16_t alignU16(uint16_t value, uint16_t alignment)
{
    if (alignment == 0) {
        return value;
    }

    return (value + (alignment - 1)) & ~(alignment - 1);
}

static inline uint32_t alignU32(uint32_t value, uint32_t alignment)
{
    if (alignment == 0) {
        return value;
    }

    return (value + (alignment - 1)) & ~(alignment - 1);
}

static inline uint16_t clampU16(uint16_t value, uint16_t minValue, uint16_t maxValue)
{
    uint16_t ret = value;
    if (value < minValue) {
        ret = minValue;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline int32_t clampS32(int32_t value, int32_t minValue, int32_t maxValue)
{
    int32_t ret = value;
    if (value < minValue) {
        ret = minValue;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue)
{
    uint32_t ret = value;
    if (value < minValue) {
        ret = minValue;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline int64_t clampS64(int64_t value, int64_t minValue, int64_t maxValue)
{
    int64_t ret = value;
    if (value < minValue) {
        ret = minValue;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline float clampF32(float value, float minValue, float maxValue)
{
    float ret = value;
    if (value < minValue) {
        ret = minValue;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline float floorF32(float value) { return floorf(value); }

static inline int32_t minS16(int16_t x, int16_t y) { return x < y ? x : y; }

static inline int32_t minS32(int32_t x, int32_t y) { return x < y ? x : y; }

static inline uint8_t minU8(uint8_t x, uint8_t y) { return x < y ? x : y; }

static inline uint16_t minU16(uint16_t x, uint16_t y) { return x < y ? x : y; }

static inline uint32_t minU32(uint32_t x, uint32_t y) { return x < y ? x : y; }

static inline uint64_t minU64(uint64_t x, uint64_t y) { return x < y ? x : y; }

static inline int32_t maxS32(int32_t x, int32_t y) { return x > y ? x : y; }

static inline uint32_t maxU32(uint32_t x, uint32_t y) { return x > y ? x : y; }

static inline uint64_t maxU64(uint64_t x, uint64_t y) { return x > y ? x : y; }

static inline size_t minSize(size_t x, size_t y) { return x < y ? x : y; }

static inline size_t maxSize(size_t x, size_t y) { return x > y ? x : y; }

static inline uint8_t saturateU8(int32_t value)
{
    const int32_t result = clampS32(value, 0, 255);
    return (uint8_t)result;
}

/* S15 saturation is for the END of upscaling (this is the thing that you apply residuals to, so
 * the maximum and minimum values must be one maximum-residual apart). */
static inline int16_t saturateS15(int32_t value)
{
    const int32_t result = minS32(maxS32(value, -16384), 16383);
    return (int16_t)result;
}

/* S16 saturation is for RESIDUALS (and general use demoting int32_ts to int16_ts). */
static inline int16_t saturateS16(int32_t value)
{
    const int32_t result = clampS32(value, -32768, 32767);
    return (int16_t)result;
}

static inline uint16_t saturateUN(int32_t value, uint16_t maxValue)
{
    uint16_t ret = (uint16_t)value;
    if (value < 0) {
        ret = 0;
    } else if (value > maxValue) {
        ret = maxValue;
    }
    return ret;
}

static inline int32_t divideCeilS32(int32_t numerator, int32_t denominator)
{
    /* This function is for ceiling a positive division. */
    assert(numerator > 0);
    assert(denominator > 0);

    if (denominator == 0) {
        return 0;
    }

    return (numerator + denominator - 1) / denominator;
}

static inline uint16_t divideCeilU16(uint16_t numerator, uint16_t denominator)
{
    /* This function is for ceiling a positive division. */
    assert(numerator > 0);
    assert(denominator > 0);

    if (denominator == 0) {
        return 0;
    }

    return (numerator + denominator - 1) / denominator;
}

static inline uint32_t nextPowerOfTwoU32(uint32_t val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;
    return val;
}

static inline uint16_t nextPowerOfTwoU16(uint16_t val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val++;
    return val;
}

static inline uint8_t nextPowerOfTwoU8(uint8_t val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val++;
    return val;
}

#endif // VN_LCEVC_COMMON_LIMIT_H
