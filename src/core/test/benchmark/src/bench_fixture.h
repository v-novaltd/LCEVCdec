/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#pragma once

extern "C"
{
#include "context.h"
}

#include <benchmark/benchmark.h>

#include <system_error>

namespace lcevc_dec {

// Base class for fixtures that handles common configuration of the DPI
class Fixture : public benchmark::Fixture
{
public:
    virtual ~Fixture() = default;

    void SetUp(benchmark::State& st) override;
    void TearDown(benchmark::State& state) override;

    Context_t ctx;
};

} // namespace lcevc_dec
