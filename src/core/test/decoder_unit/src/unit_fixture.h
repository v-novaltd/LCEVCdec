/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include <gtest/gtest.h>

extern "C"
{
#include "context.h"
}

// -----------------------------------------------------------------------------

class ContextWrapper
{
public:
    void initialize();
    void release();

    Context_t* get() { return &ctx; }

    Context_t ctx;
};

class Fixture : public testing::Test
{
protected:
    void SetUp() override { context.initialize(); }
    void TearDown() override { context.release(); }
    ContextWrapper context;
};

template <typename T>
class FixtureWithParam : public testing::TestWithParam<T>
{
protected:
    void SetUp() override { context.initialize(); }
    void TearDown() override { context.release(); }
    ContextWrapper context;
};

// -----------------------------------------------------------------------------
