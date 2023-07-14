/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

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