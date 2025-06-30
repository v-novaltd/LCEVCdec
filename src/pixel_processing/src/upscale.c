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

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pixel_processing/upscale.h>
//
#include "fp_types.h"
#include "upscale_neon.h"
#include "upscale_scalar.h"
#include "upscale_sse.h"

#include <assert.h>
#include <LCEVC/pipeline/buffer.h>

/*------------------------------------------------------------------------------*/

/*! \brief  Helper function used to query the horizontal function look-up tables.
 *
 * It has a fall back mechanism when SIMD is desired to provide the non-SIMD
 * function if a SIMD function does not yet exists.
 *
 * \return A valid function pointer on success, otherwise NULL. */
UpscaleHorizontalFunction getHorizontalFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                                LdpFixedPoint baseFP, Interleaving interleaving,
                                                bool forceScalar)
{
    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        VNLogError("Invalid horizontal function request - src_fp, dst_fp is invalid\n");
        return NULL;
    }

    UpscaleHorizontalFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    /* Find a SIMD functions */

    if (!forceScalar && acceleration->SSE) {
        res = upscaleGetHorizontalFunctionSSE(interleaving, srcFP, dstFP, baseFP);
    }

    if (!forceScalar && acceleration->NEON) {
        assert(res == NULL);
        res = upscaleGetHorizontalFunctionNEON(interleaving, srcFP, dstFP, baseFP);
    }

    /* Find a non-SIMD function */
    if (!res) {
        res = upscaleGetHorizontalFunction(interleaving, srcFP, dstFP, baseFP);
    }

    return res;
}

/*!
 * Helper function used to query the vertical function look-up tables. It has a
 * fall back mechanism when SIMD is desired to provide the non-SIMD function if a
 * SIMD function does not yet exists.
 *
 * \return A valid function pointer on success, otherwise NULL. */
UpscaleVerticalFunction getVerticalFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                            bool forceScalar, uint32_t* xStep)
{
    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        VNLogError("Invalid vertical function request - src_fp or dst_fp is invalid\n");
        return NULL;
    }

    UpscaleVerticalFunction res = NULL;
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    /* Find a SIMD function */
    if (!forceScalar && acceleration->SSE) {
        res = upscaleGetVerticalFunctionSSE(srcFP, dstFP);
        *xStep = 16;
    }

    if (!forceScalar && acceleration->NEON) {
        assert(res == NULL);
        res = upscaleGetVerticalFunctionNEON(srcFP, dstFP);
        *xStep = 16;
    }

    /* Find a non-SIMD function */
    if (!res) {
        res = upscaleGetVerticalFunction(srcFP, dstFP);

        /* @todo: This is no longer required to be 2 as PA doesn't exist any more for
         *        vertical functions. Although it may be slower so double check. */
        *xStep = 2;
    }

    return res;
}

/*! \brief Determines the stride requirements for the intermediate upscale surface.
 *
 * Currently this is fixed between SIMD and non-SIMD where SIMD requires a stride
 * of 16 since it works on 16-pixels at a time.
 *
 * \param accel The acceleration mode.
 *
 * \return The required stride alignment. */
static int32_t getRequiredStrideAlignment(bool forceScalar)
{
    return forceScalar ? 2 : kBufferRowAlignment;
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
        case 3: return ILRGB;
        case 4: return ILRGBA;
    }

    return ILCount;
}

/*! \brief Initialises intermediate plane for 2D upscaling.
 *
 * This is performed per invocation of the `upscale` entry point to allow for
 * dynamically changing upscaling conditions.
 *
 * The allocated surface is guaranteed to have memory backing that is large enough for
 * the desired upscale operation - it will not shrink based upon this request.
 *
 * \param params   The parameters used when upscaling.
 */
static void internalInitialise(LdcMemoryAllocator* allocator, const LdppUpscaleArgs* params,
                               LdcMemoryAllocation* allocation, LdpPicturePlaneDesc* planeDesc)
{
    /* No need to initialize intermediate surface for 1D. */
    if (params->mode == Scale1D) {
        return;
    }

    const LdpPictureLayoutInfo* dstLayoutInfo = params->dstLayout->layoutInfo;
    const LdpFixedPoint fp = dstLayoutInfo->fixedPoint;
    const int32_t channelCount = dstLayoutInfo->interleave[params->planeIndex];
    const uint16_t strideAlignment =
        (uint16_t)(getRequiredStrideAlignment(params->forceScalar) * channelCount);
    const uint32_t upscaleWidth =
        params->dstLayout->width >> (1 + dstLayoutInfo->planeWidthShift[params->planeIndex]);
    const uint32_t upscaleStrideBytes =
        alignU16((uint16_t)(upscaleWidth * channelCount), strideAlignment) * fixedPointByteSize(fp);
    const uint32_t upscaleHeight =
        params->dstLayout->height >> dstLayoutInfo->planeHeightShift[params->planeIndex];
    const uint32_t upscaleSize = upscaleHeight * upscaleStrideBytes;

    if (allocation->size < upscaleSize) {
        if (allocation->ptr != NULL) {
            VNFree(allocator, allocation);
        }

        VNAllocateAlignedArray(allocator, allocation, uint8_t, kBufferRowAlignment, upscaleSize);

        planeDesc->firstSample = allocation->ptr;
        planeDesc->rowByteStride = upscaleStrideBytes;
    }
}

