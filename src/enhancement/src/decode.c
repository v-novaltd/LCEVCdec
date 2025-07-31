/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/enhancement/dimensions.h>
#include <LCEVC/enhancement/transform_unit.h>
//
#include "chunk.h"
#include "config_parser_types.h"
#include "dequant.h"
#include "entropy.h"
#include "transform.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

static inline int32_t entropyDecodeAllLayers(const uint8_t numLayers, const bool decoderExists,
                                             const int32_t tuTotal,
                                             EntropyDecoder residualDecoders[RCLayerCountDDS],
                                             int32_t zerosOut[RCLayerCountDDS],
                                             int16_t coeffsOut[RCLayerCountDDS], int32_t* minZeroCountOut)
{
    int32_t coeffsNonZeroMask = 0;
    for (uint8_t layer = 0; layer < numLayers; layer++) {
        if (zerosOut[layer] > 0) {
            zerosOut[layer]--;
            coeffsOut[layer] = 0;
        } else if (decoderExists) {
            const int32_t layerZero = entropyDecode(&residualDecoders[layer], &coeffsOut[layer]);
            zerosOut[layer] = (layerZero == EntropyNoData) ? (tuTotal - 1) : layerZero;
            if (zerosOut[layer] < 0) {
                return zerosOut[layer];
            }

            /* set i-th bit if nonzero */
            coeffsNonZeroMask |= ((coeffsOut[layer] != 0) << layer);
        } else {
            /* No decoder, skip over whole surface. */
            zerosOut[layer] = tuTotal - 1;
            coeffsOut[layer] = 0;
        }

        /* Calculate lowest common zero run */
        if (*minZeroCountOut > zerosOut[layer]) {
            *minZeroCountOut = zerosOut[layer];
        }
    }
    return coeffsNonZeroMask;
}

static inline void deblockResiduals(const LdeDeblock* deblock, int16_t residuals[RCLayerCountDDS])
{
    /*
        Residual layer ordering as a grid:
            [ 0  1  4  5  ]
            [ 2  3  6  7  ]
            [ 8  9  12 13 ]
            [ 10 11 14 15 ]
    */

    /* clang-format off */
    residuals[0]  = (int16_t)((deblock->corner * (uint32_t)(residuals[0])) >> 4);  /* 0, 0 */
    residuals[1]  = (int16_t)((deblock->side   * (uint32_t)(residuals[1])) >> 4);  /* 1, 0 */
    residuals[4]  = (int16_t)((deblock->side   * (uint32_t)(residuals[4])) >> 4);  /* 2, 0 */
    residuals[5]  = (int16_t)((deblock->corner * (uint32_t)(residuals[5])) >> 4);  /* 3, 0 */
    residuals[2]  = (int16_t)((deblock->side   * (uint32_t)(residuals[2])) >> 4);  /* 0, 1 */
    residuals[7]  = (int16_t)((deblock->side   * (uint32_t)(residuals[7])) >> 4);  /* 3, 1 */
    residuals[8]  = (int16_t)((deblock->side   * (uint32_t)(residuals[8])) >> 4);  /* 0, 2 */
    residuals[13] = (int16_t)((deblock->side   * (uint32_t)(residuals[13])) >> 4); /* 3, 2 */
    residuals[10] = (int16_t)((deblock->corner * (uint32_t)(residuals[10])) >> 4); /* 0, 3 */
    residuals[11] = (int16_t)((deblock->side   * (uint32_t)(residuals[11])) >> 4); /* 1, 3 */
    residuals[14] = (int16_t)((deblock->side   * (uint32_t)(residuals[14])) >> 4); /* 2, 3 */
    residuals[15] = (int16_t)((deblock->corner * (uint32_t)(residuals[15])) >> 4); /* 3, 3 */
    /* clang-format on */
}

/*------------------------------------------------------------------------------*/

