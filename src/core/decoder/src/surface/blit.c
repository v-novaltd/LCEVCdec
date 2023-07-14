/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "surface/blit.h"

#include "context.h"
#include "surface/blit_common.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

BlitFunction_t surfaceBlitGetFunctionScalar(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending);
BlitFunction_t surfaceBlitGetFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending);
BlitFunction_t surfaceBlitGetFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP, BlendingMode_t blending);

BlitFunction_t surfaceBlitGetFunction(FixedPoint_t srcFP, FixedPoint_t dstFP, Interleaving_t interleaving,
                                      BlendingMode_t blending, CPUAccelerationFeatures_t preferredAccel)
{
    VN_UNUSED(interleaving);

    BlitFunction_t res = NULL;

    if (accelerationFeatureEnabled(preferredAccel, CAFSSE)) {
        res = surfaceBlitGetFunctionSSE(srcFP, dstFP, blending);
    }

    if (accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        assert(res == NULL);
        res = surfaceBlitGetFunctionNEON(srcFP, dstFP, blending);
    }

    if (!res) {
        res = surfaceBlitGetFunctionScalar(srcFP, dstFP, blending);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct BlitSlicedJobContext
{
    BlitFunction_t function;
    const Surface_t* src;
    const Surface_t* dst;
} BlitSlicedJobContext_t;

int32_t blitSlicedJob(const void* executeContext, JobIndex_t index, SliceOffset_t offset)
{
    const BlitSlicedJobContext_t* context = (const BlitSlicedJobContext_t*)executeContext;
    const BlitArgs_t args = {context->src, context->dst, (uint32_t)offset.offset, (uint32_t)offset.count};

    context->function(&args);
    return 0;
}

bool surfaceBlit(Context_t* ctx, const Surface_t* src, const Surface_t* dst, BlendingMode_t blending)
{
    ThreadManager_t* threadManager = &ctx->threadManager;

    if (src->interleaving != dst->interleaving) {
        VN_ERROR(ctx->log, "blit requires both src and dst ilvl to be the same\n");
        return false;
    }

    const BlitSlicedJobContext_t slicedJobContext = {
        surfaceBlitGetFunction(src->type, dst->type, src->interleaving, blending, ctx->cpuFeatures),
        src, dst};

    if (!slicedJobContext.function) {
        VN_ERROR(ctx->log, "failed to find function to perform blitting with\n");
        return false;
    }

    return threadingExecuteSlicedJobs(threadManager, &blitSlicedJob, &slicedJobContext, dst->height);
}

/*------------------------------------------------------------------------------*/
