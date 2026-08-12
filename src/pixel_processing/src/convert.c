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

#include <LCEVC/pixel_processing/convert.h>
//
#include "convert_common.h"

#include <LCEVC/pipeline/types.h>
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
//
#include <assert.h>

/*------------------------------------------------------------------------------*/

PlaneConvertFunction planeConvertGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                                   uint32_t planeIndex, bool isNV12);
PlaneConvertFunction planeConvertGetFunctionSSE(LdpFixedPoint srcFP, LdpFixedPoint dstFP, bool isNV12);
PlaneConvertFunction planeConvertGetFunctionNEON(LdpFixedPoint srcFP, LdpFixedPoint dstFP, bool isNV12);

PlaneConvertFunction planeConvertGetFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                             uint32_t planeIndex, bool isNV12)
{
    PlaneConvertFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    /* Find a SIMD function */
    if (acceleration->hasSSE) {
        res = planeConvertGetFunctionSSE(srcFP, dstFP, isNV12);
    }
    if (acceleration->hasNeon) {
        assert(res == NULL);
        res = planeConvertGetFunctionNEON(srcFP, dstFP, isNV12);
    }

    if (!res) {
        res = planeConvertGetFunctionScalar(srcFP, dstFP, planeIndex, isNV12);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct LdppConvertSlicedJobContext
{
    PlaneConvertFunction function;
    const LdpPicturePlaneDesc src;
    const LdpPicturePlaneDesc dst;
    uint32_t minWidth;

#if VN_SDK_FEATURE(TRACING)
    LdpPipelineDiagInfo diagInfo;
#endif
} LdppConvertSlicedJobContext;

static bool ConvertSlicedJob(void* argument, uint32_t offset, uint32_t count)
{
    const LdppConvertSlicedJobContext* context = (const LdppConvertSlicedJobContext*)argument;
    const LdppConvertArgs args = {&context->src, &context->dst, context->minWidth, offset, count};

    VNTraceScopedBeginArgs("task", context->diagInfo.task, "timestamp", context->diagInfo.timestamp,
                           "loq", context->diagInfo.loq, "plane", context->diagInfo.plane, "offset",
                           offset, "count", count);

    context->function(&args);

    VNTraceScopedEnd();
    return true;
}

bool ldppPlaneConvert(LdcTaskPool* taskPool, LdcTask* parent, const uint32_t planeIndex,
                      const LdpPictureLayout* srcLayout, const LdpPictureLayout* dstLayout,
                      LdpPicturePlaneDesc* srcPlane, LdpPicturePlaneDesc* dstPlane,
                      const LdpPipelineDiagInfo* diagInfo)
{
    const uint32_t width =
        minU32(srcLayout->width >> srcLayout->layoutInfo->planeWidthShift[planeIndex],
               dstLayout->width >> dstLayout->layoutInfo->planeWidthShift[planeIndex]);

    const uint32_t height =
        minU32(srcLayout->height >> srcLayout->layoutInfo->planeHeightShift[planeIndex],
               dstLayout->height >> dstLayout->layoutInfo->planeHeightShift[planeIndex]);

    const bool isNV12 = srcLayout->layoutInfo->format == LdpColorFormatNV12_8 ||
                        dstLayout->layoutInfo->format == LdpColorFormatNV12_8;
    if (isNV12 && planeIndex == 2) {
        if (srcLayout->layoutInfo->fixedPoint == LdpFPU8) {
            srcPlane->firstSample++;
        }
        if (dstLayout->layoutInfo->fixedPoint == LdpFPU8) {
            dstPlane->firstSample++;
        }
    }

    const LdppConvertSlicedJobContext slicedJobContext = {
        planeConvertGetFunction(srcLayout->layoutInfo->fixedPoint,
                                dstLayout->layoutInfo->fixedPoint, planeIndex, isNV12),
        *srcPlane,
        *dstPlane,
        width,
#if VN_SDK_FEATURE(TRACING)
        *diagInfo
#endif
    };

    if (!slicedJobContext.function) {
        VNLogError("Failed to find plane conversion function");
        return false;
    }

    return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &ConvertSlicedJob, NULL,
                                        &slicedJobContext, sizeof(slicedJobContext), height);
}

/*------------------------------------------------------------------------------*/
