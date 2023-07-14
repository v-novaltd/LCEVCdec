/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_fixture.h"

extern "C"
{
#include "common/dither.h"
#include "common/log.h"
#include "common/memory.h"
}

namespace lcevc_dec {

void logCallback(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength)
{
    printf("%s", msg);
}

void Fixture::SetUp(benchmark::State& st)
{
    MemorySettings_t memorySettings = {0};
    memoryInitialise(&ctx.memory, &memorySettings);

    LoggerSettings_t loggerSettings = {0};
    loggerSettings.callback = &logCallback;
    logInitialize(ctx.memory, &ctx.log, &loggerSettings);

    ctx.useOldCodeLengths = false;

    ditherInitialize(ctx.memory, &ctx.dither, 0, true);
}

void Fixture::TearDown(benchmark::State& state)
{
    ditherRelease(ctx.dither);
    logRelease(ctx.log);
    memoryRelease(ctx.memory);
}

} // namespace lcevc_dec