/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "surface/upscale.h"

#include "common/memory.h"
#include "context.h"
#include "surface/upscale_neon.h"
#include "surface/upscale_scalar.h"
#include "surface/upscale_sse.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

/*! \brief  Helper function used to query the horizontal function look-up tables.
 *
 * It has a fall back mechanism when SIMD is desired to provide the non-SIMD
 * function if a SIMD function does not yet exists.
 *
 * \return A valid function pointer on success, otherwise NULL. */
UpscaleHorizontal_t getHorizontalFunction(Logger_t log, FixedPoint_t srcFP, FixedPoint_t dstFP,
                                          FixedPoint_t baseFP, Interleaving_t interleaving,
                                          CPUAccelerationFeatures_t preferredAccel)
{
    VN_UNUSED(preferredAccel);

    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        VN_ERROR(log, "Invalid horizontal function request - src_fp, dst_fp is invalid\n");
        return NULL;
    }

    UpscaleHorizontal_t res = NULL;

    /* Find a SIMD functions */

#if VN_CORE_FEATURE(SSE)
    if (accelerationFeatureEnabled(preferredAccel, CAFSSE)) {
        res = upscaleGetHorizontalFunctionSSE(interleaving, srcFP, dstFP, baseFP);
    }
#endif

#if VN_CORE_FEATURE(NEON)
    if (accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        res = upscaleGetHorizontalFunctionNEON(interleaving, srcFP, dstFP, baseFP);
    }
#endif

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
UpscaleVertical_t getVerticalFunction(Logger_t log, FixedPoint_t srcFP, FixedPoint_t dstFP,
                                      CPUAccelerationFeatures_t preferredAccel, uint32_t* xStep)
{
    VN_UNUSED(preferredAccel);

    UpscaleVertical_t res = NULL;

    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        VN_ERROR(log, "Invalid vertical function request - src_fp or dst_fp is invalid\n");
        return NULL;
    }

    /* Find a SIMD function */
#if VN_CORE_FEATURE(SSE)
    if (accelerationFeatureEnabled(preferredAccel, CAFSSE)) {
        res = upscaleGetVerticalFunctionSSE(srcFP, dstFP);
        *xStep = 16;
    }
#endif

#if VN_CORE_FEATURE(NEON)
    if (accelerationFeatureEnabled(preferredAccel, CAFNEON)) {
        res = upscaleGetVerticalFunctionNEON(srcFP, dstFP);
        *xStep = 16;
    }
#endif

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
static int32_t getRequiredStrideAlignment(CPUAccelerationFeatures_t accel)
{
    return (accel == CAFNone) ? 2 : 16;
}

/*! \brief Initialises intermediate surface for 2D upscaling.
 *
 * This is performed per invocation of the `upscale` entry point to allow for
 * dynamically changing upscaling conditions.
 *
 * The allocated surface is guaranteed to have memory backing that is large enough for
 * the desired upscale operation - it will not shrink based upon this request.
 *
 * \param log      Logger
 * \param ctx      Decoder context
 * \param params   The parameters used when upscaling.
 *
 * \return 0 on success, otherwise -1. */
