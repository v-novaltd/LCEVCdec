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

#include "context.h"

#include "common/log.h"
#include "common/memory.h"
#include "common/types.h"
#include "decode/dequant.h"
#include "decode/deserialiser.h"
#include "LCEVC/PerseusDecoder.h"
#include "surface/blit.h"
#include "surface/surface.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

void highlightSetValue(Highlight_t* state, BitDepth_t depth, uint16_t value)
{
    switch (depth) {
        case Depth8: {
            value *= 255;
            state->valUnsigned = value;
            state->valSigned = fpU8ToS8((uint8_t)value);
            break;
        }
        case Depth10: {
            value *= 1023;
            state->valUnsigned = value;
            state->valSigned = fpU10ToS10(value);
            break;
        }
        case Depth12: {
            value *= 4095;
            state->valUnsigned = value;
            state->valSigned = fpU12ToS12(value);
            break;
        }
        case Depth14: {
            value *= 16383;
            state->valUnsigned = value;
            state->valSigned = fpU14ToS14(value);
            return;
        }
        default: break;
    }
}

void contextSetDepths(Context_t* ctx)
{
    /* This function sets the context state up for the appropriate depths/fixedpoint
     * types in use for each LOQ. */
    const DeserialisedData_t* data = &ctx->deserialised;

    const bool isPrecise = !ctx->useExternalSurfaces && ctx->pipelineMode == PPM_PRECISION;
    const bool loq1EnhaDepth = data->loq1UseEnhaDepth;
    const bool loq2Enabled = (data->scalingModes[LOQ1] != Scale0D);

    const BitDepth_t baseDepth = data->baseDepth;
    const FixedPoint_t baseLP = fixedPointFromBitdepth(baseDepth);
    const FixedPoint_t baseHP = fixedPointHighPrecision(baseLP);

    const BitDepth_t enhaDepth = data->enhaDepth;
    const FixedPoint_t enhaLP = fixedPointFromBitdepth(enhaDepth);
    const FixedPoint_t enhaHP = fixedPointHighPrecision(enhaLP);

    /* LOQ-2 the input bit-depth is always base depth, there are no copies here as
     * we can do conversion during the upsample. Also LOQ-2 does not have any
     * processing on itself, so input == output always. */
    if (loq2Enabled) {
        ctx->inputDepth[LOQ2] = baseDepth;
        ctx->outputDepth[LOQ2] = baseDepth;
        ctx->inputFP[LOQ2] = baseLP;
        ctx->outputFP[LOQ2] = isPrecise ? baseHP : baseLP;
    } else {
        ctx->inputDepth[LOQ2] = DepthCount;
        ctx->outputDepth[LOQ2] = DepthCount;
        ctx->inputFP[LOQ2] = FPCount;
        ctx->outputFP[LOQ2] = FPCount;
    }

    /* LOQ-1 */
    ctx->inputDepth[LOQ1] = baseDepth;
    ctx->inputFP[LOQ1] = loq2Enabled ? ctx->outputFP[LOQ2] : baseLP;

    if (loq1EnhaDepth) {
        ctx->outputDepth[LOQ1] = enhaDepth;
        ctx->outputFP[LOQ1] = isPrecise ? enhaHP : enhaLP;
    } else {
        ctx->outputDepth[LOQ1] = baseDepth;
        ctx->outputFP[LOQ1] = isPrecise ? baseHP : baseLP;
    }

    /* LOQ-0
     *     - Is always processed at enhancement depth
     *     - Only precision mode impacts input fp. */
    ctx->inputDepth[LOQ0] = enhaDepth;
    ctx->outputDepth[LOQ0] = enhaDepth;

    ctx->inputFP[LOQ0] = isPrecise ? enhaHP : enhaLP;
    ctx->outputFP[LOQ0] = enhaLP;

    /* apply_fp is a helper for some logic further down, so set it up accordingly. */
    ctx->applyFP[LOQ0] = ctx->inputFP[LOQ0];
    ctx->applyFP[LOQ1] = ctx->outputFP[LOQ1];
    ctx->applyFP[LOQ2] = ctx->outputFP[LOQ2];

    /* Update highlight values too. */
    highlightSetValue(&ctx->highlightState[LOQ0], ctx->outputDepth[LOQ0], 1);
    highlightSetValue(&ctx->highlightState[LOQ1], ctx->outputDepth[LOQ1], 0);
}

