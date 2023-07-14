#include "decode/decode_parallel.h"

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "common/neon.h"
#include "common/sse.h"
#include "common/stats.h"
#include "context.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_convert.h"
#include "decode/deserialiser.h"
#include "decode/entropy.h"
#include "decode/generate_cmdbuffer.h"
#include "decode/transform.h"
#include "decode/transform_coeffs.h"
#include "decode/transform_unit.h"
#include "surface/blit.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/
/* Residual Decode */
/*------------------------------------------------------------------------------*/

typedef struct DecodeParallel
{
    Memory_t memory;
    TransformCoeffs_t coeffs[RCLayerMaxCount];
    TransformCoeffs_t temporalCoeffs;
    CmdBuffer_t* tileClearCmdBuffer[RCMaxPlanes];
    CmdBuffer_t* intraCmdBuffer[RCMaxPlanes];
    CmdBuffer_t* interCmdBuffer[RCMaxPlanes][LOQEnhancedCount];

}* DecodeParallel_t;

/*------------------------------------------------------------------------------*/
/* Apply Command Buffers */
/*------------------------------------------------------------------------------*/

static ApplyCmdBufferMode_t getMode(ApplyCmdBufferMode_t mode, const Highlight_t* highlight)
{
    return (highlight && highlight->enabled) ? ACBM_Highlight : mode;
}

static void applyCommandBuffers(Context_t* ctx, DecodeParallel_t decode, FrameStats_t frameStats,
                                int32_t plane, Surface_t* dst, const Highlight_t* highlight, LOQIndex_t loq)
{
    if ((dst == NULL) || (dst->data == NULL)) {
        return;
    }

    /* clang-format off */
    if (loq == LOQ0)
    {
        VN_FRAMESTATS_RECORD_FUNCTION(
            applyCmdBuffer(ctx, decode->tileClearCmdBuffer[plane], dst, ACBM_Tiles, ctx->cpuFeatures, NULL),
            frameStats, STTileClear);

        VN_FRAMESTATS_RECORD_FUNCTION(
            applyCmdBuffer(ctx, decode->interCmdBuffer[plane][LOQ0], dst,
                getMode(ACBM_Inter, highlight), ctx->cpuFeatures, highlight),
            frameStats, STApplyInterLOQ0);

        VN_FRAMESTATS_RECORD_FUNCTION(
            applyCmdBuffer(ctx, decode->intraCmdBuffer[plane], dst,
                getMode(ACBM_Intra, highlight), ctx->cpuFeatures, highlight),
            frameStats, STApplyIntra);
    }
    else
    {
        VN_FRAMESTATS_RECORD_FUNCTION(
            applyCmdBuffer(ctx, decode->interCmdBuffer[plane][LOQ1], dst,
                getMode(ACBM_Inter, highlight), ctx->cpuFeatures, highlight),
            frameStats, STApplyInterLOQ1);
    }
    /* clang-format on */
}

/*------------------------------------------------------------------------------*/
/* Apply Command Buffers Conversion (S7 output). */
/*------------------------------------------------------------------------------*/

static void applyCommandBuffersWithConversion(Context_t* ctx, DecodeParallel_t decode,
                                              int32_t plane, const Surface_t* hpSrc, Surface_t* dst,
                                              const Highlight_t* highlight, LOQIndex_t loq)
{
    if ((dst == NULL) || (dst->data == NULL) || (hpSrc == NULL)) {
        return;
    }

    // @todo: Add stats.

    if (loq == LOQ0) {
        /* Clear source and destination tiles regions */
        applyCmdBuffer(ctx, decode->tileClearCmdBuffer[plane], hpSrc, ACBM_Tiles, ctx->cpuFeatures, NULL);
        applyCmdBuffer(ctx, decode->tileClearCmdBuffer[plane], dst, ACBM_Tiles, ctx->cpuFeatures, NULL);

        /* Update source surfaces */
        applyCmdBuffer(ctx, decode->interCmdBuffer[plane][LOQ0], hpSrc,
                       getMode(ACBM_Inter, highlight), ctx->cpuFeatures, highlight);

        applyCmdBuffer(ctx, decode->intraCmdBuffer[plane], hpSrc, getMode(ACBM_Intra, highlight),
                       ctx->cpuFeatures, highlight);

        /* Now perform conversion using cmdbuffer coords to determine pixels to copy from hpSrc into
         * dst. */
        applyConvert(ctx, decode->interCmdBuffer[plane][LOQ0], hpSrc, dst, ctx->cpuFeatures);
        applyConvert(ctx, decode->intraCmdBuffer[plane], hpSrc, dst, ctx->cpuFeatures);

    } else {
        // Just apply straight to dst, needs a special function to do the job though.
        applyCmdBuffer(ctx, decode->interCmdBuffer[plane][LOQ1], hpSrc,
                       getMode(ACBM_Inter, highlight), ctx->cpuFeatures, highlight);

        applyConvert(ctx, decode->interCmdBuffer[plane][LOQ1], hpSrc, dst, ctx->cpuFeatures);
    }
}

