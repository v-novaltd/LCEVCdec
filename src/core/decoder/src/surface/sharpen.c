/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#include "common/log.h"
#include "common/memory.h"
#include "common/threading.h"
#include "common/types.h"
#include "context.h"
#include "surface/sharpen_common.h"
#include "surface/surface.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct Sharpen
{
    ThreadManager_t* threadManager;
    Memory_t memory;
    Logger_t log;
    bool lockSettings;
    float strength;
    SharpenType_t type;
    Surface_t surfaceIntermediate;
} * Sharpen_t;

/*------------------------------------------------------------------------------*/

bool sharpenInitialize(ThreadManager_t* threadManager, Memory_t memory, Logger_t log,
                       Sharpen_t* sharpenOut, float globalStrength)
{
    Sharpen_t result = VN_MALLOC_T(memory, struct Sharpen);

    if (!result) {
        return false;
    }

    result->threadManager = threadManager;
    result->memory = memory;
    result->log = log;

    if (globalStrength == -1.0f) {
        result->lockSettings = false;
        result->type = STDisabled;
        result->strength = 0.0f;
    } else {
        /* @todo: Determine if we should allow the user to override mode too. */
        result->lockSettings = true;
        result->type = (globalStrength == 0.0f) ? STDisabled : STOutOfLoop;
        result->strength = globalStrength;
    }

    surfaceIdle(&result->surfaceIntermediate);

    *sharpenOut = result;

    return true;
}

void sharpenRelease(Sharpen_t sharpen)
{
    if (sharpen) {
        surfaceRelease(sharpen->memory, &sharpen->surfaceIntermediate);

        Memory_t memory = sharpen->memory;
        VN_FREE(memory, sharpen);
    }
}

bool sharpenSet(Sharpen_t sharpen, SharpenType_t type, float strength)
{
    if (!sharpen) {
        return false;
    }

    /* Ignore settings if they're locked globally. */
    if (!sharpen->lockSettings) {
        sharpen->type = type;
        sharpen->strength = strength;
    }

    return true;
}

SharpenType_t sharpenGetMode(const Sharpen_t sharpen)
{
    return sharpen ? sharpen->type : STDisabled;
}

float sharpenGetStrength(const Sharpen_t sharpen)
{
    if (!sharpen || (sharpen->type == STDisabled)) {
        return 0.0f;
    }

    return sharpen->strength;
}

bool sharpenIsEnabled(Sharpen_t sharpen)
{
    if (!sharpen) {
        return false;
    }

    return sharpen->type != STDisabled && sharpen->strength > 0.0f;
}

/*------------------------------------------------------------------------------*/

SharpenFunction_t surfaceSharpenGetFunctionScalar(FixedPoint_t dstFP);
SharpenFunction_t surfaceSharpenGetFunctionSSE(FixedPoint_t dstFP);
SharpenFunction_t surfaceSharpenGetFunctionNEON(FixedPoint_t dstFP);

