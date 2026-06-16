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

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pixel_processing/upscale.h>
//

#include "fp_types.h"
#include "upscale_common.h"
//
#include <assert.h>

/*------------------------------------------------------------------------------*/

UpscaleFunction getUpscaleFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP, Interleaving interleaving,
                                   bool is2d, bool pa, bool ditheringEnabled)
{
    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        VNLogError("Invalid 2D function request - srcFp or dstFp is invalid");
        return NULL;
    }

    UpscaleFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    if (acceleration->hasSSE) {
        res = upscaleGetFunctionSSE(srcFP, dstFP, interleaving, is2d, pa, ditheringEnabled);
    }
    if (acceleration->hasNeon) {
        res = upscaleGetFunctionNEON(srcFP, dstFP, interleaving, is2d, pa, ditheringEnabled);
    }

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    if (!res) {
        res = upscaleGetFunctionScalar(srcFP, dstFP, interleaving, is2d, pa, ditheringEnabled);
    }
#endif

    return res;
}

static bool interleavingEqual(const LdpPictureLayoutInfo* layoutLeft, const LdpPictureLayoutInfo* layoutRight)
{
    return 0 == memcmp(layoutLeft->interleave, layoutRight->interleave, kLdpPictureMaxNumPlanes);
}

static Interleaving getInterleaving(const LdpPictureLayoutInfo* layout, const uint32_t planeIndex)
{
    switch (layout->interleave[planeIndex]) {
        case 1: return ILNone;
        case 2: return ILNV12;
    }

    return ILCount;
}

/*------------------------------------------------------------------------------*/

/* Upscale threading shared state. */
typedef struct UpscaleSlicedJobContext
{
    uint32_t planeIndex;
    const LdpPictureLayout* srcLayout;
    const LdpPictureLayout* dstLayout;
    LdpPicturePlaneDesc srcPlane;
    LdpPicturePlaneDesc dstPlane;
    UpscaleFunction upscaleFunction;
    bool applyPA;
    bool is2D;
    LdeKernel kernel;
    const LdppDitherFrame* frameDither;

#if VN_SDK_FEATURE(TRACING)
    LdpPipelineDiagInfo diagInfo;
#endif
} UpscaleSlicedJobContext;

/*------------------------------------------------------------------------------*/

/*! \brief Populates LdppUpscaleParams with constants required for upscaling and inline conversion
 *         to and from dst picture format and internal signed 16-bit format.
 *         See `LdppUpscaleParams` docs for details on each param.
 */

static LdppUpscaleParams generateUpscaleParams(const LdeKernel* kernel, LdpFixedPoint dstFp)
{
    LdppUpscaleParams params = {0};
    params.kernel[0] = kernel->coeffs[0];
    params.kernel[1] = kernel->coeffs[1];
    params.kernel[2] = kernel->coeffs[2];
    params.kernel[3] = kernel->coeffs[3];

    switch (dstFp) {
        // Unsigned planes (direct upscale)
        case LdpFPU8: {
            params.shift = 7;
            params.offset = 64;
            params.midpoint = 128;
            break;
        }
        case LdpFPU10:
            params.shift = 5;
            params.offset = 16;
            params.midpoint = 512;
            break;
        case LdpFPU12:
            params.shift = 3;
            params.offset = 4;
            params.midpoint = 2048;
            break;
        case LdpFPU14:
            params.shift = 1;
            params.offset = 1;
            params.midpoint = 8192;
            break;
        // Signed planes (upscale converted planes)
        case LdpFPS8: params.shift = 7; break;
        case LdpFPS10: params.shift = 5; break;
        case LdpFPS12: params.shift = 3; break;
        case LdpFPS14: params.shift = 1; break;
        case LdpFPCount: break;
    }

    return params;
}

/*------------------------------------------------------------------------------*/

