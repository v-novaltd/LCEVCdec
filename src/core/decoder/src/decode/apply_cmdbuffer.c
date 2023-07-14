/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "decode/apply_cmdbuffer.h"

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "common/neon.h"
#include "common/sse.h"
#include "context.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionScalar(ApplyCmdBufferMode_t, FixedPoint_t, TransformType_t);
ApplyCmdBufferFunction_t applyCmdBufferGetFunctionSSE(ApplyCmdBufferMode_t, FixedPoint_t, TransformType_t);
ApplyCmdBufferFunction_t applyCmdBufferGetFunctionNEON(ApplyCmdBufferMode_t, FixedPoint_t, TransformType_t);

ApplyCmdBufferFunction_t applyCmdBufferGetFunction(ApplyCmdBufferMode_t mode,
                                                   FixedPoint_t surfaceFP, TransformType_t transform,
                                                   CPUAccelerationFeatures_t preferredAccel)
{
    ApplyCmdBufferFunction_t res = NULL;

    if (accelerationFeatureEnabled(preferredAccel, CAFSSE)) {
        res = applyCmdBufferGetFunctionSSE(mode, surfaceFP, transform);
    }

    if (accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        assert(res == NULL);
        res = applyCmdBufferGetFunctionNEON(mode, surfaceFP, transform);
    }

    if (!res) {
        res = applyCmdBufferGetFunctionScalar(mode, surfaceFP, transform);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferSlicedJobContext
{
    ApplyCmdBufferFunction_t function;
    const Surface_t* surface;
    CmdBufferData_t* buffer;
    int32_t layerCount;
    const Highlight_t* highlight;
} ApplyCmdBufferSlicedJobContext_t;

int32_t applyCmdBufferSlicedJob(const void* executeContext, JobIndex_t index, SliceOffset_t offset)
{
    const ApplyCmdBufferSlicedJobContext_t* context =
        (const ApplyCmdBufferSlicedJobContext_t*)executeContext;

    ApplyCmdBufferArgs_t args = {
        .surface = context->surface,
        .coords = context->buffer->coordinates + (offset.offset * 2),
        .residuals = context->buffer->residuals + (offset.offset * context->layerCount),
        .count = (int32_t)offset.count,
        .highlight = context->highlight,
    };
    context->function(&args);
    return 0;
}

bool applyCmdBuffer(Context_t* ctx, const CmdBuffer_t* buffer, const Surface_t* surface,
                    ApplyCmdBufferMode_t mode, CPUAccelerationFeatures_t accel, const Highlight_t* highlight)
{
    if (!surface->data) {
        VN_ERROR(ctx->log, "apply cmdbuffer surface has no data pointer\n");
        return false;
    }

    if (surface->interleaving != ILNone) {
        VN_ERROR(ctx->log, "apply cmdbuffer does not support interleaved destination surfaces\n");
        return false;
    }

    if (cmdBufferIsEmpty(buffer)) {
        return true;
    }

    if ((mode == ACBM_Highlight) && (highlight == NULL)) {
        VN_ERROR(
            ctx->log,
            "apply cmdbuffer failed, requested highlight, but highlight state was not supplied\n");
        return false;
    }

    const int32_t layerCount = cmdBufferGetLayerCount(buffer);
    const TransformType_t transform =
        (mode != ACBM_Tiles) ? transformTypeFromLayerCount(layerCount) : TransformDD;

    ApplyCmdBufferFunction_t function = applyCmdBufferGetFunction(mode, surface->type, transform, accel);

    if (!function) {
        VN_ERROR(ctx->log, "apply cmdbuffer failed, unsupported configuration\n");
        return false;
    }

    CmdBufferData_t bufferData = cmdBufferGetData(buffer);

    const ApplyCmdBufferSlicedJobContext_t slicedJobContext = {
        .function = function,
        .surface = surface,
        .buffer = &bufferData,
        .layerCount = layerCount,
        .highlight = (mode == ACBM_Highlight) ? highlight : NULL,
    };

    return threadingExecuteSlicedJobs(&ctx->threadManager, &applyCmdBufferSlicedJob,
                                      &slicedJobContext, bufferData.count);
}

/*------------------------------------------------------------------------------*/