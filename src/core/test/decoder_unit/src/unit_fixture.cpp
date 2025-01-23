/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

void ContextWrapper::initialize(Memory_t memory, Logger_t log)
{
    memset(&ctx, 0, sizeof(Context_t));

    threadingInitialise(memory, log, &ctx.threadManager, 1);

    ditherInitialize(memory, &ctx.dither, 0, true, -1);

    sharpenInitialize(&ctx.threadManager, memory, log, &ctx.sharpen, -1.0f);
}

void ContextWrapper::release()
{
    sharpenRelease(ctx.sharpen);
    ditherRelease(ctx.dither);
}

void LoggerWrapper::initialize(Memory_t memory)
{
    memset(&log, 0, sizeof(Logger_t));

    LoggerSettings_t loggerSettings = {0};
    loggerSettings.callback = &logCallback;
    logInitialize(memory, &log, &loggerSettings);
}

void LoggerWrapper::release() { logRelease(log); }

void MemoryWrapper::initialize()
{
    memset(&memory, 0, sizeof(Memory_t));

    const MemorySettings_t memorySettings = {0};
    memoryInitialise(&memory, &memorySettings);
}

void MemoryWrapper::release() { memoryRelease(memory); }
