/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <LCEVC/pixel_processing/blit.h>
//
#include "blit_common.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
//
#include <assert.h>

/*------------------------------------------------------------------------------*/

PlaneBlitFunction planeBlitGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                             LdppBlendingMode blending, uint32_t planeIndex, bool isNV12);
PlaneBlitFunction planeBlitGetFunctionSSE(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                          LdppBlendingMode blending, uint32_t planeIndex, bool isNV12);
PlaneBlitFunction planeBlitGetFunctionNEON(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                           LdppBlendingMode blending);

PlaneBlitFunction planeBlitGetFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP, LdppBlendingMode blending,
                                       bool forceScalar, uint32_t planeIndex, bool isNV12)
{
    PlaneBlitFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    if (!forceScalar && acceleration->SSE) {
        res = planeBlitGetFunctionSSE(srcFP, dstFP, blending, planeIndex, isNV12);
    }

    if (!forceScalar && acceleration->NEON) {
        assert(res == NULL);
        // No SIMD functions for BMCopy on NEON - fallthrough to scalar
        res = planeBlitGetFunctionNEON(srcFP, dstFP, blending);
    }

    if (!res) {
        res = planeBlitGetFunctionScalar(srcFP, dstFP, blending, planeIndex, isNV12);
    }

    return res;
}

/*------------------------------------------------------------------------------*/

typedef struct LdppBlitSlicedJobContext
{
    PlaneBlitFunction function;
    const LdpPicturePlaneDesc src;
    const LdpPicturePlaneDesc dst;
    uint32_t minWidth;
} LdppBlitSlicedJobContext;

static bool blitSlicedJob(void* argument, uint32_t offset, uint32_t count)
{
    VNTraceScopedBegin();

    const LdppBlitSlicedJobContext* context = (const LdppBlitSlicedJobContext*)argument;
    const LdppBlitArgs args = {&context->src, &context->dst, context->minWidth, offset, count};

    context->function(&args);

    VNTraceScopedEnd();
    return true;
}

bool ldppPlaneBlit(LdcTaskPool* taskPool, LdcTask* parent, bool forceScalar, const uint32_t planeIndex,
                   const LdpPictureLayout* srcLayout, const LdpPictureLayout* dstLayout,
                   LdpPicturePlaneDesc* srcPlane, LdpPicturePlaneDesc* dstPlane, LdppBlendingMode blending)
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

    LdppBlitSlicedJobContext slicedJobContext = {
        planeBlitGetFunction(srcLayout->layoutInfo->fixedPoint, dstLayout->layoutInfo->fixedPoint,
                             blending, forceScalar, planeIndex, isNV12),
        *srcPlane, *dstPlane, width};

    if (!slicedJobContext.function) {
        VNLogError("failed to find function to perform blitting with\n");
        return false;
    }

    return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &blitSlicedJob, NULL, &slicedJobContext,
                                        sizeof(slicedJobContext), height);
}

/*------------------------------------------------------------------------------*/