/* Callback that is invoked on each thread during upscaling. */
static bool upscaleSlicedJob(void* argument, uint32_t yStart, uint32_t count)
{
    const UpscaleSlicedJobContext* context = (const UpscaleSlicedJobContext*)argument;

    VNTraceScopedBeginArgs("task", context->diagInfo.task, "timestamp", context->diagInfo.timestamp,
                           "loq", context->diagInfo.loq, "plane", context->diagInfo.plane, "offset",
                           yStart, "count", count);

    const uint32_t yEnd = yStart + count;

    UpscaleFunction upscaleFunction = context->upscaleFunction;
    assert(upscaleFunction);

    const uint32_t width = (context->srcLayout->width >>
                            context->srcLayout->layoutInfo->planeWidthShift[context->planeIndex]) *
                           context->srcLayout->layoutInfo->interleave[context->planeIndex];
    const uint32_t height = context->srcLayout->height >>
                            context->srcLayout->layoutInfo->planeHeightShift[context->planeIndex];

    LdppDitherSlice sliceDither;
    if (context->frameDither) {
        ldppDitherSliceInitialise(&sliceDither, context->frameDither, yStart, context->planeIndex);
    }
    LdppDitherSlice* dither = context->frameDither ? &sliceDither : NULL;
    LdppUpscaleParams params =
        generateUpscaleParams(&context->kernel, context->dstLayout->layoutInfo->fixedPoint);
    params.applyPA = context->applyPA ? 1 : 0;

    upscaleFunction(context->srcPlane.firstSample, context->dstPlane.firstSample,
                    context->srcPlane.rowByteStride, context->dstPlane.rowByteStride, width, height,
                    yStart, yEnd, params, dither);

    VNTraceScopedEnd();
    return true;
}

/*! Execute a multi-threaded upscale operation. */
static bool upscaleExecute(LdcTaskPool* taskPool, LdcTask* parent, const LdppUpscaleArgs* params,
                           const LdpPipelineDiagInfo* diagInfo)
{
    assert(params->mode != Scale0D);

    UpscaleSlicedJobContext slicedJobContext = {0};

    const bool is2D = (params->mode == Scale2D);

    const LdpPictureLayoutInfo* srcLayoutInfo = params->srcLayout->layoutInfo;
    const LdpPictureLayoutInfo* dstLayoutInfo = params->dstLayout->layoutInfo;

#if VN_SDK_FEATURE(TRACING)
    slicedJobContext.diagInfo = *diagInfo;
#endif

    slicedJobContext.planeIndex = params->planeIndex;
    slicedJobContext.srcLayout = params->srcLayout;
    slicedJobContext.dstLayout = params->dstLayout;
    slicedJobContext.srcPlane = params->srcPlane;
    slicedJobContext.dstPlane = params->dstPlane;
    bool ditheringEnabled = params->frameDither != NULL;
    Interleaving interleaving = getInterleaving(srcLayoutInfo, params->planeIndex);
    slicedJobContext.is2D = is2D;

    slicedJobContext.upscaleFunction =
        getUpscaleFunction(srcLayoutInfo->fixedPoint, dstLayoutInfo->fixedPoint, interleaving, is2D,
                           params->applyPA, ditheringEnabled);

    slicedJobContext.kernel = *params->kernel;
    slicedJobContext.applyPA = params->applyPA;
    slicedJobContext.frameDither = params->frameDither;

    if (!slicedJobContext.upscaleFunction) {
        VNLogError("Failed to find upscale function");
        return false;
    }

    const uint32_t srcHeight = params->srcLayout->height >>
                               params->srcLayout->layoutInfo->planeHeightShift[params->planeIndex];
    return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &upscaleSlicedJob, NULL,
                                        &slicedJobContext, sizeof(slicedJobContext), srcHeight);
}

/*------------------------------------------------------------------------------*/

bool ldppUpscale(LdcTaskPool* taskPool, LdcTask* parent, const LdppUpscaleArgs* params,
                 const LdpPipelineDiagInfo* diagInfo)
{
    const LdpPictureLayout* srcLayout = params->srcLayout;
    const LdpPictureLayout* dstLayout = params->dstLayout;
    const LdeKernel* kernel = params->kernel;

    if (!interleavingEqual(srcLayout->layoutInfo, dstLayout->layoutInfo)) {
        VNLogError("Upscale: src and dst must be the same interleaving type");
        return false;
    }

    if (!kernel) {
        VNLogError("Upscale: kernel cannot be NULL");
        return false;
    }

    if (kernel->length & 1 || kernel->length > 8 || !kernel->length) {
        VNLogError("Upscale: kernel length must be multiple of 2 and max 8 and non-zero");
        return false;
    }

    const LdpFixedPoint srcFP = srcLayout->layoutInfo->fixedPoint;
    const LdpFixedPoint dstFP = dstLayout->layoutInfo->fixedPoint;

    if (fixedPointIsSigned(srcFP) != fixedPointIsSigned(dstFP)) {
        VNLogError("Upscale: cannot convert between signed and unsigned formats");
        return false;
    }

    if (!fixedPointIsSigned(srcFP) && (bitdepthFromFixedPoint(srcFP) > bitdepthFromFixedPoint(dstFP))) {
        VNLogError("Upscale: src bitdepth must be less than or equal to dst bitdepth - LCEVC"
                   "doesn't allow bit-depth reduction");
        return false;
    }

    return upscaleExecute(taskPool, parent, params, diagInfo);
}

/*------------------------------------------------------------------------------*/
