/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

#include <LCEVC/common/bitutils.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/stdlib.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/enhancement/transform_unit.h>
#include <LCEVC/enhancement/utilities.h>
//
#include "chunk.h"
#include "config_parser_types.h"
#include "decode_utilities.h"
#include "dequant.h"
#include "entropy.h"
#include "transform.h"

/*------------------------------------------------------------------------------*/

/* Decode a fresh (coefficient, zero-run) pair for each layer listed in expiredMask, storing the
 * coefficient and the layer's next event TU (the absolute TU index at which its new run expires).
 * The caller has already zeroed coeffsOut. Returns the nonzero-coefficient mask (negative on
 * error). */
static inline int32_t entropyDecodeExpiredLayers(uint32_t expiredMask, const bool decoderExists,
                                                 const int32_t tuTotal, const int32_t tuIndex,
                                                 EntropyDecoder residualDecoders[RCLayerCountDDS],
                                                 int32_t nextEventTu[RCLayerCountDDS],
                                                 int16_t coeffsOut[RCLayerCountDDS])
{
    int32_t coeffsNonZeroMask = 0;
    while (expiredMask != 0) {
        const uint32_t layer = ctz32(expiredMask);
        expiredMask &= expiredMask - 1;

        if (decoderExists) {
            int32_t layerZeros = entropyDecode(&residualDecoders[layer], &coeffsOut[layer]);
            if (layerZeros == EntropyNoData) {
                layerZeros = tuTotal - 1;
            }
            if (layerZeros < 0) {
                return layerZeros;
            }
            /* Clamp to the end of the tile: a longer run behaves identically (the layer simply
             * never expires again), and the clamp keeps tuIndex + run + 1 far from overflow. */
            nextEventTu[layer] = tuIndex + minS32(layerZeros, tuTotal - 1) + 1;

            /* set i-th bit if nonzero */
            coeffsNonZeroMask |= ((coeffsOut[layer] != 0) << layer);
        } else {
            /* No decoder, skip over whole surface. */
            nextEventTu[layer] = tuIndex + tuTotal;
        }
    }
    return coeffsNonZeroMask;
}

/* Handle the residual layers at the current TU. Layer runs are tracked as absolute "next event"
 * TU indices, with the earliest one (and the set of layers on it) cached across events. So when
 * the current TU is not a layer event (it was reached because of temporal signalling), there is
 * no per-layer work at all; coeffsOut is only valid when the returned mask is nonzero, which is
 * the only case in which the caller reads it.
 *
 * Returns the nonzero-coefficient mask (negative on error), and sets minZeroCountOut to the
 * number of all-zero TUs between this TU and the next layer event. */
