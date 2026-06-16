/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <LCEVC/pixel_processing/add.h>
//
#include "add_common.h"
#include "fp_types.h"

#include <LCEVC/pipeline/types.h>
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
//
#include <assert.h>

/*------------------------------------------------------------------------------*/

PlaneAddFunction planeAddGetFunctionScalar(LdpFixedPoint dstFP);
PlaneAddFunction planeAddGetFunctionSSE(LdpFixedPoint dstFP);
PlaneAddFunction planeAddGetFunctionNEON(LdpFixedPoint dstFP);

PlaneAddFunction planeAddGetFunction(LdpFixedPoint dstFP)
{
    PlaneAddFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    if (fixedPointIsSigned(dstFP)) {
        VNLogError("Cannot add to a signed fixed point plane");
        return NULL;
    }

    /* Find a SIMD function */
    if (acceleration->hasSSE) {
        res = planeAddGetFunctionSSE(dstFP);
    }
    if (acceleration->hasNeon) {
        assert(res == NULL);
        res = planeAddGetFunctionNEON(dstFP);
    }

    /* Fallback to a non-SIMD function */
#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    if (!res) {
        res = planeAddGetFunctionScalar(dstFP);
    }
#endif

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct LdppAddSlicedJobContext
{
    PlaneAddFunction function;
    const LdpPicturePlaneDesc src1;
    const LdpPicturePlaneDesc src2;
    const LdpPicturePlaneDesc dst;
    uint32_t minWidth;

#if VN_SDK_FEATURE(TRACING)
    LdpPipelineDiagInfo diagInfo;
#endif
} LdppAddSlicedJobContext;

static bool addSlicedJob(void* argument, uint32_t offset, uint32_t count)
{
    const LdppAddSlicedJobContext* context = (const LdppAddSlicedJobContext*)argument;
    const LdppAddArgs args = {&context->src1,    &context->src2, &context->dst,
                              context->minWidth, offset,         count};
    VNTraceScopedBeginArgs("task", context->diagInfo.task, "timestamp", context->diagInfo.timestamp,
                           "loq", context->diagInfo.loq, "plane", context->diagInfo.plane, "offset",
                           offset, "count", count);

    context->function(&args);

    VNTraceScopedEnd();
    return true;
}

bool ldppPlaneAdd(LdcTaskPool* taskPool, LdcTask* parent, uint32_t planeIndex,
                  const LdpPictureLayout* srcLayout, const LdpPictureLayout* dstLayout,
                  LdpPicturePlaneDesc* srcPlane1, LdpPicturePlaneDesc* srcPlane2,
                  LdpPicturePlaneDesc* dstPlane, const LdpPipelineDiagInfo* diagInfo)
{
    const uint32_t width =
        minU32(srcLayout->width >> srcLayout->layoutInfo->planeWidthShift[planeIndex],
               dstLayout->width >> dstLayout->layoutInfo->planeWidthShift[planeIndex]);

    const uint32_t height =
        minU32(srcLayout->height >> srcLayout->layoutInfo->planeHeightShift[planeIndex],
               dstLayout->height >> dstLayout->layoutInfo->planeHeightShift[planeIndex]);

    const LdppAddSlicedJobContext slicedJobContext = {
        planeAddGetFunction(dstLayout->layoutInfo->fixedPoint),
        *srcPlane1,
        *srcPlane2,
        *dstPlane,
        width,
#if VN_SDK_FEATURE(TRACING)
        *diagInfo
#endif
    };

    if (!slicedJobContext.function) {
        VNLogError("Cannot find a suitable function for plane addition");
        return false;
    }

    return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &addSlicedJob, NULL, &slicedJobContext,
                                        sizeof(slicedJobContext), height);
}

/*------------------------------------------------------------------------------*/
