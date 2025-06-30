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

    ditherInitialize(ctx.memory, &ctx.dither, 0, true, -1);
}

void Fixture::TearDown(benchmark::State& state)
{
    ditherRelease(ctx.dither);
    logRelease(ctx.log);
    memoryRelease(ctx.memory);
}

} // namespace lcevc_dec
