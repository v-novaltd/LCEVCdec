/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "decode/decode_parallel.h"

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "common/neon.h"
#include "common/sse.h"
#include "common/stats.h"
#include "common/threading.h"
#include "context.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_convert.h"
#include "decode/deserialiser.h"
#include "decode/entropy.h"
#include "decode/generate_cmdbuffer.h"
#include "decode/transform.h"
#include "decode/transform_unit.h"
#include "surface/blit.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/
/* Apply Command Buffers */
/*------------------------------------------------------------------------------*/

static int32_t applyCommandBuffers(Logger_t log, ThreadManager_t threadManager, TileState_t tile,
                                   const FrameStats_t frameStats,
                                   CPUAccelerationFeatures_t cpuFeatures, const Surface_t* dst,
                                   const Highlight_t* highlight, bool surfaceRasterOrder)
{
    if ((dst == NULL) || (dst->data == NULL)) {
        return -1;
    }
    int32_t res = 0;

    VN_FRAMESTATS_RECORD_START(frameStats, STApplyInterLOQ1Start);
    VN_CHECK(applyCmdBuffer(log, threadManager, &tile, dst, surfaceRasterOrder, cpuFeatures, highlight));
    VN_FRAMESTATS_RECORD_STOP(frameStats, STApplyInterLOQ1Start);

    return res;
}

/*------------------------------------------------------------------------------*/
/* Apply Command Buffers Conversion (S7 output). */
/*------------------------------------------------------------------------------*/

static void applyCommandBuffersWithConversion(Logger_t log, ThreadManager_t threadManager,
                                              TileState_t tile, const Surface_t* hpSrc,
                                              CPUAccelerationFeatures_t cpuFeatures, Surface_t* dst,
                                              const Highlight_t* highlight, bool surfaceRasterOrder)
{
    if ((dst == NULL) || (dst->data == NULL) || (hpSrc == NULL)) {
        return;
    }

    // @todo: Add stats.

    applyCmdBuffer(log, threadManager, &tile, hpSrc, surfaceRasterOrder, cpuFeatures, highlight);
    applyConvert(&tile, hpSrc, dst, !surfaceRasterOrder);
}

/*------------------------------------------------------------------------------*/
/* 1st phase tile decode (up to command buffer generation). */
/*------------------------------------------------------------------------------*/

int32_t decodeTile(const DeserialisedData_t* data, Logger_t log, ThreadManager_t threadManager,
                   DecodeParallel_t decode, const DecodeParallelArgs_t* args, TileState_t* tile,
                   CmdBuffer_t* cmdbuffer, int32_t planeIndex, bool useOldCodeLengths)
{
    const int32_t numLayers = data->numLayers;
    const uint8_t tuWidthShift = (numLayers == RCLayerCountDDS) ? 2 : 1;
    FrameStats_t frameStats = args->stats;

    TUState_t tuState = {0};
    tuStateInitialise(&tuState, tile->width, tile->height, tile->x, tile->y, tuWidthShift);
    BlockClearJumps_t blockClears = {0};
    blockClearJumpsInitialize(decode->memory, &blockClears);

    /* Entropy decode coefficients. */
    TransformCoeffsDecodeArgs_t decodeCoeffsArgs = {0};
    decodeCoeffsArgs.log = log;
    decodeCoeffsArgs.threadManager = threadManager;
    decodeCoeffsArgs.chunks = tile->chunks;
    decodeCoeffsArgs.temporalChunk = tile->temporalChunk;
    decodeCoeffsArgs.chunkCount = numLayers;
    decodeCoeffsArgs.coeffs = decode->coeffs;
    decodeCoeffsArgs.temporalCoeffs = decode->temporalCoeffs;
    decodeCoeffsArgs.useOldCodeLengths = useOldCodeLengths;
    decodeCoeffsArgs.temporalUseReducedSignalling = data->temporalUseReducedSignalling;
    decodeCoeffsArgs.tuState = &tuState;
    decodeCoeffsArgs.blockClears = &blockClears;
    VN_FRAMESTATS_RECORD_START(frameStats, STEntropyDecodeStart);
    transformCoeffsDecode(&decodeCoeffsArgs);
    VN_FRAMESTATS_RECORD_STOP(frameStats, STEntropyDecodeStop);
    /* Generate command buffers (i.e. dequant + inverse transform combined). */
    VN_FRAMESTATS_RECORD_START(frameStats, STGenerateCommandBuffersStart);
    generateCommandBuffers(data, args, cmdbuffer, planeIndex, decode->coeffs,
                           decode->temporalCoeffs, blockClears, &tuState);
    blockClearJumpsRelease(blockClears);
    VN_FRAMESTATS_RECORD_STOP(frameStats, STGenerateCommandBuffersStop);

    return 0;
}

/*------------------------------------------------------------------------------*/

bool decodeParallelInitialize(Memory_t memory, DecodeParallel_t* decodes)
{
    for (uint32_t loq = 0; loq < LOQEnhancedCount; loq++) {
        DecodeParallel_t result = VN_CALLOC_T(memory, struct DecodeParallel);

        if (!result) {
            return false;
        }

        result->memory = memory;

        for (uint32_t i = 0; i < RCLayerMaxCount; ++i) {
            if (!transformCoeffsInitialize(memory, &result->coeffs[i])) {
                decodeParallelRelease(result);
                return false;
            }
        }

        if (!transformCoeffsInitialize(memory, &result->temporalCoeffs)) {
            decodeParallelRelease(result);
            return false;
        }

        decodes[loq] = result;
    }
    return true;
}

