/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include <gtest/gtest.h>

extern "C"
{
#include "common/log.h"
#include "common/memory.h"
#include "context.h"
}

// -----------------------------------------------------------------------------

class ContextWrapper
{
public:
    void initialize(Memory_t memory, Logger_t log);
    void release();

    Context_t* get() { return &ctx; }

    Context_t ctx;
};

class LoggerWrapper
{
public:
    void initialize(Memory_t memory);
    void release();

    Logger_t& get() { return log; }

    Logger_t log;
};

class MemoryWrapper
{
public:
    void initialize();
    void release();

    Memory_t& get() { return memory; }

    Memory_t memory;
};

class Fixture : public testing::Test
{
protected:
    void SetUp() override
    {
        memoryWrapper.initialize();
        logWrapper.initialize(memoryWrapper.get());
        contextWrapper.initialize(memoryWrapper.get(), logWrapper.get());
    }
    void TearDown() override
    {
        contextWrapper.release();
        logWrapper.release();
        memoryWrapper.release();
    }
    ContextWrapper contextWrapper;
    LoggerWrapper logWrapper;
    MemoryWrapper memoryWrapper;
};

template <typename T>
class FixtureWithParam : public testing::TestWithParam<T>
{
protected:
    void SetUp() override
    {
        memoryWrapper.initialize();
        logWrapper.initialize(memoryWrapper.get());
        contextWrapper.initialize(memoryWrapper.get(), logWrapper.get());
    }
    void TearDown() override
    {
        contextWrapper.release();
        logWrapper.release();
        memoryWrapper.release();
    }

    ContextWrapper contextWrapper;
    LoggerWrapper logWrapper;
    MemoryWrapper memoryWrapper;
};

// -----------------------------------------------------------------------------