static inline int32_t entropyDecodeAllLayers(const uint8_t numLayers, const bool decoderExists,
                                             const int32_t tuTotal, const int32_t tuIndex,
                                             EntropyDecoder residualDecoders[RCLayerCountDDS],
                                             int32_t nextEventTu[RCLayerCountDDS],
                                             int32_t* minNextTuInOut, uint32_t* expiredMaskInOut,
                                             int16_t coeffsOut[RCLayerCountDDS], int32_t* minZeroCountOut)
{
    int32_t coeffsNonZeroMask = 0;

    /* Temporal signalling can stop short of the next layer event, but never jump past it. */
    assert(tuIndex <= *minNextTuInOut);

    if (tuIndex == *minNextTuInOut) {
        memset(coeffsOut, 0, RCLayerCountDDS * sizeof(int16_t));
        coeffsNonZeroMask = entropyDecodeExpiredLayers(*expiredMaskInOut, decoderExists, tuTotal, tuIndex,
                                                       residualDecoders, nextEventTu, coeffsOut);
        if (coeffsNonZeroMask < 0) {
            return coeffsNonZeroMask;
        }
        findNextLayerEvent(numLayers, nextEventTu, minNextTuInOut, expiredMaskInOut);
    }

    *minZeroCountOut = *minNextTuInOut - tuIndex - 1;
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

bool ldeDecodeEnhancement(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                          const LdeLOQIndex loq, const uint32_t planeIdx, const uint32_t tileIdx,
                          LdeCmdBufferCpu* cmdBufferCpu, LdeCmdBufferGpu* cmdBufferGpu,
                          LdeCmdBufferGpuBuilder* cmdBufferBuilder)
{
    bool res = true;

    if (!globalConfig->initialized || !frameConfig->frameConfigSet) {
        VNLogError("Global or frame config not initialized");
        return false;
    }
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
    if (!calculateDequant(&dequant, globalConfig, frameConfig, planeIdx, loq)) {
        return false;
    }
    const bool temporalEnabled = globalConfig->temporalEnabled;
    const bool temporalRefresh = temporalEnabled && frameConfig->temporalRefresh;
    const uint8_t numLayers = globalConfig->numLayers;
    const bool dds = globalConfig->transform == TransformDDS;
    const uint8_t tuWidthShift = dds ? 2 : 1;     /* The width, log2, of the transform unit */
    const uint8_t residualMemSize = dds ? 32 : 8; /* Size of residuals in bytes */
    const bool temporalReducedSignalling = globalConfig->temporalReducedSignallingEnabled;
    const LdeScalingMode scaling = globalConfig->scalingModes[loq];
    const bool tuRasterOrder = (!globalConfig->temporalEnabled && globalConfig->tileDimensions == TDTNone);
    const bool cpuCmdBuffers = cmdBufferCpu ? true : false;
    const bool userDataEnabled = globalConfig->userData.enabled;

    if (!cpuCmdBuffers && (!cmdBufferGpu || !cmdBufferBuilder)) {
        VNLogError("No CPU or GPU cmdbuffer provided to decode to");
        return false;
    }

    LdeChunk* chunks = {0};
    LdeChunk* temporalChunk = {0};
    if (!getLayerChunks(globalConfig, frameConfig, planeIdx, loq, tileIdx, &chunks)) {
        VNLogError("Failed to resolve layer chunks - inconsistent chunk configuration");
        return false;
    }
    if (loq == LOQ0) {
        getTemporalChunk(globalConfig, frameConfig, planeIdx, tileIdx, &temporalChunk);
    } else {
        temporalChunk = NULL;
    }

    int16_t coeffs[RCLayerCountDDS] = {0};
    int16_t residuals[RCLayerCountDDS] = {0};
    /* Absolute TU index of each layer's next symbol; every layer starts by decoding at TU 0. */
    int32_t layerNextEventTu[RCLayerCountDDS] = {0};
    int32_t layerMinNextTu = 0; /* Earliest entry of layerNextEventTu, cached across events */
    uint32_t layerExpiredMask = (1u << globalConfig->numLayers) - 1; /* Layers at layerMinNextTu */
    int32_t temporalRun = 0; /* Current symbol run in temporal layer */
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
    TransformFunction transformFn = transformGetFunction(globalConfig->transform, scaling);

    /* Setup decoders */
    EntropyDecoder residualDecoders[RCLayerCountDDS];
    EntropyDecoder temporalDecoder;
    if (tileHasEntropyDecode) {
        memset(residualDecoders, 0, sizeof(EntropyDecoder) * numLayers);
        for (uint8_t layerIdx = 0; layerIdx < numLayers; ++layerIdx) {
            VNCheckB(entropyInitialize(&residualDecoders[layerIdx], &chunks[layerIdx], EDTDefault,
                                       bitstreamVersion));
        }
    }

    if (temporalChunk) {
        memset(&temporalDecoder, 0, sizeof(temporalDecoder));
        VNCheckB(entropyInitialize(&temporalDecoder, temporalChunk, EDTTemporal, bitstreamVersion));
    }

    /* Setup TU state */
    uint16_t tileStartX = 0;
    uint16_t tileStartY = 0;
    if (tileIdx > 0) {
        ldeTileStartFromConfig(globalConfig, loq, (uint16_t)planeIdx, (uint16_t)tileIdx,
                               &tileStartX, &tileStartY);
    }
    VNCheckB(ldeTuStateInitialize(&tuState, width, height, tileStartX, tileStartY, tuWidthShift));

    /* Break loop once tile is fully decoded. */
    while (true) {
        /* Decode bitstream and track zero runs */
        int32_t minZeroCount = 0;
        coeffsNonzeroMask =
            entropyDecodeAllLayers(numLayers, tileHasEntropyDecode, (int32_t)tuState.tuTotal,
                                   (int32_t)tuIndex, residualDecoders, layerNextEventTu,
                                   &layerMinNextTu, &layerExpiredMask, coeffs, &minZeroCount);

        if (coeffsNonzeroMask < 0) {
            VNLogError("Entropy decode failed - likely stream corruption");
            res = false;
            break;
        }

        if (userDataEnabled && loq == LOQ1) {
            applyUserData(&globalConfig->userData, coeffs);
        }

        /* Decode temporal and track temporal run */
        const bool blockStart = tileHasTemporalDecode ? ldeTuIsBlockStart(&tuState, tuIndex) : false;
        if (clearBlockQueue == 0 && tileHasTemporalDecode && temporalEnabled) {
            if (temporalRun <= 0) {
                clearBlockRemainder = false;

                /* A zero-length run is a valid transition to the next temporal state without
                 * consuming a TU. Keep decoding at this TU until a non-empty run is reached. */
                do {
                    temporalRun = entropyDecodeTemporal(&temporalDecoder, &temporal);
                    if (temporalRun == EntropyNoData || temporalRun > (int32_t)tuState.tuTotal) {
                        temporalRun = (int32_t)tuState.tuTotal;
                    }
                } while (temporalRun == 0);

                if (temporalRun < 0) {
                    VNLogError("Invalid temporalRun value %d - likely stream corruption", temporalRun);
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

        uint32_t blockTUCount = tileHasTemporalDecode ? ldeTuCoordsBlockTuCount(&tuState, tuIndex) : 0;
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
                dequantCoeffs(&dequant, temporal, numLayers, coeffs);

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
                } else if (loq == LOQ0 && (temporalRefresh || temporal == TSIntra ||
                                           clearBlockQueue > 0 || clearBlockRemainder)) {
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
                } else if (loq == LOQ0 && (temporalRefresh || temporal == TSIntra)) {
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
                int32_t nextBlockStart = (int32_t)blockTUCount - 1;
                if (tuIndex >= tuState.block.maxWholeBlockTu) {
                    nextBlockStart -= (int32_t)((tuIndex - tuState.block.maxWholeBlockTu) % blockTUCount);
                } else {
                    nextBlockStart -=
                        (int32_t)((tuIndex % tuState.block.tuPerRow) % tuState.block.tuPerBlock);
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
