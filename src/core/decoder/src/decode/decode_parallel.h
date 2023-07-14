/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_DECODE_PARALLEL_H_
#define VN_DEC_CORE_DECODE_PARALLEL_H_

#include "common/types.h"

/*------------------------------------------------------------------------------*/

typedef struct CmdBuffer CmdBuffer_t;
typedef struct Deblock Deblock_t;
typedef struct Dequant Dequant_t;
typedef struct DeserialisedData DeserialisedData_t;
typedef struct FrameStats* FrameStats_t;
typedef struct Highlight Highlight_t;
typedef struct Memory* Memory_t;
typedef struct Surface Surface_t;

/*------------------------------------------------------------------------------*/

typedef struct DecodeParallel* DecodeParallel_t;

/*! \brief Allocates and prepares an instance of DecodeParallel_t.
 *
 *  \note Upon success the user must call `decodeParallelRelease`. */
bool decodeParallelInitialize(Memory_t memory, DecodeParallel_t* decode);

/*! \brief Releases and deallocates an instance of DecodeParallel_t. */
void decodeParallelRelease(DecodeParallel_t decode);

/*! \brief Retrieve the command buffer for a given plane and temporal signal type.
 *
 *  \note The contents of the returned CmdBuffer are only valid after a call to
 *        decodeParallel. A subsequent call to `decodeParallel` will potentially
 *        invalidate the returned CmdBuffer_t. */
CmdBuffer_t* decodeParallelGetResidualCmdBuffer(DecodeParallel_t decode, int32_t plane,
                                                TemporalSignal_t temporal, LOQIndex_t loq);

/*! \brief Retrieve the temporal tile clear command buffer for a given plane.
 *
 *  \note The contents of the returned CmdBuffer are only valid after a call to
 *        decodeParallel. A subsequent call to `decodeParallel` will potentially
 *        invalidate the returned CmdBuffer_t. */
CmdBuffer_t* decodeParallelGetTileClearCmdBuffer(DecodeParallel_t decode, int32_t plane);

/*! \brief Contains all the parameters needed to perform residual decoding. */
typedef struct DecodeParallelArgs
{
    Surface_t* dst[3]; /**< Destination surfaces for this LOQ to apply residuals to. */
    LOQIndex_t loq; /**< LOQ to apply residuals to. The destination surface dimensions must adhere to expected dimensions of the LOQ. */
    ScalingMode_t scalingMode; /**< The scaling mode used to scale to the `loq` */
    const Dequant_t* dequant;  /**< Array of dequantization parameters  */
    CPUAccelerationFeatures_t preferredAccel; /**< Preferred acceleration features to utilize, noting this is a request not a requirement. */
    FrameStats_t stats; /**< [optional] Frame stats for recording useful decoding information. */
    Deblock_t* deblock; /**< Deblocking parameters to use, only needed for LOQ-1. */
    Highlight_t* highlight; /**< [optional] Highlight state to apply, overrides residual application and writes saturated values into the destination surface. */
} DecodeParallelArgs_t;

/*! \brief Perform residual decoding using the supplied parameters.
 *
 * This will perform residual decoding across all enhancement enabled planes and all
 * tiles (if the bitstream contains tiling).
 *
 * This implementation performs some parallelism to help with decoding speed, but
 * performance can be worse depending on the operating point, content and configuration
 * used.
 *
 * This function behaves similarly to the serial decode implementation whereby it will
 * either:
 *    1. Apply residuals in-place on the destination surfaces
 *    2. Apply residuals to the internal temporal buffer(s), then apply the temporal buffers to the destination surfaces.
 *    3. Calculate command buffers such that an external user may perform residual application/updates.
 */
int32_t decodeParallel(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DECODE_PARALLEL_H_ */