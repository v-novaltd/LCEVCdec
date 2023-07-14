/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "unit_rng.h"

#include <iostream>

RNG::RNG(uint32_t range)
    : m_seed(std::random_device{}())
    , m_engine{m_seed}
    , m_data{0, range}
{
    std::cout << "Random seed: " << m_seed << std::endl;
}