/*------------------------------------------------------------------------------*/

/*! \brief defined predicted average modes of operation. */
typedef enum PAMode
{
    PAMDisabled,
    PAM1D,
    PAM2D
} PAMode;

/*!
 * Helper function to determine the predicted_average_mode to apply.
 *
 * \param paEnabled   Whether predicted-average is enabled or not.
 * \param is2D        Whether predicted-average is 2D or 1D.
 *
 * \return The calculated predicted average mode. */
static inline PAMode getPAMode(bool paEnabled, bool is2D)
{
    if (!paEnabled) {
        return PAMDisabled;
    }

    return is2D ? PAM2D : PAM1D;
}

static inline uint8_t* surfaceGetLine(const LdpPicturePlaneDesc* desc, const uint32_t lineOffset)
{
    return desc->firstSample + (lineOffset * desc->rowByteStride);
}

/* Upscale threading shared state. */
typedef struct UpscaleSlicedJobContext
{
    uint32_t planeIndex;
    LdpPictureLayout* srcLayout;
    LdpPictureLayout* dstLayout;
    LdpPicturePlaneDesc srcPlane;
    LdpPicturePlaneDesc dstPlane;
    LdpPicturePlaneDesc intermediatePlane;
    UpscaleHorizontalFunction lineFunction;
    UpscaleVerticalFunction colFunction;
    LdeKernel kernel;
    bool applyPA;
    LdppDitherFrame* frameDither;
    uint32_t colStepping;

    LdcMemoryAllocator* intermediateAllocator;
    LdcMemoryAllocation intermediateAllocation;
} UpscaleSlicedJobContext;

/*------------------------------------------------------------------------------*/

/*!
 * Helper function that performs horizontal upscaling for a given job.
 *
 * This performs upscaling down a slice of src surface, where each invocation of
 * hori_func will upscale 2 full width lines at a time, with optional predicted-average
 * and dithering applied.
 *
 * \param context        Upscale context
 * \param yStart         The row to start upscaling from.
 * \param yEnd           The row to end upscaling from (exclusive).
 * \param paMode         The predicted-average mode to use. */
static void horizontalTask(const UpscaleSlicedJobContext* context, uint32_t yStart, uint32_t yEnd,
                           PAMode paMode)
{
    bool is2D = context->colFunction != NULL;
    const uint32_t baseWidth = context->srcLayout->width >>
                               context->srcLayout->layoutInfo->planeWidthShift[context->planeIndex];
    const LdpPicturePlaneDesc* horizontalInputPlane =
        is2D ? &context->intermediatePlane : &context->srcPlane;

    LdppDitherSlice sliceDither;

    uint8_t* dstPtrs[2];
    const uint8_t* srcPtrs[2];
    const uint8_t* basePtrs[2] = {NULL, NULL};

    if (context->frameDither) {
        ldppDitherSliceInitialise(&sliceDither, context->frameDither, yStart, context->planeIndex);
    }

    for (uint32_t y = yStart; y < yEnd; y += 2) {
        srcPtrs[0] = surfaceGetLine(horizontalInputPlane, y);
        dstPtrs[0] = surfaceGetLine(&context->dstPlane, y);

        /* y_end is aligned to even so can always expect there to be 2 lines available
         * except for last job which deals with the remainder */
        if (y + 1 < yEnd) {
            srcPtrs[1] = surfaceGetLine(horizontalInputPlane, y + 1);
            dstPtrs[1] = surfaceGetLine(&context->dstPlane, y + 1);
        } else {
            /* Maintain valid pointers, this will simply duplicate work on last line and
             * prevents the need for each specific implementation to have to check for
             * pointer validity. */
            srcPtrs[1] = srcPtrs[0];
            dstPtrs[1] = dstPtrs[0];
        }

        /* The presence of valid base_ptrs informs the horizontalFunction implementation
         * of what mode of PA to apply. */
        switch (paMode) {
            case PAM1D: {
                basePtrs[0] = srcPtrs[0];
                basePtrs[1] = srcPtrs[1];
                break;
            }
            case PAM2D: {
                basePtrs[0] = surfaceGetLine(&context->srcPlane, y >> 1);
                break;
            }
            case PAMDisabled:;
        }

        context->lineFunction(context->frameDither ? &sliceDither : NULL, srcPtrs, dstPtrs,
                              basePtrs, baseWidth, 0, baseWidth, &context->kernel,
                              context->dstLayout->layoutInfo->fixedPoint);
    }
}

