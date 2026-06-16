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

/*
 * Win32 implementation for common clock utilities.
 */
#ifndef VN_LCEVC_COMMON_DETAIL_CLOCK_WIN32_H
#define VN_LCEVC_COMMON_DETAIL_CLOCK_WIN32_H

#include <immintrin.h>
#include <time.h>

#ifdef __MINGW32__
#ifdef __SIZEOF_INT128__
static inline uint64_t _div128(uint64_t hi, uint64_t lo, uint64_t divisor, int64_t* remainder)
{
    if (divisor == 0) {
        return 0;
    }
    __uint128_t dividend = ((__uint128_t)hi << 64) | lo;
    uint64_t quotient = dividend / divisor;
    if (remainder) {
        *remainder = (int64_t)(dividend % divisor);
    }
    return quotient;
}

static inline int64_t mulh(int64_t a, int64_t b)
{
    __int128 result = (__int128)a * (__int128)b;
    return (int64_t)(result >> 64);
}
#else

static inline uint64_t _div128(uint64_t dividend_high, uint64_t dividend_low, uint64_t divisor,
                               int64_t* remainder)
{
    // Handle division by zero
    if (divisor == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return UINT64_MAX;
    }

    // Handle case where dividend < divisor
    if (dividend_high == 0 && dividend_low < divisor) {
        if (remainder) {
            *remainder = dividend_low;
        }
        return 0;
    }

    // Handle case where divisor is 1
    if (divisor == 1) {
        if (remainder) {
            *remainder = 0;
        }
        return dividend_low;
    }

    // Use long division algorithm for 128-bit / 64-bit division
    uint64_t quotient = 0;
    uint64_t current_remainder_high = dividend_high;
    uint64_t current_remainder_low = dividend_low;

    // Find the highest set bit in the dividend
    int shift = 0;
    if (dividend_high != 0) {
        shift = 63 - __builtin_clzll(dividend_high) + 64;
    } else if (dividend_low != 0) {
        shift = 63 - __builtin_clzll(dividend_low);
    } else {
        // Dividend is zero
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }

    // Perform long division bit by bit
    for (int i = shift; i >= 0; i--) {
        // Shift current remainder left by 1
        current_remainder_high = (current_remainder_high << 1) | (current_remainder_low >> 63);
        current_remainder_low = current_remainder_low << 1;

        // Check if we can subtract the divisor
        if (current_remainder_high > divisor ||
            (current_remainder_high == divisor && current_remainder_low >= 0)) {
            // Subtract divisor from current remainder
            if (current_remainder_low < divisor) {
                current_remainder_high--;
            }
            current_remainder_low -= divisor;
            current_remainder_high -= (current_remainder_high >= divisor) ? divisor : 0;

            // Set the corresponding bit in quotient
            if (i < 64) {
                quotient |= (1ULL << i);
            }
        }
    }

    if (remainder) {
        *remainder = (int64_t)current_remainder_low;
    }

    return quotient;
}

static inline uint64_t mulh(uint64_t a, uint64_t b)
{
    // Handle zero cases
    if (a == 0 || b == 0) {
        return 0;
    }

    // Split each 64-bit number into two 32-bit parts
    uint64_t a_low = a & 0xFFFFFFFFULL;
    uint64_t a_high = a >> 32;
    uint64_t b_low = b & 0xFFFFFFFFULL;
    uint64_t b_high = b >> 32;

    // Compute partial products
    uint64_t low_low = a_low * b_low;
    uint64_t low_high = a_low * b_high;
    uint64_t high_low = a_high * b_low;
    uint64_t high_high = a_high * b_high;

    // Combine partial products
    // The full 128-bit result would be:
    // high_high << 64 + (low_high + high_low) << 32 + low_low

    uint64_t middle = low_high + high_low;
    uint64_t middle_carry = (middle < low_high) ? 1 : 0;

    uint64_t low_part_carry = (low_low >> 32) + (middle & 0xFFFFFFFFULL);
    uint64_t final_carry = (low_part_carry >> 32);

    uint64_t result = high_high + (middle >> 32) + middle_carry + final_carry;

    return result;
}
#endif
#endif

static inline uint64_t performanceCounterFrequency(void)
{
    static uint64_t frequency = 0;

    if (frequency == 0) {
        QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
    }

    return frequency;
}

static inline uint64_t performanceCounterScale(uint64_t units)
{
    static uint64_t scaleNanoseconds = 0;
    static uint64_t scaleMicroseconds = 0;
    uint64_t* scale = (units == 1000000000) ? &scaleNanoseconds : &scaleMicroseconds;

    if (*scale == 0) {
        int64_t remainder = 0;
        *scale = _div128(units, 0, performanceCounterFrequency(), &remainder);
    }

    return *scale;
}

static inline uint64_t performanceCounterUnits(uint64_t units)
{
    const uint64_t scale = performanceCounterScale(units);

    uint64_t counter = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
#ifdef __MINGW32__
    return mulh(scale, counter);
#else
    return __mulh(scale, counter);
#endif
}

static inline uint64_t ldcClockMonotonicNanoseconds(void)
{
    const uint64_t frequency = performanceCounterFrequency();

    uint64_t counter = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);

    const uint64_t seconds = counter / frequency;
    const uint64_t remainder = counter % frequency;
    return (seconds * 1000000000) + ((remainder * 1000000000) / frequency);
}

static inline uint64_t ldcClockWallSeconds(void)
{
    const time_t currentTime = time(NULL);
    return (currentTime >= 0) ? (uint64_t)currentTime : 0;
}

static inline uint64_t ldcClockDeadlineMicroseconds(int32_t offset)
{
    if ((uint32_t)offset == UINT32_MAX) {
        return UINT64_MAX;
    }

    return performanceCounterUnits(1000000) + offset;
}

#endif // VN_LCEVC_COMMON_DETAIL_CLOCK_WIN32_H