/*------------------------------------------------------------------------------*/
/* 1st phase tile decode (up to command buffer generation). */
/*------------------------------------------------------------------------------*/

int32_t decodeTile(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args,
                   TileState_t* tile, int32_t planeIndex)
{
    const DeserialisedData_t* data = &ctx->deserialised;
    const int32_t numLayers = data->numLayers;
    const uint32_t tuSize = (numLayers == RCLayerCountDDS) ? 4 : 2;
    FrameStats_t frameStats = args->stats;

    TUState_t tuState = {0};
    tuStateInitialise(&tuState, tile, tuSize);

    /* Entropy decode coefficients. */
    TransformCoeffsDecodeArgs_t decodeCoeffsArgs = {0};
    decodeCoeffsArgs.ctx = ctx;
    decodeCoeffsArgs.chunks = tile->chunks;
    decodeCoeffsArgs.temporalChunk = tile->temporalChunk;
    decodeCoeffsArgs.chunkCount = numLayers;
    decodeCoeffsArgs.coeffs = decode->coeffs;
    decodeCoeffsArgs.temporalCoeffs = decode->temporalCoeffs;
    decodeCoeffsArgs.temporalUseReducedSignalling = data->temporalUseReducedSignalling;
    decodeCoeffsArgs.tuState = &tuState;
    decodeCoeffsArgs.tileClearCmdBuffer = decode->tileClearCmdBuffer[planeIndex];
    VN_FRAMESTATS_RECORD_FUNCTION(transformCoeffsDecode(&decodeCoeffsArgs), frameStats, STEntropyDecode);

    /* Generate command buffers (i.e. dequant + inverse transform combined). */
    VN_FRAMESTATS_RECORD_FUNCTION(generateCommandBuffers(ctx, decode, args, tile, planeIndex,
                                                         decode->coeffs, decode->temporalCoeffs, &tuState),
                                  frameStats, STGenerateCommandBuffers);

    return 0;
}

/*------------------------------------------------------------------------------*/

bool decodeParallelInitialize(Memory_t memory, DecodeParallel_t* decode)
{
    DecodeParallel_t result = VN_CALLOC_T(memory, struct DecodeParallel);

    if (!result) {
        return false;
    }

    result->memory = memory;

    for (uint32_t i = 0; i < RCLayerMaxCount; ++i) {
        if (!transformCoeffsInitialize(memory, &result->coeffs[i])) {
            goto error_exit;
        }
    }

    if (!transformCoeffsInitialize(memory, &result->temporalCoeffs)) {
        goto error_exit;
    }

    for (uint32_t i = 0; i < RCMaxPlanes; ++i) {
        if (!cmdBufferInitialise(memory, &result->tileClearCmdBuffer[i], CBTCoordinates)) {
            goto error_exit;
        }

        if (!cmdBufferInitialise(memory, &result->intraCmdBuffer[i], CBTResiduals)) {
            goto error_exit;
        }

        for (uint32_t j = 0; j < TSCount; ++j) {
            if (!cmdBufferInitialise(memory, &result->interCmdBuffer[i][j], CBTResiduals)) {
                goto error_exit;
            }
        }
    }

    *decode = result;
    return true;

error_exit:
    decodeParallelRelease(result);
    return false;
}

void decodeParallelRelease(DecodeParallel_t decode)
{
    if (decode) {
        for (uint32_t i = 0; i < RCLayerMaxCount; ++i) {
            transformCoeffsRelease(decode->coeffs[i]);
        }
        transformCoeffsRelease(decode->temporalCoeffs);

        for (uint32_t i = 0; i < RCMaxPlanes; ++i) {
            cmdBufferFree(decode->tileClearCmdBuffer[i]);
            cmdBufferFree(decode->intraCmdBuffer[i]);

            for (uint32_t j = 0; j < TSCount; ++j) {
                cmdBufferFree(decode->interCmdBuffer[i][j]);
            }
        }

        VN_FREE(decode->memory, decode);
    }
}

CmdBuffer_t* decodeParallelGetResidualCmdBuffer(DecodeParallel_t decode, int32_t plane,
                                                TemporalSignal_t temporal, LOQIndex_t loq)
{
    if (!decode) {
        return NULL;
    }

    if (temporal == TSInter) {
        return decode->interCmdBuffer[plane][loq];
    }

    if (temporal == TSIntra && loq == LOQ0) {
        return decode->intraCmdBuffer[plane];
    }

    return NULL;
}

CmdBuffer_t* decodeParallelGetTileClearCmdBuffer(DecodeParallel_t decode, int32_t plane)
{
    if (!decode) {
        return NULL;
    }

    return decode->tileClearCmdBuffer[plane];
}