void decodeParallelRelease(DecodeParallel_t decode)
{
    if (decode) {
        for (uint32_t i = 0; i < RCLayerMaxCount; ++i) {
            transformCoeffsRelease(decode->coeffs[i]);
        }
        transformCoeffsRelease(decode->temporalCoeffs);

        for (uint32_t planeIndex = 0; planeIndex < RCMaxPlanes; ++planeIndex) {
            for (uint32_t tileIndex = 0; tileIndex < decode->tileCache[planeIndex].tileCount; ++tileIndex) {
                cmdBufferFree(decode->tileCache[planeIndex].tiles[tileIndex].cmdBuffer);
            }
        }

        VN_FREE(decode->memory, decode);
    }
}

CmdBuffer_t* decodeParallelGetCmdBuffer(DecodeParallel_t decode, int32_t plane, uint8_t tileIdx)
{
    if (!decode) {
        return NULL;
    }

    return decode->tileCache[plane].tiles[tileIdx].cmdBuffer;
}

CmdBufferEntryPoint_t* decodeParallelGetCmdBufferEntryPoint(DecodeParallel_t decode, uint8_t planeIdx,
                                                            uint8_t tileIdx, uint16_t entryPointIndex)
{
    return &decodeParallelGetCmdBuffer(decode, planeIdx, tileIdx)->entryPoints[entryPointIndex];
}

int32_t decodeParallel(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args)
{
    DeserialisedData_t* data = args->deserialised;
    const int32_t planeCount = data->numPlanes;
    const int32_t layerCount = data->numLayers;
    const LOQIndex_t loq = args->loq;
    int32_t res = 0;

    if (loq >= LOQEnhancedCount) {
        return -1;
    }

    VN_FRAMESTATS_RECORD_START(args->stats, STDecodeStart);
    for (int32_t planeIndex = 0; planeIndex < minS32(planeCount, RCMaxPlanes); ++planeIndex) {
        const int32_t tileCount = data->tileCount[planeIndex][loq];
        const StatType_t baseStat = (args->loq == LOQ0) ? STLOQ0_LayerByteSize0 : STLOQ1_LayerByteSize0;

        if (tileDataInitialize(&decode->tileCache[planeIndex], decode->memory, data, planeIndex, loq) != 0) {
            return -1;
        }

        /* Walk over tiles accumulating decoded results into cmdbuffers for each tile. */
        for (int32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
            TileState_t* tile = &decode->tileCache[planeIndex].tiles[tileIndex];
            if (!tile->cmdBuffer) {
                cmdBufferInitialise(decode->memory, &tile->cmdBuffer, ctx->applyCmdBufferThreads);
            }
            cmdBufferReset(tile->cmdBuffer, ctx->deserialised.numLayers);

            if (args->stats) {
                if (tile->chunks) {
                    for (int32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
                        VN_FRAMESTATS_RECORD_VALUE(args->stats, (StatType_t)(baseStat + layerIndex),
                                                   tile->chunks[layerIndex].size);
                    }
                }

                if (args->loq == LOQ0 && tile->temporalChunk) {
                    VN_FRAMESTATS_RECORD_VALUE(args->stats, STLOQ0_TemporalByteSize,
                                               tile->temporalChunk->size);
                }
            }

            decodeTile(data, args->log, *(args->threadManager), decode, args, tile, tile->cmdBuffer,
                       planeIndex, args->useOldCodeLengths);
            cmdBufferSplit(tile->cmdBuffer);

            /* Apply command buffers.  */
            PlaneSurfaces_t* plane = &ctx->planes[planeIndex];
            Surface_t* cmdBufferDst = NULL;

            /* (@todo: Dst should be passed in). */
            if (ctx->generateSurfaces) {
                if (ctx->useExternalSurfaces && !ctx->convertS8) {
                    cmdBufferDst = &plane->externalSurfaces[loq];
                } else {
                    cmdBufferDst = (loq == LOQ0) ? &plane->temporalBuffer[FTTop] : &plane->basePixels;
                }
            } else if (args->applyTemporal) {
                // @todo: support interlaced.
                cmdBufferDst = &plane->temporalBuffer[FTTop];
            } else {
                /* Use the external surface stride. */
                cmdBufferDst = args->dst[planeIndex];
            }
            bool surfaceRasterOrder = !data->temporalEnabled && data->tileDimensions == TDTNone;

            /* @todo: Refactor S8 conversion (or remove it if we can find a way). */
            if (ctx->generateSurfaces && ctx->convertS8) {
                Surface_t* convertDst = NULL;

                if (ctx->useExternalSurfaces) {
                    convertDst = &plane->externalSurfaces[loq];
                } else {
                    convertDst = (loq == LOQ0) ? &plane->temporalBufferU8 : &plane->basePixelsU8;
                }

                applyCommandBuffersWithConversion(args->log, *(args->threadManager), *tile,
                                                  cmdBufferDst, ctx->cpuFeatures, convertDst,
                                                  args->highlight, surfaceRasterOrder);
            } else {
                VN_CHECK(applyCommandBuffers(args->log, *(args->threadManager), *tile, args->stats,
                                             ctx->cpuFeatures, cmdBufferDst, args->highlight,
                                             surfaceRasterOrder));
            }
        }
    }
    VN_FRAMESTATS_RECORD_STOP(args->stats, STDecodeStop);

    return res;
}

/*------------------------------------------------------------------------------*/