SharpenFunction_t surfaceSharpenGetFunction(FixedPoint_t dstFP, CPUAccelerationFeatures_t preferredAccel)
{
    SharpenFunction_t res = NULL;

    if (accelerationFeatureEnabled(preferredAccel, CAFSSE)) {
        res = surfaceSharpenGetFunctionSSE(dstFP);
    }

    if (accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        res = surfaceSharpenGetFunctionNEON(dstFP);
    }

    if (!res) {
        res = surfaceSharpenGetFunctionScalar(dstFP);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

static bool prepareIntermediateSurface(Sharpen_t sharpen, const Surface_t* surface)
{
    const FixedPoint_t fp = surface->type;
    const uint32_t height = surface->height;
    const uint32_t stride = surface->width;

    /* Release previously allocated sharpening surface if not big enough or initialised. */
    if (!surfaceIsIdle(&sharpen->surfaceIntermediate) &&
        !surfaceCompatible(&sharpen->surfaceIntermediate, fp, stride, height, ILNone)) {
        surfaceRelease(sharpen->memory, &sharpen->surfaceIntermediate);
    }

    if (surfaceIsIdle(&sharpen->surfaceIntermediate) &&
        (surfaceInitialise(sharpen->memory, &sharpen->surfaceIntermediate, fp, stride, height,
                           stride, ILNone) != 0)) {
        return false;
    }

    return true;
}

typedef struct SharpenSlicedJobContext
{
    SharpenFunction_t function;
    const Surface_t* src;
    const Surface_t* tmpSurface;
    Dither_t dither;
    float strength;
    const size_t pixelSize;
    const size_t lineCopySize;
} SharpenSlicedJobContext_t;

int32_t sharpenSlicedJob(const void* executeContext, JobIndex_t index, SliceOffset_t offset)
{
    if (isFirstSlice(index)) {
        /* First slice, skip first row. */
        offset.offset += 1;
        offset.count -= 1;
    }

    if (isLastSlice(index)) {
        /* Last slice, skip last row. */
        offset.count -= 1;
    }

    const SharpenSlicedJobContext_t* context = (const SharpenSlicedJobContext_t*)executeContext;
    const SharpenArgs_t args = {context->src,      context->tmpSurface,     context->dither,
                                context->strength, (uint32_t)offset.offset, (uint32_t)offset.count};
    context->function(&args);

    return 0;
}

int32_t sharpenPostRunJob(const void* executeContext, JobIndex_t index, SliceOffset_t offset)
{
    const SharpenSlicedJobContext_t* context = (const SharpenSlicedJobContext_t*)executeContext;

    /* Copy over first and last line, these were purposefully
     * omitted during the sharpen operation to prevent races.
     * Noting for the image borders we take slightly different
     * lines since we contracted during job invocation. */
    const size_t y[2] = {
        isFirstSlice(index) ? 1 : offset.offset,
        isLastSlice(index) ? context->src->height - 2 : offset.offset + offset.count - 1,
    };

    for (int32_t j = 0; j < 2; ++j) {
        const uint32_t lineIndex = (uint32_t)y[j];

        /* Copy line over. */
        const uint8_t* src = surfaceGetLine(context->tmpSurface, lineIndex);
        uint8_t* dst = surfaceGetLine(context->src, lineIndex);

        memoryCopy(dst + context->pixelSize, src + context->pixelSize, context->lineCopySize);
    }
    return 0;
}

bool surfaceSharpen(Sharpen_t sharpen, const Surface_t* surface, Dither_t dither,
                    CPUAccelerationFeatures_t preferredAccel)
{
    if (!sharpen) {
        return false;
    }

    if (surface->interleaving == ILRGB || surface->interleaving == ILRGBA) {
        VN_ERROR(sharpen->log, "sharpen does not support RGB");
        return false;
    }

    if (!prepareIntermediateSurface(sharpen, surface)) {
        VN_ERROR(sharpen->log, "Failed to prepare sharpen intermediate surface\n");
        return false;
    }

    const uint8_t pixelSize = (uint8_t)fixedPointByteSize(surface->type);
    const SharpenSlicedJobContext_t slicedJobContext = {surfaceSharpenGetFunction(surface->type, preferredAccel),
                                                        surface,
                                                        &sharpen->surfaceIntermediate,
                                                        dither,
                                                        sharpen->strength,
                                                        pixelSize,
                                                        (size_t)(surface->width - 2) * pixelSize};

    if (!slicedJobContext.function) {
        VN_ERROR(sharpen->log, "Failed to find sharpen function\n");
        return false;
    }

    return threadingExecuteSlicedJobsWithPostRun(sharpen->threadManager, &sharpenSlicedJob,
                                                 &sharpenPostRunJob, &slicedJobContext, surface->height);
}

/*------------------------------------------------------------------------------*/
