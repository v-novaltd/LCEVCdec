/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#include "decode/transform_coeffs.h"

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "context.h"
#include "decode/entropy.h"
#include "decode/transform_unit.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

/* @todo: These constants need tuning. */
enum CoeffsConstants
{
    CCDefaultInitialCapacity = 1024,
    CCGrowCapacityFactor = 2
};

typedef struct TransformCoeffs
{
    Memory_t memory;
    int16_t* coeffs;
    uint32_t* runs;
    uint32_t count;
    uint32_t capacity;
    bool error;
} * TransformCoeffs_t;

typedef struct DecodeChunkArgs
{
    Logger_t log;
    const Chunk_t* chunk;
    TransformCoeffs_t coeffs;
    const TUState_t* tuState;
    BlockClearJumps_t* blockClears;
    uint8_t bitstreamVersion;
    bool temporalUseReducedSignalling;
} DecodeChunkArgs_t;

typedef bool (*DecodeChunkFunction_t)(DecodeChunkArgs_t* args);

/*------------------------------------------------------------------------------*/

/*! Appends a coefficient and zero run pair. */
static inline void transformCoeffsPush(TransformCoeffs_t coeffs, int16_t coeff, uint32_t run)
{
    if (coeffs->count >= coeffs->capacity) {
        const uint32_t newCapacity = coeffs->capacity * CCGrowCapacityFactor;

        int16_t* newCoeffs = VN_REALLOC_T_ARR(coeffs->memory, coeffs->coeffs, int16_t, newCapacity);
        uint32_t* newRuns = VN_REALLOC_T_ARR(coeffs->memory, coeffs->runs, uint32_t, newCapacity);

        if (!newCoeffs || !newRuns) {
            coeffs->error = true;
            return;
        }

        coeffs->coeffs = newCoeffs;
        coeffs->runs = newRuns;
        coeffs->capacity = newCapacity;
    }

    coeffs->coeffs[coeffs->count] = coeff;
    coeffs->runs[coeffs->count] = run;
    coeffs->count++;
}

static inline void blockClearJumpPush(BlockClearJumps_t blockClears, uint32_t jump)
{
    if (blockClears->count >= blockClears->capacity) {
        const uint32_t newCapacity = blockClears->capacity * CCGrowCapacityFactor;

        uint32_t* newJumps =
            VN_REALLOC_T_ARR(blockClears->memory, blockClears->jumps, uint32_t, newCapacity);

        if (!newJumps) {
            blockClears->error = true;
            return;
        }

        blockClears->jumps = newJumps;
        blockClears->capacity = newCapacity;
    }

    blockClears->jumps[blockClears->count] = jump;
    blockClears->count++;
}

/*! Resets an instance ready to be decoded into without adjusting capacity. */
static inline void transformCoeffsReset(TransformCoeffs_t coeffs)
{
    if (coeffs) {
        coeffs->count = 0;
    }
}

/*! Decode a single layer chunk to coefficients and runs. */
static bool decodeResidualCoeffs(DecodeChunkArgs_t* args)
{
    // Context_t* ctx = args->ctx;
    const int32_t tuCount = (int32_t)args->tuState->tuTotal;
    const Chunk_t* chunk = args->chunk;

    EntropyDecoder_t decoder = {0};
    entropyInitialise(args->log, &decoder, chunk, EDTDefault, args->bitstreamVersion);

    int16_t coeff = 0;
    int32_t tuIndex = 0;

    while (tuIndex < tuCount) {
        int32_t run = entropyDecode(&decoder, &coeff);

        if (run == EntropyNoData) {
            run = tuCount - 1;
        } else if (run == -1) {
            return false;
        }

        /* 1 symbol and the zero run. */
        tuIndex += (run + 1);

        transformCoeffsPush(args->coeffs, coeff, (uint32_t)run);
    }

    return (tuIndex == tuCount) ? true : false;
}