/*!
 * Helper function that performs vertical upscaling for a given job.
 *
 * This performs upscaling across a slice of src surface, where each invocation
 * of the vert_func will upscale some number of columns, determined by x_step.
 *
 * \param context        Upscale context
 * \param yStart       The row to start upscaling from on the input surface.
 * \param yEnd         The row to end upscaling from on the input surface (exclusive).
 * \param xStep        The number of columns upscaled per invocation of vert_func. */
static void verticalTask(const UpscaleSlicedJobContext* context, uint32_t yStart, uint32_t yEnd, uint32_t xStep)
{
    UpscaleVerticalFunction vertFunction = context->colFunction;
    const LdpFixedPoint srcFP = context->srcLayout->layoutInfo->fixedPoint;
    const LdpFixedPoint dstFP = context->dstLayout->layoutInfo->fixedPoint;
    const uint32_t srcPelSize = fixedPointByteSize(srcFP);
    const uint32_t dstPelSize = fixedPointByteSize(dstFP);
    uint32_t srcStep = xStep * srcPelSize;
    uint32_t dstStep = xStep * dstPelSize;
    const uint32_t rowCount = yEnd - yStart;

    /* Assume that src and dst interleaving is the same. */
    const uint32_t channelCount = context->srcLayout->layoutInfo->interleave[context->planeIndex];
    const uint8_t* srcPtr = context->srcPlane.firstSample;
    uint8_t* dstPtr = context->intermediatePlane.firstSample;
    const uint32_t width =
        context->srcLayout->width >>
        context->srcLayout->layoutInfo->planeWidthShift[context->planeIndex] * channelCount;
    const uint32_t height =
        context->srcLayout->height >>
        context->srcLayout->layoutInfo->planeHeightShift[context->planeIndex] * channelCount;
    const uint32_t srcStride = context->srcPlane.rowByteStride / srcPelSize;
    const uint32_t dstStride = context->intermediatePlane.rowByteStride / dstPelSize;

    for (uint32_t x = 0; x < width; x += xStep) {
        /* Check if there's a potential overflow in the current step */
        if ((x + xStep) > width) {
            /* If overflow is detected, set up for scalar mode by default */
            vertFunction = upscaleGetVerticalFunction(srcFP, dstFP);
            xStep = 2;
            srcStep = xStep * srcPelSize;
            dstStep = xStep * dstPelSize;

            /* If there's only one last pixel to be upscaled due to odd width, move
            back one pixel to upscale the last pixel */
            if ((x + xStep) - width == 1) {
                srcPtr -= srcPelSize;
                dstPtr -= dstPelSize;
            }
        }
        vertFunction(srcPtr, srcStride, dstPtr, dstStride, yStart, rowCount, height, &context->kernel);
        srcPtr += srcStep;
        dstPtr += dstStep;
    }
}

/*------------------------------------------------------------------------------*/

/* Callback that is invoked on each thread during upscaling. */
static bool upscaleSlicedJob(void* argument, uint32_t offset, uint32_t count)
{
    VNTraceScopedBegin();

    const UpscaleSlicedJobContext* context = (const UpscaleSlicedJobContext*)argument;

    const bool bothPasses = (context->colFunction != NULL);
    const int32_t horiStart = (int32_t)(offset << (bothPasses ? 1 : 0));
    const int32_t horiEnd = (int32_t)((offset + count) << (bothPasses ? 1 : 0));
    const PAMode paMode = getPAMode(context->applyPA, bothPasses);

    if (bothPasses) {
        const int32_t vertStart = (int32_t)offset;
        const int32_t vertEnd = (int32_t)(offset + count);
        const uint32_t vertStep = context->colStepping;

        verticalTask(context, vertStart, vertEnd, vertStep);
    }

    horizontalTask(context, horiStart, horiEnd, paMode);

    VNTraceScopedEnd();
    return true;
}

