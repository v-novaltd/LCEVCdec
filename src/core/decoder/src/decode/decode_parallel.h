/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#ifndef VN_DEC_CORE_DECODE_PARALLEL_H_
#define VN_DEC_CORE_DECODE_PARALLEL_H_

#include "common/tile.h"
#include "common/types.h"
#include "decode/transform_coeffs.h"

#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct CmdBuffer CmdBuffer_t;
typedef struct CmdBufferEntryPoint CmdBufferEntryPoint_t;
typedef struct Context Context_t;
typedef struct Deblock Deblock_t;
typedef struct Dequant Dequant_t;
typedef struct DeserialisedData DeserialisedData_t;
typedef struct Highlight Highlight_t;
typedef struct Logger* Logger_t;
typedef struct Memory* Memory_t;
typedef struct Surface Surface_t;
typedef struct ThreadManager ThreadManager_t;

/*------------------------------------------------------------------------------*/

typedef struct DecodeParallel
{
    Memory_t memory;
    TransformCoeffs_t coeffs[RCLayerMaxCount];
    TransformCoeffs_t temporalCoeffs;
    CacheTileData_t tileCache[RCMaxPlanes];
} * DecodeParallel_t;

/*! \brief Allocates and prepares an instance of DecodeParallel_t.
 *
 *  \note Upon success the user must call `decodeParallelRelease`. */
bool decodeParallelInitialize(Memory_t memory, DecodeParallel_t* decode);

/*! \brief Releases and deallocates an instance of DecodeParallel_t. */
void decodeParallelRelease(DecodeParallel_t decode);

/*! \brief Contains all the parameters needed to perform residual decoding. */
typedef struct DecodeParallelArgs
{
    DeserialisedData_t* deserialised;
    Logger_t log;
    ThreadManager_t* threadManager;
    Surface_t* dst[3]; /**< Destination surfaces for this LOQ to apply residuals to. */
    LOQIndex_t loq; /**< LOQ to apply residuals to. The destination surface dimensions must adhere to expected dimensions of the LOQ. */
    ScalingMode_t scalingMode; /**< The scaling mode used to scale to the `loq` */
    const Dequant_t* dequant;  /**< Array of dequantization parameters  */
    CPUAccelerationFeatures_t preferredAccel; /**< Preferred acceleration features to utilize, noting this is a request not a requirement. */
    Deblock_t* deblock;     /**< Deblocking parameters to use, only needed for LOQ-1. */
    Highlight_t* highlight; /**< [optional] Highlight state to apply, overrides residual application and writes saturated values into the destination surface. */
    uint8_t bitstreamVersion;
    bool applyTemporal;
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

CmdBuffer_t* decodeParallelGetCmdBuffer(DecodeParallel_t decode, int32_t plane, uint8_t tileIdx);

CmdBufferEntryPoint_t* decodeParallelGetCmdBufferEntryPoint(DecodeParallel_t decode, uint8_t planeIdx,
                                                            uint8_t tileIdx, uint16_t entryPointIndex);
/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DECODE_PARALLEL_H_ */