void contextPlaneSurfacesInitialise(Context_t* ctx)
{
    const int32_t kMaxPlanes = 3;

    memorySet(ctx->planes, 0, sizeof(PlaneSurfaces_t) * kMaxPlanes);

    for (int32_t planeIndex = 0; planeIndex < kMaxPlanes; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

        for (int32_t i = 0; i < 2; ++i) {
            surfaceIdle(&plane->temporalBuffer[i]);
        }

        surfaceIdle(&plane->temporalBufferU8);
        surfaceIdle(&plane->basePixels);
        surfaceIdle(&plane->basePixelsU8);

        for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
            surfaceIdle(&plane->externalSurfaces[i]);
        }

        for (int32_t i = 0; i < LOQMaxCount; ++i) {
            surfaceIdle(&plane->internalSurfaces[i]);
        }

        surfaceIdle(&plane->loq2UpsampleTarget);
    }

    surfaceIdle(&ctx->upscaleIntermediateSurface);
}

void contextPlaneSurfacesRelease(Context_t* ctx, Memory_t memory)
{
    for (int32_t planeIndex = 0; planeIndex < 3; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

        for (int32_t i = 0; i < 2; i += 1) {
            surfaceRelease(memory, &plane->temporalBuffer[i]);
        }

        surfaceRelease(memory, &plane->temporalBufferU8);
        surfaceRelease(memory, &plane->basePixels);
        surfaceRelease(memory, &plane->basePixelsU8);

        for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
            surfaceRelease(memory, &plane->externalSurfaces[i]);
        }

        for (int32_t i = 0; i < LOQMaxCount; ++i) {
            surfaceRelease(memory, &plane->internalSurfaces[i]);
        }

        surfaceRelease(memory, &plane->loq2UpsampleTarget);
    }

    surfaceRelease(memory, &ctx->upscaleIntermediateSurface);
}

static inline int32_t contextPrepareSurface(Memory_t memory, Surface_t* surf, FixedPoint_t fpType,
                                            uint32_t width, uint32_t height)
{
    if (!surfaceIsIdle(surf) && !surfaceCompatible(surf, fpType, width, height, ILNone)) {
        surfaceRelease(memory, surf);
        assert(surfaceIsIdle(surf));
    }

    if (surfaceIsIdle(surf) &&
        (surfaceInitialise(memory, surf, fpType, width, height, width, ILNone) != 0)) {
        return -1;
    }

    return 0;
}

static int32_t contextInternalSurfacesPrepare(Context_t* ctx, Memory_t memory, Logger_t log)
{
    const DeserialisedData_t* data = &ctx->deserialised;

    const int32_t loqCount = (data->scalingModes[LOQ1] == Scale0D) ? LOQEnhancedCount : LOQMaxCount;

    for (int32_t planeIndex = 0; planeIndex < 3; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

        for (int32_t loq = 0; loq < loqCount; ++loq) {
            const FixedPoint_t fpType = ctx->applyFP[loq];

            Surface_t* internalSurface = &plane->internalSurfaces[loq];
            uint32_t width = 0;
            uint32_t height = 0;
            deserialiseCalculateSurfaceProperties(data, (LOQIndex_t)loq, planeIndex, &width, &height);

            if (contextPrepareSurface(memory, internalSurface, fpType, width, height) != 0) {
                VN_ERROR(log, "unable to allocate internal buffer\n");
                return -1;
            }
        }
    }

    return 0;
}

bool contextLOQUsingInternalSurfaces(Context_t* ctx, Memory_t memory, Logger_t log, LOQIndex_t loq)
{
    /* fixedpoint_is_signed, is an equivalent to is_high_precision - essentially at that point
     * we need to perform some copy as external API only provides low_precision memory. */
    const bool res = (ctx->inputDepth[loq] != ctx->outputDepth[loq]) ||
                     fixedPointIsSigned(ctx->inputFP[loq]) || fixedPointIsSigned(ctx->outputFP[loq]);

    if (res) {
        contextInternalSurfacesPrepare(ctx, memory, log);
    }

    return res;
}

int32_t contextInternalSurfacesImageCopy(Context_t* ctx, Logger_t log, Surface_t src[3],
                                         LOQIndex_t loq, bool fromSrc)
{
    for (int32_t planeIndex = 0; planeIndex < 3; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

        if (surfaceIsIdle(&src[planeIndex])) {
            continue;
        }

        if (fromSrc) {
            if (!surfaceBlit(log, &(ctx->threadManager), ctx->cpuFeatures, &src[planeIndex],
                             &plane->internalSurfaces[loq], BMCopy)) {
                return -1;
            }
        } else {
            if (!surfaceBlit(log, &(ctx->threadManager), ctx->cpuFeatures,
                             &plane->internalSurfaces[loq], &src[planeIndex], BMCopy)) {
                return -1;
            }
        }
    }

    return 0;
}

