/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

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
    Context_t* ctx;
    const Chunk_t* chunk;
    TransformCoeffs_t coeffs;
    const TUState_t* tuState;
    CmdBuffer_t* tileClearCmdBuffer;
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
    Context_t* ctx = args->ctx;
    const int32_t tuCount = (int32_t)args->tuState->tuTotal;
    const Chunk_t* chunk = args->chunk;

    VN_PROFILE_FUNCTION();

    EntropyDecoder_t decoder = {0};
    entropyInitialise(ctx, &decoder, chunk, EDTDefault);

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

    entropyRelease(&decoder);

    VN_PROFILE_STOP();

    return (tuIndex == tuCount) ? true : false;
}

/*! Decode a single temporal chunk to coefficients and runs. */
static bool decodeTemporalCoeffs(DecodeChunkArgs_t* args)
{
    Context_t* ctx = args->ctx;
    const TUState_t* tuState = args->tuState;
    uint32_t tuIndex = 0;
    uint8_t temporalSignal = 0;
    uint32_t blockTUCount = 0;
    uint32_t blockWidth = 0;
    uint32_t blockHeight = 0;

    VN_PROFILE_FUNCTION();

    EntropyDecoder_t entropyDecoder = {0};
    if (entropyInitialise(ctx, &entropyDecoder, args->chunk, EDTTemporal) != 0) {
        return false;
    }

    while (tuIndex < tuState->tuTotal) {
        int32_t run = entropyDecodeTemporal(&entropyDecoder, &temporalSignal);

        /* If there's no data, we prime the temporal coeffs with a single
         * entry that indicates all the residual data is Inter */
        if (run == EntropyNoData) {
            run = tuState->tuTotal;
        }

        TemporalCoeff_t temporal = (TemporalCoeff_t)temporalSignal;

        if (args->temporalUseReducedSignalling && (temporal == TCIntra)) {
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
                transformCoeffsPush(args->coeffs, temporal, intraRun - 1);
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

                tuIndex += blockTUCount;

                /* Ensure we store a command for clearing block at x & y back to zero,
                 * all symbols under this are implicitly intra. */
                cmdBufferAppendCoord(args->tileClearCmdBuffer, (int16_t)x, (int16_t)y);
            }

            /* The signal is intra, but it is a block clear, the distinction is useful
             * as an intra block clear can be skipped over, where as an intra signal run can't. */
            temporal = TCIntraBlock;
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
            transformCoeffsPush(args->coeffs, (int16_t)temporal, run - 1);
        }
    }

    entropyRelease(&entropyDecoder);

    VN_PROFILE_STOP();

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
            jobArgs->ctx = args->ctx;
            jobArgs->chunk = chunk;
            jobArgs->coeffs = args->coeffs[i];
            jobArgs->tuState = args->tuState;
            jobIndex++;
        }
    }

    if (args->temporalChunk && args->temporalChunk->entropyEnabled) {
        DecodeJobData_t* job = &jobData[jobIndex];
        job->function = &decodeTemporalCoeffs;

        DecodeChunkArgs_t* jobArgs = &job->args;
        jobArgs->ctx = args->ctx;
        jobArgs->chunk = args->temporalChunk;
        jobArgs->coeffs = args->temporalCoeffs;
        jobArgs->tuState = args->tuState;
        jobArgs->tileClearCmdBuffer = args->tileClearCmdBuffer;
        jobArgs->temporalUseReducedSignalling = args->temporalUseReducedSignalling;
        jobIndex++;
    }

    /* Dispatch jobs */
    return threadingExecuteJobs(&args->ctx->threadManager, decodeFunctor, jobData, jobIndex,
                                sizeof(DecodeJobData_t));
}

/*------------------------------------------------------------------------------*/