int32_t decodeParallel(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args)
{
    DeserialisedData_t* data = &ctx->deserialised;
    const int32_t planeCount = data->numPlanes;
    const int32_t layerCount = data->numLayers;
    const LOQIndex_t loq = args->loq;

    if (loq >= LOQEnhancedCount) {
        return -1;
    }

    for (int32_t planeIndex = 0; planeIndex < minS32(planeCount, RCMaxPlanes); ++planeIndex) {
        const int32_t tileCount = data->tileCount[planeIndex][loq];
        const int32_t tileWidth = data->tileWidth[planeIndex];
        const int32_t tileHeight = data->tileHeight[planeIndex];
        const int32_t planeWidth = (int32_t)args->dst[planeIndex]->width;
        const int32_t planeHeight = (int32_t)args->dst[planeIndex]->height;

        if (loq == LOQ0) {
            cmdBufferReset(decode->tileClearCmdBuffer[planeIndex], 0);
            cmdBufferReset(decode->interCmdBuffer[planeIndex][LOQ0], layerCount);
            cmdBufferReset(decode->intraCmdBuffer[planeIndex], layerCount);
        } else {
            cmdBufferReset(decode->interCmdBuffer[planeIndex][LOQ1], layerCount);
        }

        statsRecordTime(args->stats, STDecodeStart);

        const StatType_t baseStat = (args->loq == LOQ0) ? STLOQ0_LayerByteSize0 : STLOQ1_LayerByteSize0;

        /* Walk over tiles accumulating decoded results into whole frame cmdbuffers. */
        for (int32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
            const int32_t tileIndexX = tileIndex % data->tilesAcross[planeIndex][loq];
            const int32_t tileIndexY = tileIndex / data->tilesAcross[planeIndex][loq];

            TileState_t tile = {0};

            tile.x = tileIndexX * tileWidth;
            tile.y = tileIndexY * tileHeight;

            tile.width = minS32(tileWidth, planeWidth - tile.x);
            tile.height = minS32(tileHeight, planeHeight - tile.y);

            if (deserialiseGetTileLayerChunks(data, planeIndex, loq, tileIndex, &tile.chunks) != 0) {
                return -1;
            }

            if (loq == LOQ0) {
                deserialiseGetTileTemporalChunk(data, planeIndex, tileIndex, &tile.temporalChunk);
            } else {
                tile.temporalChunk = NULL;
            }

            if (args->stats) {
                if (tile.chunks) {
                    for (int32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
                        statsRecordValue(args->stats, (StatType_t)(baseStat + layerIndex),
                                         tile.chunks[layerIndex].size);
                    }
                }

                if (args->loq == LOQ0 && tile.temporalChunk) {
                    statsRecordValue(args->stats, STLOQ0_TemporalByteSize, tile.temporalChunk->size);
                }
            }

            decodeTile(ctx, decode, args, &tile, planeIndex);
        }

        /* Apply command buffers.  */

        if (!ctx->generateCmdBuffers) {
            /* Skip apply if the user wants to generate command buffers, as they'll be
             * reading them and using them directly. */
            PlaneSurfaces_t* plane = &ctx->planes[planeIndex];
            Surface_t* cmdBufferDst = NULL;

            /* (@todo: Dst should be passed in). */
            if (ctx->generateSurfaces) {
                if (ctx->useExternalSurfaces && !ctx->convertS8) {
                    cmdBufferDst = &plane->externalSurfaces[loq];
                } else {
                    cmdBufferDst = (loq == LOQ0) ? &plane->temporalBuffer[FTTop] : &plane->basePixels;
                }
            } else if ((args->loq == LOQ0) && data->temporalEnabled) {
                // @todo: support interlaced.
                cmdBufferDst = &plane->temporalBuffer[FTTop];
            } else {
                /* Use the external surface stride. */
                cmdBufferDst = args->dst[planeIndex];
            }

            /* @todo: Refactor S8 conversion (or remove it if we can find a way). */
            if (ctx->generateSurfaces && ctx->convertS8) {
                Surface_t* convertDst = NULL;

                if (ctx->useExternalSurfaces) {
                    convertDst = &plane->externalSurfaces[loq];
                } else {
                    convertDst = (loq == LOQ0) ? &plane->temporalBufferU8 : &plane->basePixelsU8;
                }

                applyCommandBuffersWithConversion(ctx, decode, planeIndex, cmdBufferDst, convertDst,
                                                  args->highlight, args->loq);
            } else {
                applyCommandBuffers(ctx, decode, args->stats, planeIndex, cmdBufferDst,
                                    args->highlight, args->loq);
            }
        }

        statsRecordTime(args->stats, STDecodeStop);
    }

    return 0;
}

/*------------------------------------------------------------------------------*/