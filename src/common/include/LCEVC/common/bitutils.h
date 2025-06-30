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

#ifndef VN_LCEVC_COMMON_BITUTILS_H
#define VN_LCEVC_COMMON_BITUTILS_H

#include <stdint.h>

// Saturating shifts - result is 0 if count >= total_bits
static inline uint16_t shl16(uint16_t value, unsigned shift)
{
    return (shift < 16) ? (value << shift) : 0;
}
static inline uint32_t shl32(uint32_t value, unsigned shift)
{
    return (shift < 32) ? (value << shift) : 0;
}
static inline uint64_t shl64(uint64_t value, unsigned shift)
{
    return (shift < 64) ? (value << shift) : 0;
}

static inline uint16_t shr16(uint16_t value, unsigned shift)
{
    return (shift < 16) ? (value >> shift) : 0;
}
static inline uint32_t shr32(uint32_t value, unsigned shift)
{
    return (shift < 32) ? (value >> shift) : 0;
}
static inline uint64_t shr64(uint64_t value, unsigned shift)
{
    return (shift < 64) ? (value >> shift) : 0;
}

// Funnel shift - pick bits from concatenation of two inputs
static inline uint16_t funnel16(uint16_t high, uint16_t low, unsigned shift)
{
    return shl16(high, shift) | shr16(low, (16 - shift));
}
static inline uint32_t funnel32(uint32_t high, uint32_t low, unsigned shift)
{
    return shl32(high, shift) | shr32(low, (32 - shift));
}
static inline uint64_t funnel64(uint64_t high, uint64_t low, unsigned shift)
{
    return shl64(high, shift) | shr64(low, (64 - shift));
}

#ifndef _MSC_VER
// Count leading zeros
static inline uint16_t clz16(uint16_t value) { return (uint16_t)__builtin_clz(value); }
static inline uint32_t clz32(uint32_t value) { return __builtin_clz(value); }
static inline uint64_t clz64(uint64_t value) { return __builtin_clzl(value); }

// Count trailing zeros
static inline uint16_t ctz16(uint16_t value) { return (uint16_t)__builtin_ctz(value); }
static inline uint32_t ctz32(uint32_t value) { return __builtin_ctz(value); }
static inline uint64_t ctz64(uint64_t value) { return __builtin_ctzl(value); }

// Endian swap
static inline uint16_t bswap16(uint16_t value) { return __builtin_bswap16(value); }
static inline uint32_t bswap32(uint32_t value) { return __builtin_bswap32(value); }
static inline uint64_t bswap64(uint64_t value) { return __builtin_bswap64(value); }

#else

#include <intrin.h>

// Count leading zeros
static uint32_t __inline clz16(uint16_t value)
{
    unsigned long leading_zero = 0;

    if (_BitScanReverse(&leading_zero, value)) {
        return 15 - leading_zero;
    } else {
        return 16;
    }
}

static uint32_t __inline clz32(uint32_t value)
{
    unsigned long leading_zero = 0;

    if (_BitScanReverse(&leading_zero, value)) {
        return 31 - leading_zero;
    } else {
        return 32;
    }
}

static uint32_t __inline clz64(uint64_t value)
{
    unsigned long leading_zero = 0;

    if (_BitScanReverse64(&leading_zero, value)) {
        return 63 - leading_zero;
    } else {
        return 64;
    }
}

static uint32_t __inline ctz16(uint16_t value)
{
    unsigned long trailing_zero = 0;

    if (_BitScanForward(&trailing_zero, value)) {
        return trailing_zero;
    } else {
        return 16;
    }
}

static uint32_t __inline ctz32(uint32_t value)
{
    unsigned long trailing_zero = 0;

    if (_BitScanForward(&trailing_zero, value)) {
        return trailing_zero;
    } else {
        return 32;
    }
}

static uint32_t __inline ctz64(uint64_t value)
{
    unsigned long trailing_zero = 0;

    if (_BitScanForward64(&trailing_zero, value)) {
        return trailing_zero;
    } else {
        return 64;
    }
}

static inline uint16_t bswap16(uint16_t value) { return _byteswap_ushort(value); }
static inline uint32_t bswap32(uint32_t value) { return _byteswap_ulong(value); }
static inline uint64_t bswap64(uint64_t value) { return _byteswap_uint64(value); }

#endif

#endif // VN_LCEVC_COMMON_BITUTILS_H
