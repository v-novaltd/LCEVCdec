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

#include "surface/blit.h"

#include "common/log.h"
#include "common/threading.h"
#include "common/types.h"
#include "lcevc_config.h"
#include "surface/blit_common.h"
#include "surface/surface.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    VN_UNUSED(index);
    const BlitSlicedJobContext_t* context = (const BlitSlicedJobContext_t*)executeContext;
    const BlitArgs_t args = {context->src, context->dst, (uint32_t)offset.offset, (uint32_t)offset.count};

    context->function(&args);
    return 0;
}

bool surfaceBlit(Logger_t log, ThreadManager_t* threadManager, CPUAccelerationFeatures_t cpuFeatures,
                 const Surface_t* src, const Surface_t* dst, BlendingMode_t blending)
{
    if (src->interleaving != dst->interleaving) {
        VN_ERROR(log, "blit requires both src and dst ilvl to be the same\n");
        return false;
    }

    const BlitSlicedJobContext_t slicedJobContext = {
        surfaceBlitGetFunction(src->type, dst->type, src->interleaving, blending, cpuFeatures), src, dst};

    if (!slicedJobContext.function) {
        VN_ERROR(log, "failed to find function to perform blitting with\n");
        return false;
    }

    return threadingExecuteSlicedJobs(threadManager, &blitSlicedJob, &slicedJobContext,
                                      minU32(src->height, dst->height));
}

/*------------------------------------------------------------------------------*/