/*! Decode a single temporal chunk to coefficients and runs. */
static bool decodeTemporalCoeffs(DecodeChunkArgs_t* args)
{
    const TUState_t* tuState = args->tuState;
    uint32_t tuIndex = 0;
    TemporalSignal_t temporalSignal = TSInter;
    uint32_t blockTUCount = 0;
    uint32_t blockWidth = 0;
    uint32_t blockHeight = 0;

    EntropyDecoder_t entropyDecoder = {0};
    if (entropyInitialise(args->log, &entropyDecoder, args->chunk, EDTTemporal,
                          args->bitstreamVersion) != 0) {
        return false;
    }

    while (tuIndex < tuState->tuTotal) {
        int32_t run = entropyDecodeTemporal(&entropyDecoder, &temporalSignal);

        /* If there's no data, we prime the temporal coeffs with a single
         * entry that indicates all the residual data is Inter */
        if (run == EntropyNoData) {
            run = (int32_t)tuState->tuTotal;
        }

        if (args->temporalUseReducedSignalling && (temporalSignal == (TemporalSignal_t)TCIntra)) {
            /* Reduced signaling has special logic for Intra signals, as
             * the run-length may be composed of an initial set of individual
             * transforms followed by a run of block clears, so we have to
             * walk through the run-length until we hit a block start,
            walk through run until a block start
               is hit, this means the whole block is intra, which we can expand for the full run. */
            uint32_t x = 0;
            uint32_t y = 0;

            /* temporal is always block raster. */
            tuCoordsBlockRaster(tuState, tuIndex, &x, &y);

            bool blockStart = (x % BSTemporal == 0) && (y % BSTemporal == 0);
            uint32_t startIndex = tuIndex;

            /* Iterate over the intra run until we hit a block start. */
            while (!blockStart && run) {
                /* @todo: Shortcut this, we could calculate how far from block start we are. */
                tuIndex++;
                run--;
                tuCoordsBlockRaster(tuState, tuIndex, &x, &y);
                blockStart = (x % BSTemporal == 0) && (y % BSTemporal == 0);
            }

            /* Write out the run of signals that must be individually set. (for min run skip) */
            const uint32_t intraRun = tuIndex - startIndex;

            if (intraRun) {
                transformCoeffsPush(args->coeffs, (int16_t)temporalSignal, intraRun - 1);
            }

            startIndex = tuIndex;

            /* Iterate over the intra block starts until the end. */
            while (run) {
                assert(blockStart);

                /* @todo: Move to bottom of loop, x / y are already populated with useful info,
                          though don't want this to go OOB.
                   @todo: Look into ways to skip this forward without doing the relatively heavy
                          raster call. */
                tuCoordsBlockRaster(tuState, tuIndex, &x, &y);

                /* Figure out how many TUs are in this block (edges have fewer),
                   and accumulate it into the intra run. */
                tuCoordsBlockDetails(tuState, x, y, &blockWidth, &blockHeight, &blockTUCount);
                run--;

                uint32_t currentIndex = tuIndex;
                if (tuState->block.tuPerBlockRowRightEdge != 0 || y >= tuState->blockAligned.maxWholeBlockY) {
                    currentIndex = tuCoordsBlockAlignedIndex(tuState, x, y);
                }
                blockClearJumpPush(*args->blockClears, currentIndex);

                tuIndex += blockTUCount;
            }

            /* The signal is intra, but it is a block clear, the distinction is useful
             * as an intra block clear can be skipped over, where as an intra signal run can't. */
            temporalSignal = (TemporalSignal_t)TCIntraBlock;
            run = (int32_t)(tuIndex - startIndex);
        } else {
            tuIndex += run;
        }

        if (run) {
            /* Finally record the temporal run.
             *
             * Noting we subtract 1 from the run-length. This is because of the difference
             * in representation for the run-lengths between both types of entropy encoded
             * data.
             *
             * For transform coefficients it is the run-length of zeros between none-zero
             * symbols, i.e. it is exclusive of the symbol location.
             *
             * For temporal coefficients it is the run-length of the current temporal signal
             * i.e. it is inclusive of the symbol "start".
             *
             * Subtracting 1 ensures the temporal run-lengths behave similar to the residual
             * coeffs run-lengths when generating command buffers. */
            transformCoeffsPush(args->coeffs, (int16_t)temporalSignal, run - 1);
        }
    }

    return (tuIndex == tuState->tuTotal) && !args->coeffs->error;
}

/*------------------------------------------------------------------------------*/

/*! Helper type for thread system. */
typedef struct DecodeJobData
{
    DecodeChunkFunction_t function;
    DecodeChunkArgs_t args;
} DecodeJobData_t;

/*! Invoke the appropriate decode function on a thread. */
int32_t decodeFunctor(void* data)
{
    DecodeJobData_t* job = (DecodeJobData_t*)data;
    return job->function(&job->args) ? 0 : -1;
}

