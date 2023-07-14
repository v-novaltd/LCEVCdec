/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "unit_fixture.h"

extern "C"
{
#include "common/dither.h"
#include "common/log.h"
#include "common/memory.h"
#include "context.h"
#include "surface/sharpen.h"
}

inline void logCallback(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength)
{
    printf("%s", msg);
}

void ContextWrapper::initialize()
{
    memset(&ctx, 0, sizeof(Context_t));

    const MemorySettings_t memorySettings = {0};
    memoryInitialise(&ctx.memory, &memorySettings);

    LoggerSettings_t loggerSettings = {0};
    loggerSettings.callback = &logCallback;
    logInitialize(ctx.memory, &ctx.log, &loggerSettings);

    ditherInitialize(ctx.memory, &ctx.dither, 0, true);

    sharpenInitialize(&ctx, &ctx.sharpen, -1.0f);

    ctx.useOldCodeLengths = false;
}

void ContextWrapper::release()
{
    sharpenRelease(ctx.sharpen);
    ditherRelease(ctx.dither);
    logRelease(ctx.log);
    memoryRelease(ctx.memory);
}