int32_t contextTemporalConvertSurfacesPrepare(Context_t* ctx, Memory_t memory, Logger_t log)
{
    const DeserialisedData_t* data = &ctx->deserialised;
    const FixedPoint_t highPrecisionFPType = fixedPointHighPrecision(ctx->applyFP[LOQ0]);
    uint32_t width[2];
    uint32_t height[2];

    for (int32_t planeIndex = 0; planeIndex < ctx->deserialised.numPlanes; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];
        Surface_t* temporalSurface = &plane->temporalBuffer[data->fieldType];

        deserialiseCalculateSurfaceProperties(data, LOQ0, planeIndex, &width[LOQ0], &height[LOQ0]);
        deserialiseCalculateSurfaceProperties(data, LOQ1, planeIndex, &width[LOQ1], &height[LOQ1]);

        if (contextPrepareSurface(memory, temporalSurface, highPrecisionFPType, width[LOQ0],
                                  height[LOQ0]) != 0) {
            VN_ERROR(log, "unable to allocate temporal surface\n");
            return -1;
        }

        if (ctx->convertS8) {
            Surface_t* temporalSurfaceU8 = &plane->temporalBufferU8;

            if (contextPrepareSurface(memory, temporalSurfaceU8, FPU8, width[LOQ0], height[LOQ0]) != 0) {
                VN_ERROR(log, "unable to allocate temporal u8 surface\n");
                return -1;
            }
        }

        /* Reset base pixels back to 0. */
        const bool resetBasePixels =
            ctx->generateSurfaces && (!ctx->useExternalSurfaces || ctx->convertS8);
        const bool resetBasePixelsU8 = ctx->generateSurfaces && ctx->useExternalSurfaces && ctx->convertS8;
        if (resetBasePixels) {
            Surface_t* basePixels = &plane->basePixels;

            if (contextPrepareSurface(memory, basePixels, highPrecisionFPType, width[LOQ1],
                                      height[LOQ1]) != 0) {
                VN_ERROR(log, "unable to allocate base pixels surface\n");
                return -1;
            }
        }

        if (resetBasePixelsU8) {
            Surface_t* basePixelsU8 = &plane->basePixelsU8;

            if (contextPrepareSurface(memory, basePixelsU8, FPU8, width[LOQ1], height[LOQ1]) != 0) {
                VN_ERROR(log, "unable to allocate base pixels u8 surface\n");
                return -1;
            }
        }
    }

    return 0;
}

int32_t contextLOQ2TargetSurfacePrepare(Context_t* ctx, Memory_t memory, Logger_t log)
{
    /* Setup internal upsample target. This target is only used when
       scaling_modes[PSS_LOQ_1] != 0D and the user calls the `perseus_decoder_decode`
       API. */
    const FixedPoint_t fpType = ctx->inputFP[LOQ1];

    for (int32_t plane = 0; plane < 3; ++plane) {
        Surface_t* planeSurface = &ctx->planes[plane].loq2UpsampleTarget;
        uint32_t width = 0;
        uint32_t height = 0;

        deserialiseCalculateSurfaceProperties(&ctx->deserialised, LOQ1, plane, &width, &height);

        /* Re-alloc surface if the dimensions have changed. */
        if (!surfaceIsIdle(planeSurface) &&
            !surfaceCompatible(planeSurface, fpType, width, height, ILNone)) {
            surfaceRelease(memory, planeSurface);
            assert(surfaceIsIdle(planeSurface));
        }

        if (surfaceIsIdle(planeSurface) &&
            (surfaceInitialise(memory, planeSurface, fpType, width, height, width, ILNone) != 0)) {
            VN_ERROR(log, "unable to allocate loq2_target_surface");
            return -1;
        }
    }

    return 0;
}

void contextExternalSurfacesPrepare(Context_t* ctx)
{
    for (int32_t plane = 0; plane < 3; ++plane) {
        for (int32_t loq = 0; loq < LOQEnhancedCount; ++loq) {
            FixedPoint_t fpType = ctx->applyFP[loq];

            /* This is an implicit S16 representation according to the current API. */
            if (!ctx->convertS8) {
                fpType = fixedPointHighPrecision(fpType);
            }

            uint32_t width = 0;
            uint32_t height = 0;
            Surface_t* surf = &ctx->planes[plane].externalSurfaces[loq];

            deserialiseCalculateSurfaceProperties(&ctx->deserialised, (LOQIndex_t)loq, plane,
                                                  &width, &height);
            surfaceInitialiseExt2(surf, fpType, width, height, width, ILNone);
        }
    }
}

const Dequant_t* contextGetDequant(Context_t* ctx, int32_t planeIndex, LOQIndex_t loq)
{
    assert(planeIndex < RCMaxPlanes);
    assert(loq < LOQEnhancedCount);
    return &ctx->dequant.values[loq][planeIndex];
}

/*------------------------------------------------------------------------------*/