/*------------------------------------------------------------------------------*/

bool blockClearJumpsInitialize(Memory_t memory, BlockClearJumps_t* blockClear)
{
    BlockClearJumps_t result = VN_CALLOC_T(memory, struct BlockClearJumps);

    if (!result) {
        return false;
    }

    result->memory = memory;
    result->jumps = VN_MALLOC_T_ARR(memory, uint32_t, CCDefaultInitialCapacity);

    if (!result->jumps) {
        blockClearJumpsRelease(result);
        return false;
    }

    result->capacity = CCDefaultInitialCapacity;

    *blockClear = result;
    return true;
}

void blockClearJumpsRelease(BlockClearJumps_t blockClear)
{
    if (blockClear) {
        Memory_t memory = blockClear->memory;
        VN_FREE(memory, blockClear->jumps);
        VN_FREE(memory, blockClear);
    }
}

bool transformCoeffsInitialize(Memory_t memory, TransformCoeffs_t* coeffs)
{
    TransformCoeffs_t result = VN_CALLOC_T(memory, struct TransformCoeffs);

    if (!result) {
        return false;
    }

    result->memory = memory;
    result->coeffs = VN_MALLOC_T_ARR(memory, int16_t, CCDefaultInitialCapacity);
    result->runs = VN_MALLOC_T_ARR(memory, uint32_t, CCDefaultInitialCapacity);

    if (!result->coeffs || !result->runs) {
        transformCoeffsRelease(result);
        return false;
    }

    result->capacity = CCDefaultInitialCapacity;

    *coeffs = result;
    return true;
}

void transformCoeffsRelease(TransformCoeffs_t coeffs)
{
    if (coeffs) {
        Memory_t memory = coeffs->memory;
        VN_FREE(memory, coeffs->coeffs);
        VN_FREE(memory, coeffs->runs);
        VN_FREE(memory, coeffs);
    }
}

TransformCoeffsData_t transformCoeffsGetData(TransformCoeffs_t coeffs)
{
    const TransformCoeffsData_t result = {coeffs->coeffs, coeffs->runs, coeffs->count};
    return result;
}

bool transformCoeffsDecode(TransformCoeffsDecodeArgs_t* args)
{
    /* Reset all coeffs first */
    for (int32_t i = 0; i < args->chunkCount; ++i) {
        transformCoeffsReset(args->coeffs[i]);
    }
    transformCoeffsReset(args->temporalCoeffs);

    /* Fast bypass if there's no data. */
    if (!args->chunks && !args->temporalChunk) {
        return true;
    }

    /* Build parallel jobs */
    DecodeJobData_t jobData[RCLayerMaxCount + 1] = {{0}};

    int32_t jobIndex = 0;

    if (args->chunks) {
        for (int32_t i = 0; i < args->chunkCount; ++i) {
            const Chunk_t* chunk = &args->chunks[i];

            if ((chunk == NULL) || !chunk->entropyEnabled) {
                continue;
            }

            DecodeJobData_t* job = &jobData[jobIndex];
            job->function = &decodeResidualCoeffs;

            DecodeChunkArgs_t* jobArgs = &job->args;
            jobArgs->log = args->log;
            jobArgs->chunk = chunk;
            jobArgs->coeffs = args->coeffs[i];
            jobArgs->tuState = args->tuState;
            jobArgs->bitstreamVersion = args->bitstreamVersion;
            jobIndex++;
        }
    }

    if (args->temporalChunk && args->temporalChunk->entropyEnabled) {
        DecodeJobData_t* job = &jobData[jobIndex];
        job->function = &decodeTemporalCoeffs;

        DecodeChunkArgs_t* jobArgs = &job->args;
        jobArgs->log = args->log;
        jobArgs->chunk = args->temporalChunk;
        jobArgs->coeffs = args->temporalCoeffs;
        jobArgs->tuState = args->tuState;
        jobArgs->blockClears = args->blockClears;
        jobArgs->temporalUseReducedSignalling = args->temporalUseReducedSignalling;
        jobArgs->bitstreamVersion = args->bitstreamVersion;
        jobIndex++;
    }

    /* Dispatch jobs */
    return threadingExecuteJobs(&args->threadManager, decodeFunctor, jobData, jobIndex,
                                sizeof(DecodeJobData_t));
}

/*------------------------------------------------------------------------------*/