/* Callback that is invoked once upscale is completed. */
static bool upscaleSlicedJobCompletion(void* argument, uint32_t count)
{
    VNTraceScopedBegin();

    VNUnused(count);
    UpscaleSlicedJobContext* context = (UpscaleSlicedJobContext*)argument;

    if (VNIsAllocated(context->intermediateAllocation)) {
        VNFree(context->intermediateAllocator, &context->intermediateAllocation);
    }

    VNTraceScopedEnd();
    return true;
}

/*! Execute a multi-threaded upscale operation. */
static bool upscaleExecute(LdcMemoryAllocator* allocator, LdcTaskPool* taskPool, LdcTask* parent,
                           const LdppUpscaleArgs* params, const LdeKernel* kernel)
{
    assert(params->mode != Scale0D);

    UpscaleSlicedJobContext slicedJobContext = {0};

    internalInitialise(allocator, params, &slicedJobContext.intermediateAllocation,
                       &slicedJobContext.intermediatePlane);

    const bool is2D = (params->mode == Scale2D);

    const LdpPictureLayoutInfo* srcLayoutInfo = params->srcLayout->layoutInfo;
    const LdpPictureLayoutInfo* dstLayoutInfo = params->srcLayout->layoutInfo;
    const LdpFixedPoint horizontalFPInput = is2D ? dstLayoutInfo->fixedPoint : srcLayoutInfo->fixedPoint;

    slicedJobContext.planeIndex = params->planeIndex;
    slicedJobContext.srcLayout = params->srcLayout;
    slicedJobContext.dstLayout = params->dstLayout;
    slicedJobContext.srcPlane = params->srcPlane;
    slicedJobContext.dstPlane = params->dstPlane;
    if (!is2D) {
        slicedJobContext.intermediatePlane = params->srcPlane;
    }
    slicedJobContext.lineFunction =
        getHorizontalFunction(horizontalFPInput, dstLayoutInfo->fixedPoint,
                              params->applyPA ? srcLayoutInfo->fixedPoint : LdpFPCount,
                              getInterleaving(srcLayoutInfo, params->planeIndex), params->forceScalar);
    slicedJobContext.colFunction =
        is2D ? getVerticalFunction(srcLayoutInfo->fixedPoint, dstLayoutInfo->fixedPoint,
                                   params->forceScalar, &slicedJobContext.colStepping)
             : NULL;
    slicedJobContext.kernel = *kernel;
    slicedJobContext.applyPA = params->applyPA;
    slicedJobContext.frameDither = params->frameDither;

    if (!slicedJobContext.lineFunction) {
        VNLogError("Failed to find upscale horizontal function");
        return false;
    }

    if (is2D && !slicedJobContext.colFunction) {
        VNLogError("Failed to find upscale vertical function");
        return false;
    }

    slicedJobContext.intermediateAllocator = allocator;

    const uint32_t srcHeight = params->srcLayout->height >>
                               params->srcLayout->layoutInfo->planeHeightShift[params->planeIndex];

    return ldcTaskPoolAddSlicedDeferred(taskPool, parent, &upscaleSlicedJob, &upscaleSlicedJobCompletion,
                                        &slicedJobContext, sizeof(slicedJobContext), srcHeight);
}

/*------------------------------------------------------------------------------*/

bool ldppUpscale(LdcMemoryAllocator* allocator, LdcTaskPool* taskPool, LdcTask* parent,
                 const LdeKernel* kernel, const LdppUpscaleArgs* params)
{
    const LdpPictureLayout* srcLayout = params->srcLayout;
    const LdpPictureLayout* dstLayout = params->dstLayout;

    if (!interleavingEqual(srcLayout->layoutInfo, dstLayout->layoutInfo)) {
        VNLogError("upscale: src and dst must be the same interleaving type\n");
        return false;
    }

    if (kernel->length & 1 || kernel->length > 8 || !kernel->length) {
        VNLogError("upscale: kernel length must be multiple of 2 and max 8 and non-zero\n");
        return false;
    }

    const LdpFixedPoint srcFP = srcLayout->layoutInfo->fixedPoint;
    const LdpFixedPoint dstFP = dstLayout->layoutInfo->fixedPoint;

    if (fixedPointIsSigned(srcFP) != fixedPointIsSigned(dstFP)) {
        VNLogError("upscale: cannot convert between signed and unsigned formats\n");
        return false;
    }

    if (!fixedPointIsSigned(srcFP) && (bitdepthFromFixedPoint(srcFP) > bitdepthFromFixedPoint(dstFP))) {
        VNLogError("upscale: src bitdepth must be less than or equal to dst bitdepth - do "
                   "not currently support demotion conversions\n");
        return false;
    }

    return upscaleExecute(allocator, taskPool, parent, params, kernel);
}

/*------------------------------------------------------------------------------*/