bool ldeDecodeEnhancement(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                          const LdeLOQIndex loq, const uint32_t planeIdx, const uint32_t tileIdx,
                          LdeCmdBufferCpu* cmdBufferCpu, LdeCmdBufferGpu* cmdBufferGpu,
                          LdeCmdBufferGpuBuilder* cmdBufferBuilder)
{
    bool res = true;

    if (loq > LOQ1 || planeIdx >= RCMaxPlanes || tileIdx >= globalConfig->numTiles[planeIdx][loq]) {
        VNLogError("Invalid LOQ-plane-tile: LOQ%d, plane %d, tile %d", (uint8_t)loq, planeIdx, tileIdx);
        return false;
    }
    if (!frameConfig->loqEnabled[loq] || planeIdx > globalConfig->numPlanes) {
        VNLogDebug("Nothing to decode in LOQ%d, plane %d", (uint8_t)loq, planeIdx);
        return true;
    }

    /* general */
    Dequant dequant;
    calculateDequant(&dequant, globalConfig, frameConfig, planeIdx, loq);
    const bool temporalEnabled = globalConfig->temporalEnabled;
    const uint8_t numLayers = globalConfig->numLayers;
    const bool dds = globalConfig->transform == TransformDDS;
    const uint8_t tuWidthShift = dds ? 2 : 1;     /* The width, log2, of the transform unit */
    const uint8_t residualMemSize = dds ? 32 : 8; /* Size of residuals in bytes */
    const bool temporalReducedSignalling = globalConfig->temporalReducedSignallingEnabled;
    const LdeScalingMode scaling = (LOQ0 == loq) ? globalConfig->scalingModes[LOQ0] : Scale2D;
    const bool tuRasterOrder = (!globalConfig->temporalEnabled && globalConfig->tileDimensions == TDTNone);
    const bool cpuCmdBuffers = cmdBufferCpu ? true : false;

    if (!cpuCmdBuffers && (!cmdBufferGpu || !cmdBufferBuilder)) {
        VNLogError("No CPU or GPU cmdbuffer provided to decode to");
        return false;
    }

    LdeChunk* chunks = {0};
    LdeChunk* temporalChunk = {0};
    getLayerChunks(globalConfig, frameConfig, planeIdx, loq, tileIdx, &chunks);
    if (loq == LOQ0) {
        getTemporalChunk(globalConfig, frameConfig, planeIdx, tileIdx, &temporalChunk);
    } else {
        temporalChunk = NULL;
    }

    int16_t coeffs[RCLayerCountDDS] = {0};
    int16_t residuals[RCLayerCountDDS] = {0};
    int32_t zeros[RCLayerCountDDS] = {0}; /* Current zero run in each layer */
    int32_t temporalRun = 0;              /* Current symbol run in temporal layer */
    uint32_t tuIndex = 0;
    uint32_t lastTuIndex = 0;
    TUState tuState = {0};
    uint16_t width = 0;
    uint16_t height = 0;
    ldeTileDimensionsFromConfig(globalConfig, loq, (uint16_t)planeIdx, (uint16_t)tileIdx, &width, &height);
    const bool tileHasTemporalDecode = (temporalChunk != NULL);
    const bool tileHasEntropyDecode = frameConfig->entropyEnabled;
    TemporalSignal temporal = TSInter;
    int32_t clearBlockQueue = 0;
    int32_t coeffsNonzeroMask = 0;
    bool clearBlockRemainder = false;
    uint8_t bitstreamVersion = globalConfig->bitstreamVersion;
    TransformFunction transformFn = transformGetFunction(globalConfig->transform, scaling, false);

    /* Setup decoders */
    EntropyDecoder residualDecoders[RCLayerCountDDS] = {{0}};
    EntropyDecoder temporalDecoder = {0};
    if (tileHasEntropyDecode) {
        for (uint8_t layerIdx = 0; layerIdx < numLayers; ++layerIdx) {
            VNCheckB(entropyInitialize(&residualDecoders[layerIdx], &chunks[layerIdx], EDTDefault,
                                       bitstreamVersion));
        }
    }

    if (temporalChunk) {
        VNCheckB(entropyInitialize(&temporalDecoder, temporalChunk, EDTTemporal, bitstreamVersion));
    }

    /* Setup TU state */
    uint16_t tileStartX = 0;
    uint16_t tileStartY = 0;
    if (tileIdx > 0) {
        ldeTileStartFromConfig(globalConfig, loq, planeIdx, tileIdx, &tileStartX, &tileStartY);
    }
    VNCheckB(ldeTuStateInitialize(&tuState, width, height, tileStartX, tileStartY, tuWidthShift));

    /* Break loop once tile is fully decoded. */
    while (true) {
        /* Decode bitstream and track zero runs */
        int32_t minZeroCount = INT_MAX;
        coeffsNonzeroMask = entropyDecodeAllLayers(numLayers, tileHasEntropyDecode, (int32_t)tuState.tuTotal,
                                                   residualDecoders, zeros, coeffs, &minZeroCount);

        /* Decode temporal and track temporal run */
        const bool blockStart = ldeTuIsBlockStart(&tuState, tuIndex);
        if (clearBlockQueue == 0 && tileHasTemporalDecode && temporalEnabled) {
            if (temporalRun <= 0) {
                temporalRun = entropyDecodeTemporal(&temporalDecoder, &temporal);
                clearBlockRemainder = false;

                if (temporalRun == EntropyNoData) {
                    temporalRun = (int32_t)tuState.tuTotal;
                }
                if (temporalRun <= 0) {
                    VNLogError("Invalid temporalRun value %d", temporalRun);
                    res = false;
                    break;
                }
            }
            /* Decrement run by 1 if just decoded. Temporal signal run is inclusive
             * of current symbol. RLE signals run is exclusive of current symbol.
             * All the processing assumes the run is the number after the current symbol */
            temporalRun--;

            /* Process the intra blocks when running reduced signaling. This can occur
             * at any point during a temporal run of Intra signals, so must be tracked
             * and only performed when the first Intra signal to touch a block start
             * is encountered. All subsequent Intra signals are guaranteed to be
             * block start signals so consider them here. */
            if (blockStart && temporal == TSIntra && temporalReducedSignalling) {
                /* Given that temporalRun holds the number of blocks that need clearing, set the
                 * clearBlockQueue correctly and calculate the number of TUs until that point
                 * to keep temporalRun accurate until the final clear. */
                clearBlockQueue = temporalRun + 1;
                temporalRun = 0;

                for (int32_t block = clearBlockQueue; block > 0; block--) {
                    temporalRun += (int32_t)ldeTuCoordsBlockTuCount(&tuState, tuIndex + temporalRun);
                }
            }
        }

        uint32_t blockTUCount = ldeTuCoordsBlockTuCount(&tuState, tuIndex);
        bool clearedBlock = false;

        /* Handle clearing (either clear the block, or generate a "clear" command). */
        if (blockStart && clearBlockQueue > 0) {
            uint32_t blockAlignedIndex = ldeTuIndexBlockAlignedIndex(&tuState, tuIndex);
            if (cpuCmdBuffers) {
                if (!ldeCmdBufferCpuAppend(cmdBufferCpu, CBCCClear, NULL, blockAlignedIndex - lastTuIndex)) {
                    VNLogError("Failed to append to CPU cmdbuffer, likely out of memory");
                    return false;
                }
            } else {
                if (!ldeCmdBufferGpuAppend(cmdBufferGpu, cmdBufferBuilder, CBGOClearAndSet, NULL,
                                           blockAlignedIndex, false)) {
                    VNLogError("Failed to append to GPU cmdbuffer, likely out of memory");
                    return false;
                }
            }

            lastTuIndex = blockAlignedIndex;

            clearedBlock = true;
            clearBlockQueue--;
            if (clearBlockQueue == 0) {
                clearBlockRemainder = true;
            }
        }

        /* Only actually apply if there is some meaningful data and the operation
         * will have side-effects. */
        if ((coeffsNonzeroMask != 0) || (!clearedBlock && (!temporalEnabled || temporal == TSIntra))) {
            if (coeffsNonzeroMask != 0) {
                /* Apply SW to coeffs - this is not performed in decode loop as the temporal
                 * signal residual could be zero (implied inter), however the block signal
                 * could be intra. */

                for (uint8_t layer = 0; layer < numLayers; layer++) {
                    if (coeffs[layer] > 0) {
                        coeffs[layer] =
                            (int16_t)clampS32((int32_t)coeffs[layer] * dequant.stepWidth[temporal][layer] +
                                                  dequant.offset[temporal][layer],
                                              INT16_MIN, INT16_MAX);
                    } else if (coeffs[layer] < 0) {
                        coeffs[layer] =
                            (int16_t)clampS32((int32_t)coeffs[layer] * dequant.stepWidth[temporal][layer] -
                                                  dequant.offset[temporal][layer],
                                              INT16_MIN, INT16_MAX);
                    }
                }

                /* Inverse hadamard */
                transformFn(coeffs, residuals);

                /* Apply deblocking coefficients when enabled. */
                if (LOQ1 == loq && dds && frameConfig->deblockEnabled) {
                    deblockResiduals(&globalConfig->deblock, residuals);
                }
            } else {
                memset(residuals, 0, residualMemSize);
            }

            uint32_t currentIndex = tuIndex;
            if (!tuRasterOrder) {
                currentIndex = ldeTuIndexBlockAlignedIndex(&tuState, tuIndex);
            }
            if (cpuCmdBuffers) {
                LdeCmdBufferCpuCmd command = CBCCAdd;
                if (coeffsNonzeroMask == 0 && temporal == TSIntra) {
                    command = CBCCSetZero;
                } else if (loq == LOQ0 &&
                           (temporal == TSIntra || clearBlockQueue > 0 || clearBlockRemainder)) {
                    command = CBCCSet;
                }
                if (!ldeCmdBufferCpuAppend(cmdBufferCpu, command, residuals, currentIndex - lastTuIndex)) {
                    VNLogError("Failed to append to CPU cmdbuffer, likely out of memory");
                    return false;
                }
                lastTuIndex = currentIndex;
            } else {
                LdeCmdBufferGpuOperation operation = CBGOAdd;
                if (coeffsNonzeroMask == 0 && temporal == TSIntra) {
                    operation = CBGOSetZero;
                } else if (loq == LOQ0 && temporal == TSIntra) {
                    operation = CBGOSet;
                }
                if (!ldeCmdBufferGpuAppend(cmdBufferGpu, cmdBufferBuilder, operation, residuals,
                                           currentIndex, tuRasterOrder)) {
                    VNLogError("Failed to append to GPU cmdbuffer, likely out of memory");
                    return false;
                }
            }
        }

        /* Logic for finding the next tuIndex to jump to, keeping temporalRun accurate. Note:
         * if not tileHasTemporal, that's the start of a temporal surface or LOQ1, no special
         * logic. */
        if (tileHasTemporalDecode) {
            if (clearedBlock) {
                /* After clear block has just been run, increment to the next residual or
                 * start of the next block to clear or resume the next temporal run */
                minZeroCount = minS32(minZeroCount, (int32_t)blockTUCount - 1);
                temporalRun -= minZeroCount + 1;
            } else if (clearBlockQueue > 0) {
                /* Case for an upcoming clear block or residual, whichever comes first */
                int32_t nextBlockStart;
                if (tuIndex >= tuState.block.maxWholeBlockTu) {
                    nextBlockStart =
                        blockTUCount - ((tuIndex - tuState.block.maxWholeBlockTu) % blockTUCount) - 1;
                } else {
                    nextBlockStart = (int32_t)blockTUCount -
                                     ((tuIndex % tuState.block.tuPerRow) % tuState.block.tuPerBlock) - 1;
                }
                minZeroCount = minS32(nextBlockStart, minZeroCount);
                temporalRun -= minZeroCount + 1;
            } else if (temporal == TSInter || (clearBlockRemainder && minZeroCount > temporalRun)) {
                /* Normal operation when not in a clear block, move to the next new residual or
                 * end of the temporal run */
                minZeroCount = minS32(minZeroCount, temporalRun);
                temporalRun -= minZeroCount;
            } else if (!clearBlockRemainder) {
                /* Always just increment one TU after an Intra TU */
                assert(temporal == TSIntra);
                minZeroCount = 0;
            } else {
                /* Case when applying residuals to the last block in a run of clear blocks, keep
                 * temporalRun accurate and move to the next residual */
                temporalRun -= minZeroCount;
            }
        }

        tuIndex += minZeroCount + 1;

        if (tuIndex >= tuState.tuTotal) {
            break;
        }

        if (minZeroCount > 0) {
            /* Note: if you're tempted to "optimize" this by loop-unrolling, to do 4 operations
             * at once, don't worry. Compilers are smart enough to do this already (and indeed,
             * they even turn this into SIMD code, if they have SIMD). */
            for (uint8_t layer = 0; layer < numLayers; layer++) {
                zeros[layer] -= minZeroCount;
            }
        }
    }

    if (cpuCmdBuffers && cmdBufferCpu->entryPoints) {
        ldeCmdBufferCpuSplit(cmdBufferCpu);
    }

    if (!cpuCmdBuffers) {
        ldeCmdBufferGpuBuild(cmdBufferGpu, cmdBufferBuilder, tuRasterOrder);
    }

    return res;
}

/*------------------------------------------------------------------------------*/
