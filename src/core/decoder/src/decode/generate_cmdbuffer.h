/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_GENERATE_CMDBUFFER_H_
#define VN_DEC_CORE_GENERATE_CMDBUFFER_H_

#include "common/platform.h"

/*! \file
 *
 *   This module implements the functionality for taking intermediate entropy decoded
 *   coefficients/runs and building commands buffers with complete transforms worth of
 *   residual data (and temporal tile clear signals when reduced signaling is enabled).
 */

/*------------------------------------------------------------------------------*/

typedef struct Context Context_t;
typedef struct DecodeParallel* DecodeParallel_t;
typedef struct DecodeParallelArgs DecodeParallelArgs_t;
typedef struct TileState TileState_t;
typedef struct TransformCoeffs* TransformCoeffs_t;
typedef struct TUState TUState_t;

/*------------------------------------------------------------------------------*/

void generateCommandBuffers(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args,
                            TileState_t* tile, int32_t planeIndex, TransformCoeffs_t* coeffs,
                            TransformCoeffs_t temporalCoeffs, TUState_t* tuState);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_GENERATE_CMDBUFFER_H_ */