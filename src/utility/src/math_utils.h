/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Miscellaneous simple numeric functions
//
#ifndef VN_LCEVC_UTILITY_MATH_UTILS_H
#define VN_LCEVC_UTILITY_MATH_UTILS_H

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#if defined WIN32
#include <intrin.h>
#endif

namespace lcevc_dec::utility {

//! Count leading zeros helper
//
static inline uint8_t clz(uint32_t n)
{
#if defined _WIN32 && defined _MSC_VER
    return (uint8_t)(_lzcnt_u32(n));
#elif defined __linux__ && (__GNUC__ >= 14)
    return (uint8_t)(__builtin_clzg(n, (int) (8 * sizeof(n))));
#else
    if (n == 0) {
        return (8 * sizeof(n));
    }
    return (uint8_t)(__builtin_clz(n));
#endif
}

//! Round n up to the next largest power of 2
//
inline uint32_t nextPow2(uint32_t n)
{
    const uint8_t leadingZeroes = clz(n);
    return (1 << ((8 * sizeof(n)) - leadingZeroes));
}

} // namespace lcevc_dec::utility
#endif
