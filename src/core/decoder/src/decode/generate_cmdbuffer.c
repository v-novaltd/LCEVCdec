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

#include "decode/generate_cmdbuffer.h"

#include "common/cmdbuffer.h"
#include "context.h"
#include "decode/decode_common.h"
#include "decode/decode_parallel.h"
#include "decode/deserialiser.h"
#include "decode/transform.h"
#include "decode/transform_coeffs.h"
#include "decode/transform_unit.h"

/*------------------------------------------------------------------------------*/

void generateCommandBuffers(const DeserialisedData_t* data, const DecodeParallelArgs_t* args,
                            CmdBuffer_t* cmdbuffer, int32_t planeIndex, TransformCoeffs_t* coeffs,
                            TransformCoeffs_t temporalCoeffs, const BlockClearJumps_t blockClears,
                            const TUState_t* tuState)
{
    assert(cmdbuffer);
    const int32_t numLayers = data->numLayers;
    const TransformType_t transform = transformTypeFromLayerCount(data->numLayers);
    const UserDataConfig_t* userData = &data->userData;

    int16_t values[RCLayerMaxCount] = {0};
    uint32_t layerRun[RCLayerMaxCount] = {0};
    uint32_t indices[RCLayerMaxCount] = {0};
    uint32_t tuIndex = 0;
    uint32_t lastTuIndex = 0;
    uint32_t blockClearJumpIndex = 0;
    // Coordinates are only used when temporal is enabled
    uint32_t x = 0;
    uint32_t y = 0;

    const ScalingMode_t scaling = (LOQ0 == args->loq) ? args->scalingMode : Scale2D;
    const Dequant_t* dequant = &args->dequant[planeIndex];
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
        CmdBufferCmd_t command = CBCAdd;
        if (args->loq == LOQ0 && temporal == TSIntra) {
            command = CBCSet; // TODO: implement setZero in a smart way
        }

        uint32_t currentIndex = tuIndex;
        if (data->temporalEnabled || data->tileDimensions != TDTNone) {
            tuCoordsBlockRaster(tuState, tuIndex, &x, &y);
            if (tuState->block.tuPerBlockRowRightEdge != 0 || y >= tuState->blockAligned.maxWholeBlockY) {
                currentIndex = tuCoordsBlockAlignedIndex(tuState, x, y);
            }
            if (data->temporalEnabled) {
                while (blockClearJumpIndex < blockClears->count) {
                    uint32_t nextClearTu = blockClears->jumps[blockClearJumpIndex];
                    if (nextClearTu > currentIndex) {
                        break;
                    }
                    cmdBufferAppend(cmdbuffer, CBCClear, NULL, nextClearTu - lastTuIndex);
                    lastTuIndex = nextClearTu;
                    blockClearJumpIndex++;
                }
            }
        }
        cmdBufferAppend(cmdbuffer, command, residuals, currentIndex - lastTuIndex);
        lastTuIndex = currentIndex;

        /* Move to next transform */
        tuIndex += (1 + minimumRun);

        /* Move each layer forward. */
        for (int32_t i = 0; i < numLayers; ++i) {
            layerRun[i] -= minimumRun;
        }

        temporalRun -= minimumRun;
    }
    for (; blockClearJumpIndex < blockClears->count; blockClearJumpIndex++) {
        uint32_t nextClearTu = blockClears->jumps[blockClearJumpIndex];
        cmdBufferAppend(cmdbuffer, CBCClear, NULL, nextClearTu - lastTuIndex);
        lastTuIndex = nextClearTu;
    }
}

/*------------------------------------------------------------------------------*/
