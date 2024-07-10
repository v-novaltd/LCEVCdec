/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef CORE_TEST_UNIT_RNG_H_
#define CORE_TEST_UNIT_RNG_H_

#include <cstdint>
#include <random>

// Return an equivalent SIMD flag for the passed in flag - or just return
// the passed in flag. The return value depends on the platform and feature support.
class RNG
{
public:
    explicit RNG(uint32_t range);
    inline auto operator()() { return m_data(m_engine); }

private:
    uint32_t m_seed;
    std::mt19937 m_engine;
    std::uniform_int_distribution<std::mt19937::result_type> m_data;
};

#endif // CORE_TEST_UNIT_RNG_H_