static int32_t internalInitialise(Memory_t memory, Logger_t log, Context_t* ctx, const UpscaleArgs_t* params)
{
    const Surface_t* dst = params->dst;
    const FixedPoint_t fp = dst->type;
    const Interleaving_t interleaving = dst->interleaving;
    const int32_t channelCount = (int32_t)interleavingGetChannelCount(interleaving);
    const uint16_t strideAlignment =
        (uint16_t)(getRequiredStrideAlignment(params->preferredAccel) * channelCount);
    const uint32_t upscaleWidth = dst->width >> 1;
    const uint32_t upscaleStride = alignU16((uint16_t)(upscaleWidth * channelCount), strideAlignment);

    /* No need to initialise intermediate surface for 1D. */
    if (params->mode == Scale1D) {
        return 0;
    }

    /* Release intermediate surface if its too small */
    if (!surfaceIsIdle(&ctx->upscaleIntermediateSurface) &&
        !surfaceCompatible(&ctx->upscaleIntermediateSurface, fp, upscaleStride, dst->height, interleaving)) {
        surfaceRelease(memory, &ctx->upscaleIntermediateSurface);
        assert(surfaceIsIdle(&ctx->upscaleIntermediateSurface));
    }

    /* Allocate intermediate upsample buffer, aligning to width of a register */
    if (surfaceIsIdle(&ctx->upscaleIntermediateSurface)) {
        if (surfaceInitialise(memory, &ctx->upscaleIntermediateSurface, fp, upscaleWidth,
                              dst->height, upscaleStride, interleaving) != 0) {
            VN_ERROR(log, "unable to allocate upsample buffer");
            return -1;
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

/*! \brief defined predicted average modes of operation. */
typedef enum PAMode
{
    PAMDisabled,
    PAM1D,
    PAM2D
} PAMode_t;

/*!
 * Helper function to determine the predicted_average_mode to apply.
 *
 * \param paEnabled   Whether predicted-average is enabled or not.
 * \param is2D        Whether predicted-average is 2D or 1D.
 *
 * \return The calculated predicted average mode. */
static inline PAMode_t getPAMode(bool paEnabled, bool is2D)
{
    if (!paEnabled) {
        return PAMDisabled;
    }

    return is2D ? PAM2D : PAM1D;
}

/*------------------------------------------------------------------------------*/

/*!
 * Helper function that performs horizontal upscaling for a given job.
 *
 * This performs upscaling down a slice of src surface, where each invocation of
 * hori_func will upscale 2 full width lines at a time, with optional predicted-average
 * and dithering applied.
 *
 * \param dither         Dithering info (null if not wanted)
 * \param horiFunction   The function callback form performing upscaling on 2 rows.
 * \param k              The kernel to upscale with.
 * \param src            The input surface to upscale from.
 * \param dst            The output surface to upscale to.
 * \param base           The base input surface to use for predicted-average
 *                           when performing 2D predicted-average.
 * \param yStart         The row to start upscaling from.
 * \param yEnd           The row to end upscaling from (exclusive).
 * \param paMode         The predicted-average mode to use.
 * \param ditherEnabled  When true dithering is applied. */
static void horizontalTask(Dither_t dither, UpscaleHorizontal_t horiFunction,
                           const Kernel_t* kernel, const Surface_t* src, const Surface_t* dst,
                           const Surface_t* base, uint32_t yStart, uint32_t yEnd, PAMode_t paMode)
{
    uint8_t* dstPtrs[2];
    const uint8_t* srcPtrs[2];
    const uint8_t* basePtrs[2] = {NULL, NULL};

    for (uint32_t y = yStart; y < yEnd; y += 2) {
        srcPtrs[0] = surfaceGetLine(src, y);
        dstPtrs[0] = surfaceGetLine(dst, y);

        /* y_end is aligned to even so can always expect there to be 2 lines available
         * except for last job which deals with the remainder */
        if (y + 1 < yEnd) {
            srcPtrs[1] = surfaceGetLine(src, y + 1);
            dstPtrs[1] = surfaceGetLine(dst, y + 1);
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
                assert(base != NULL);
                basePtrs[0] = surfaceGetLine(base, y >> 1);
                break;
            }
            case PAMDisabled:;
        }

        horiFunction(dither, srcPtrs, dstPtrs, basePtrs, base->width, 0, base->width, kernel);
    }
}

/*!
 * Helper function that performs vertical upscaling for a given job.
 *
 * This performs upscaling across a slice of src surface, where each invocation
 * of the vert_func will upscale some number of columns, determined by x_step.
 *
 * \param vertFunction The function callback for performing upscaling for a column.
 * \param k            The kernel to upscale with.
 * \param src          The input surface to upscale from.
 * \param dst          The output surface to upscale to.
 * \param yStart       The row to start upscaling from on the input surface.
 * \param yEnd         The row to end upscaling from on the input surface (exclusive).
 * \param xStep        The number of columns upscaled per invocation of vert_func. */
static void verticalTask(UpscaleVertical_t vertFunction, const Kernel_t* kernel, const Surface_t* src,
                         const Surface_t* dst, uint32_t yStart, uint32_t yEnd, uint32_t xStep)
{
    const uint32_t srcStep = xStep * fixedPointByteSize(src->type);
    const uint32_t dstStep = xStep * fixedPointByteSize(dst->type);
    const uint32_t rowCount = yEnd - yStart;

    /* Assume that src and dst interleaving is the same. */
    const uint32_t channelCount = interleavingGetChannelCount(src->interleaving);
    const uint8_t* srcPtr = src->data;
    uint8_t* dstPtr = dst->data;
    uint32_t width = src->width * channelCount;

    for (uint32_t x = 0; x < width; x += xStep, srcPtr += srcStep, dstPtr += dstStep) {
        vertFunction(srcPtr, src->stride, dstPtr, dst->stride, yStart, rowCount, src->height, kernel);
    }
}

/*------------------------------------------------------------------------------*/

/* Upscale threading shared state. */
typedef struct UpscaleSlicedJobContext
{
    Context_t* ctx;
    const Surface_t* src;
    const Surface_t* dst;
    const Surface_t* intermediate;
    UpscaleHorizontal_t lineFunction;
    UpscaleVertical_t colFunction;
    Kernel_t kernel;
    bool applyPA;
    bool applyDither;
    uint32_t colStepping;
} UpscaleSlicedJobContext_t;

/* Callback that is invoked on each thread during upscaling. */
int32_t upscaleSlicedJob(const void* submitContext, JobIndex_t index, SliceOffset_t offset)
{
    const UpscaleSlicedJobContext_t* context = (const UpscaleSlicedJobContext_t*)submitContext;
    Context_t* ctx = context->ctx;

    UpscaleHorizontal_t horiFunction = context->lineFunction;
    UpscaleVertical_t vertFunction = context->colFunction;
    const bool bothPasses = (vertFunction != NULL);
    const Surface_t* src = context->src;
    const Surface_t* dst = context->dst;
    const Surface_t* vertDst = bothPasses ? context->intermediate : dst;
    const Surface_t* horiSrc = bothPasses ? context->intermediate : src;
    const int32_t horiStart = (int32_t)(offset.offset << (bothPasses ? 1 : 0));
    const int32_t horiEnd = (int32_t)((offset.offset + offset.count) << (bothPasses ? 1 : 0));
    const PAMode_t paMode = getPAMode(context->applyPA, bothPasses);

    if (bothPasses) {
        const int32_t vertStart = (int32_t)offset.offset;
        const int32_t vertEnd = (int32_t)(offset.offset + offset.count);
        const uint32_t vertStep = context->colStepping;

        verticalTask(vertFunction, &context->kernel, src, vertDst, vertStart, vertEnd, vertStep);
    }

    horizontalTask((context->applyDither ? ctx->dither : NULL), horiFunction, &context->kernel,
                   horiSrc, dst, src, horiStart, horiEnd, paMode);

    return 0;
}

/*! Execute a multi-threaded upscale operation. */
static bool upscaleExecute(Memory_t memory, Logger_t log, Context_t* ctx,
                           const UpscaleArgs_t* params, Kernel_t kernel)
{
    assert(params->mode != Scale0D);

    if (internalInitialise(memory, log, ctx, params) < 0) {
        VN_ERROR(log, "Failed to initialise upscaler");
        return false;
    }

    const bool is2D = (params->mode == Scale2D);

    UpscaleSlicedJobContext_t slicedJobContext;

    slicedJobContext.ctx = ctx;
    slicedJobContext.src = params->src;
    slicedJobContext.dst = params->dst;
    slicedJobContext.intermediate = is2D ? &ctx->upscaleIntermediateSurface : params->src;
    slicedJobContext.lineFunction =
        getHorizontalFunction(log, slicedJobContext.intermediate->type, params->dst->type,
                              params->applyPA ? params->src->type : FPCount,
                              params->src->interleaving, params->preferredAccel);
    slicedJobContext.colFunction =
        is2D ? getVerticalFunction(log, params->src->type, slicedJobContext.intermediate->type,
                                   params->preferredAccel, &slicedJobContext.colStepping)
             : NULL;
    slicedJobContext.kernel = kernel;
    slicedJobContext.applyPA = params->applyPA;
    slicedJobContext.applyDither = params->applyDither;

    if (!slicedJobContext.lineFunction) {
        VN_ERROR(log, "Failed to find upscale horizontal function");
        return false;
    }

    if (is2D && !slicedJobContext.colFunction) {
        VN_ERROR(log, "Failed to find upscale vertical function");
        return false;
    }

    return threadingExecuteSlicedJobs(&ctx->threadManager, &upscaleSlicedJob, &slicedJobContext,
                                      params->src->height);
}

/*------------------------------------------------------------------------------*/

bool upscale(Memory_t memory, Logger_t log, Context_t* ctx, const UpscaleArgs_t* params)
{
    Kernel_t kernel;
    const Surface_t* src = params->src;
    const Surface_t* dst = params->dst;

    if (!upscaleGetKernel(log, ctx, params->type, &kernel)) {
        VN_ERROR(log, "upscale: valid kernel not found\n");
        return false;
    }

    if (src == NULL) {
        VN_ERROR(log, "upscale: src must not be null\n");
        return false;
    }

    if (dst == NULL) {
        VN_ERROR(log, "upscale: dst must not be null\n");
        return false;
    }

    if (src->interleaving != dst->interleaving) {
        VN_ERROR(log, "upscale: src and dst must be the same interleaving type\n");
        return false;
    }

    if (kernel.length & 1 || kernel.length > 8) {
        VN_ERROR(log, "upscale: kernel length must be multiple of 2 and max 8\n");
        return false;
    }

    const FixedPoint_t srcFP = src->type;
    const FixedPoint_t dstFP = dst->type;

    if (fixedPointIsSigned(srcFP) != fixedPointIsSigned(dstFP)) {
        VN_ERROR(log, "upscale: cannot convert between signed and unsigned formats\n");
        return false;
    }

    if (!fixedPointIsSigned(srcFP) && (bitdepthFromFixedPoint(srcFP) > bitdepthFromFixedPoint(dstFP))) {
        VN_ERROR(log, "upscale: src bitdepth must be less than or equal to dst bitdepth - do "
                      "not currently support demotion conversions\n");
        return false;
    }

    return upscaleExecute(memory, log, ctx, params, kernel);
}

/*------------------------------------------------------------------------------*/

typedef bool (*PABakingRecipe_t)(const Kernel_t* raw, Kernel_t* baked);

typedef struct KernelInfo
{
    Kernel_t kernel;
    PABakingRecipe_t paBakingRecipe;
} KernelInfo_t;

typedef struct KernelPtrInfo
{
    const Kernel_t* kernel;
    PABakingRecipe_t paBakingRecipe;
} KernelPtrInfo_t;

static bool preBakePA4Tap(const Kernel_t* raw, Kernel_t* baked)
{
    const int16_t d0 = raw->coeffs[0][0];
    const int16_t c0 = raw->coeffs[0][1];
    const int16_t b0 = raw->coeffs[0][2];
    const int16_t a0 = raw->coeffs[0][3];

    const int16_t d1 = raw->coeffs[1][3];
    const int16_t c1 = raw->coeffs[1][2];
    const int16_t b1 = raw->coeffs[1][1];
    const int16_t a1 = raw->coeffs[1][0];

    const int16_t halfBDDiff = (int16_t)((b0 - d0) / 2);

    if (raw->length != 4) {
        return false;
    }

    if (a0 != a1 || b0 != b1 || c0 != c1 || d0 != d1) {
        assert(false);
        return false;
    }

    baked->coeffs[0][0] = (int16_t)-halfBDDiff;
    baked->coeffs[0][1] = 16384;
    baked->coeffs[0][2] = halfBDDiff;
    baked->coeffs[0][3] = 0;

    baked->coeffs[1][0] = 0;
    baked->coeffs[1][1] = halfBDDiff;
    baked->coeffs[1][2] = 16384;
    baked->coeffs[1][3] = (int16_t)-halfBDDiff;

    baked->length = 4;
    baked->isPreBakedPA = true;

    return true;
}

static bool preBakePA2TapZeroPad(const Kernel_t* raw, Kernel_t* baked)
{
    Kernel_t padded;

    if (raw->length != 2) {
        return false;
    }

    padded.coeffs[0][0] = 0;
    padded.coeffs[0][1] = raw->coeffs[0][0];
    padded.coeffs[0][2] = raw->coeffs[0][1];
    padded.coeffs[0][3] = 0;

    padded.coeffs[1][0] = 0;
    padded.coeffs[1][1] = raw->coeffs[1][0];
    padded.coeffs[1][2] = raw->coeffs[1][1];
    padded.coeffs[1][3] = 0;

    padded.length = 4;

    return preBakePA4Tap(&padded, baked);
}

static bool preBakePAIdentity(const Kernel_t* raw, Kernel_t* baked)
{
    *baked = *raw;
    baked->isPreBakedPA = true;
    return true;
}

static bool preBakePaUnavailable(const Kernel_t* raw, Kernel_t* baked)
{
    VN_UNUSED(raw);
    VN_UNUSED(baked);
    return false;
}

static bool preBakePA(Logger_t log, KernelPtrInfo_t raw, UpscaleType_t type, Kernel_t* baked)
{
    if (!raw.kernel || !raw.paBakingRecipe) {
        return false;
    }

    if (raw.kernel->isPreBakedPA) {
        return preBakePAIdentity(raw.kernel, baked);
    }

    if (!raw.paBakingRecipe(raw.kernel, baked)) {
        VN_ERROR(log, "Failed to initialise upscale kernel. pre-baking PA into kernel failed for kernel type=%s, length=%d\n",
                 upscaleTypeToString(type), raw.kernel->length);
        return false;
    }

    return true;
}

/*------------------------------------------------------------------------------*/

/*! \brief defined kernels */
const KernelInfo_t kKernelInfos[] = {
    /* Nearest */
    {{{{16384, 0}, {0, 16384}}, 2, false}, preBakePAIdentity},

    /* Bilinear */
    {{{{12288, 4096}, {4096, 12288}}, 2, false}, preBakePA2TapZeroPad},

    /* Bicubic (a = -0.6) */
    {{{{-1382, 14285, 3942, -461}, {-461, 3942, 14285, -1382}}, 4, false}, preBakePA4Tap},

    /* ModifiedCubic */
    {{{{-2360, 15855, 4165, -1276}, {-1276, 4165, 15855, -2360}}, 4, false}, preBakePA4Tap},

    /* AdaptiveCubic */
    {{{{0}, {0}}, 0, false}, preBakePA4Tap},

    /* US_Reserved1 */
    {{{{0}, {0}}, 0, false}, preBakePaUnavailable},

    /* US_Reserved2 */
    {{{{0}, {0}}, 0, false}, preBakePaUnavailable},

    /* US_Unspecified */
    {{{{0}, {0}}, 0, false}, preBakePaUnavailable},

    /* Lanczos */
    {{{{493, -2183, 14627, 4440, -1114, 121}, {121, -1114, 4440, 14627, -2183, 493}}, 6, false},
     preBakePaUnavailable},

    /* Bicubic with prediction */
    {{{{231, -2662, 16384, 2662, -231, 0}, {0, -231, 2662, 16384, -2662, 231}}, 6, true}, preBakePAIdentity},

    /* MISHUS filter */
    {{{{-2048, 16384, 2048, 0}, {0, 2048, 16384, -2048}}, 4, true}, preBakePAIdentity},
};

/*------------------------------------------------------------------------------*/

static bool upscaleGetKernelUntransformed(Logger_t log, Context_t* ctx, UpscaleType_t type,
                                          KernelPtrInfo_t* info)
{
    switch (type) {
        case USNearest:
        case USLinear:
        case USCubic:
        case USModifiedCubic:
        case USLanczos:
        case USCubicPrediction:
        case USMishus:
            info->kernel = &kKernelInfos[type].kernel;
            info->paBakingRecipe = kKernelInfos[type].paBakingRecipe;
            return true;
        case USAdaptiveCubic:
            info->kernel = &ctx->deserialised.adaptiveUpscaleKernel;
            info->paBakingRecipe = kKernelInfos[type].paBakingRecipe;
            return true;
        default:;
    }

    VN_ERROR(log, "upscale: unknown/unsupported upsample type (%d)\n", type);
    return false;
}

bool upscaleGetKernel(Logger_t log, Context_t* ctx, UpscaleType_t type, Kernel_t* out)
{
    KernelPtrInfo_t raw;

    if (!upscaleGetKernelUntransformed(log, ctx, type, &raw)) {
        return false;
    }

    if (ctx->useApproximatePA && ctx->deserialised.usePredictedAverage) {
        return preBakePA(log, raw, type, out);
    }

    memoryCopy(out, raw.kernel, sizeof(Kernel_t));
    return true;
}

bool upscalePAIsEnabled(Logger_t log, Context_t* ctx)
{
    const DeserialisedData_t* data = &ctx->deserialised;
    Kernel_t kernel;
    bool isPreBakedPA = false;

    if (upscaleGetKernel(log, ctx, data->upscale, &kernel)) {
        isPreBakedPA = kernel.isPreBakedPA;
    }

    /* Non-standard upscalers have PA baked into their coeffs so implicitly disable
     * PA even if it is signaled. */
    return data->usePredictedAverage && !isPreBakedPA;
}

/*------------------------------------------------------------------------------*/