/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "decode/generate_cmdbuffer.h"

#include "common/cmdbuffer.h"
#include "common/simd.h"
#include "context.h"
#include "decode/decode_common.h"
#include "decode/decode_parallel.h"
#include "decode/deserialiser.h"
#include "decode/transform.h"
#include "decode/transform_coeffs.h"
#include "decode/transform_unit.h"

/*------------------------------------------------------------------------------*/

/*! \brief Retrieves the correct TU coordinates function to use. */
static inline TUCoordFunction_t tuCoordsGetFunction(const DeserialisedData_t* data)
{
    return (data->temporalEnabled || (data->tileDimensions != TDTNone)) ? &tuCoordsBlockRaster
                                                                        : &tuCoordsSurfaceRaster;
}

/*! \brief Used to remap the current DDS residual layout to a scanline ordering to
 *         simplify the usage of cmdbuffers.
 *
 *  \note  This function is fully intended to be removed and is an intermediate
 *         solution as the effort to change the residual memory representation is
 *         significant. */
static inline void cmdbufferAppendDDS(CmdBuffer_t* cmdbuffer, int16_t x, int16_t y, const int16_t* values)
{
    const int16_t tmp[16] = {values[0],  values[1],  values[4],  values[5], values[2],  values[3],
                             values[6],  values[7],  values[8],  values[9], values[12], values[13],
                             values[10], values[11], values[14], values[15]};

    cmdBufferAppend(cmdbuffer, x, y, tmp);
}

/*------------------------------------------------------------------------------*/

void generateCommandBuffers(Context_t* ctx, DecodeParallel_t decode, const DecodeParallelArgs_t* args,
                            TileState_t* tile, int32_t planeIndex, TransformCoeffs_t* coeffs,
                            TransformCoeffs_t temporalCoeffs, TUState_t* tuState)
{
    VN_PROFILE_FUNCTION();

    CmdBuffer_t* cmds[TSCount] = {
        decodeParallelGetResidualCmdBuffer(decode, planeIndex, TSInter, args->loq),
        decodeParallelGetResidualCmdBuffer(decode, planeIndex, TSIntra, args->loq),
    };

    const DeserialisedData_t* data = &ctx->deserialised;
    const int32_t numLayers = data->numLayers;
    const TransformType_t transform = transformTypeFromLayerCount(data->numLayers);
    const UserDataConfig_t* userData = &data->userData;

    int16_t values[RCLayerMaxCount] = {0};
    uint32_t layerRun[RCLayerMaxCount] = {0};
    uint32_t indices[RCLayerMaxCount] = {0};
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t tuIndex = 0;

    const ScalingMode_t scaling = (LOQ0 == args->loq) ? args->scalingMode : Scale2D;
    const Dequant_t* dequant = &args->dequant[planeIndex];
    const TUCoordFunction_t tuCoordsFn = tuCoordsGetFunction(data);
    const DequantTransformFunction_t dequantTransformFn =
        dequantTransformGetFunction(transform, scaling, args->preferredAccel);

    TemporalCoeff_t temporalCoeff = TCInter;
    TemporalSignal_t temporal = TSInter;
    uint32_t temporalRun = 0;
    uint32_t temporalIndex = 0;

    TransformCoeffsData_t temporalData = transformCoeffsGetData(temporalCoeffs);
    TransformCoeffsData_t coeffData[RCLayerMaxCount];

    for (int32_t i = 0; i < numLayers; ++i) {
        coeffData[i] = transformCoeffsGetData(coeffs[i]);
    }

    while (tuIndex < tuState->tuTotal) {
        uint32_t minimumRun = UINT_MAX;

        tuCoordsFn(tuState, tuIndex, &x, &y);

        /* Run down zero runs for each layer. */
        for (int32_t i = 0; i < numLayers; ++i) {
            if (layerRun[i] > 0) {
                --layerRun[i];
                values[i] = 0;
            } else if (indices[i] < coeffData[i].count) {
                // Next coefficient for this layer is now,
                const uint32_t currentIndex = indices[i];
                values[i] = coeffData[i].coeffs[currentIndex];
                layerRun[i] = coeffData[i].runs[currentIndex];

                indices[i]++;
            } else {
                // No values in this layer, let it run to the end.
                values[i] = 0;
                layerRun[i] = tuState->tuTotal;
            }

            /* Determine how many TUs we can skip over now. */
            if (minimumRun > layerRun[i]) {
                minimumRun = layerRun[i];
            }
        }

        /* Run down zero run for temporal. */
        if (temporalRun > 0) {
            temporalRun--;
        } else if (temporalIndex < temporalData.count) {
            temporalRun = temporalData.runs[temporalIndex];
            temporalCoeff = (TemporalCoeff_t)temporalData.coeffs[temporalIndex];
            temporal = (temporalCoeff == TCInter) ? TSInter : TSIntra;
            temporalIndex++;
        } else {
            temporalCoeff = TCInter;
            temporal = TSInter;
            temporalRun = tuState->tuTotal;
        }

        /* Perform user data modification if needed. */
        stripUserData(args->loq, userData, values);

        /* Can only mass skip over inter signals as intra must write 0 into
           destination
           But we can skip over intra signals that have been block cleared too.
           hence the distinction between TemporalSignal_t and TemporalCoeff_t. */
        if (temporalCoeff == TCIntra) {
            minimumRun = 0;
        } else if (minimumRun > temporalRun) {
            minimumRun = temporalRun;
        }

        int16_t residuals[RCLayerMaxCount] = {0};

        /* Dequant and apply transform */
        dequantTransformFn(dequant, temporal, values, residuals);

        /* Deblocking for LOQ-1 with DDS */
        if ((LOQ1 == args->loq) && (transform == TransformDDS) && args->deblock && args->deblock->enabled) {
            deblockResiduals(args->deblock, residuals);
        }

        /* Record in command buffer. */
        CmdBuffer_t* dstCmdBuffer = cmds[temporal];

        if (transform == TransformDDS) {
            cmdbufferAppendDDS(dstCmdBuffer, (int16_t)x, (int16_t)y, residuals);
        } else {
            cmdBufferAppend(dstCmdBuffer, (int16_t)x, (int16_t)y, residuals);
        }

        /* Move to next transform */
        tuIndex += (1 + minimumRun);

        /* Move each layer forward. */
        for (int32_t i = 0; i < numLayers; ++i) {
            layerRun[i] -= minimumRun;
        }

        temporalRun -= minimumRun;
    }

    VN_PROFILE_STOP();
}

/*------------------------------------------------------------------------------*/