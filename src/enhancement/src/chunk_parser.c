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

#include "chunk_parser.h"

#include "bitstream.h"
#include "bytestream.h"
#include "chunk.h"
#include "dequant.h"
#include "log_utilities.h"

#include <assert.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/log.h>
#include <LCEVC/enhancement/config_parser.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

static bool quantMatrixParseLOQ(ByteStream* stream, LdeLOQIndex loq, LdeFrameConfig* frameConfig,
                                const LdeGlobalConfig* globalConfig)
{
    uint8_t* values = quantMatrixGetValues(&frameConfig->quantMatrix, loq);

    for (uint8_t layer = 0; layer < globalConfig->numLayers; layer++) {
        if (!bytestreamReadU8(stream, values)) {
            return false;
        }
        values++;
    }

    return true;
}

static void quantMatrixDebugLog(const LdeQuantMatrix* quantMatrix, const LdeTransformType transform,
                                const LdeLOQIndex loq)
{
#ifdef VN_SDK_LOG_ENABLE_VERBOSE
    const uint8_t* values = quantMatrixGetValuesConst(quantMatrix, loq);

    if (transform == TransformDD) {
        VNLogVerbose("  Quant-matrix LOQ-%u: %u %u %u %u", (uint8_t)loq, values[0], values[1],
                     values[2], values[3]);
    } else if (transform == TransformDDS) {
        VNLogVerboseF("  Quant-matrix LOQ-%u: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u", loq,
                      values[0], values[1], values[2], values[3], values[4], values[5], values[6],
                      values[7], values[8], values[9], values[10], values[11], values[12],
                      values[13], values[14], values[15]);
    } else {
        VNLogVerbose("  Unknown layer count for quant-matrix");
    }
#endif
}

void calculateTileChunkIndices(LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    uint32_t offset = 0;

    memset(frameConfig->tileChunkResidualIndex, 0, sizeof(uint32_t) * RCMaxPlanes * LOQEnhancedCount);
    memset(frameConfig->tileChunkTemporalIndex, 0, sizeof(uint32_t) * RCMaxPlanes);

    for (uint8_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
        /* num_layers chunks per plane-loq-tile */
        if (frameConfig->entropyEnabled) {
            for (uint8_t loq = 0; loq < LOQEnhancedCount; ++loq) {
                const uint32_t tileCount = globalConfig->numTiles[plane][loq];
                const uint32_t chunkCount = tileCount * globalConfig->numLayers;

                frameConfig->tileChunkResidualIndex[plane][loq] = offset;
                offset += chunkCount;
            }
        }

        /* one chunk per plane-loq-tile */
        if (temporalChunkEnabled(frameConfig, globalConfig)) {
            const uint32_t chunkCount = globalConfig->numTiles[plane][LOQ0];

            frameConfig->tileChunkTemporalIndex[plane] = offset;
            offset += chunkCount;
        }
    }
}

/*-----------------------------------------------------------------------------------------------*/

static bool parseQuantMatrixLOQ0(ByteStream* stream, LdeQuantMatrixMode qmMode,
                                 LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    switch (qmMode) {
        case QMMCustomLOQ1:
        case QMMUsePrevious: {
            if (frameConfig->nalType == NTIDR || !frameConfig->quantMatrix.set) {
                VNLogVerbose(
                    "  Defaulting loq0 quant-matrix (IDR frame or quant matrix not yet set)");
                quantMatrixSetDefault(&frameConfig->quantMatrix, globalConfig->scalingModes[LOQ0],
                                      globalConfig->transform, LOQ0);
                return true;
            }
            VNLogVerbose("  Leaving loq1 quant-matrix unchanged");
            return true;
        }

        case QMMUseDefault: {
            VNLogVerbose("  Defaulting loq0 quant-matrix (signalled as default)");
            quantMatrixSetDefault(&frameConfig->quantMatrix, globalConfig->scalingModes[LOQ0],
                                  globalConfig->transform, LOQ0);
            return true;
        }

        case QMMCustomLOQ0:
        case QMMCustomBoth:
        case QMMCustomBothUnique: {
            VNLogVerbose("  Parsing custom loq0 quant-matrix");
            return quantMatrixParseLOQ(stream, LOQ0, frameConfig, globalConfig);
        }
    }

    /* qmMode not caught by switch/case, so error. */
    return false;
}

static bool parseQuantMatrixLOQ1(ByteStream* stream, LdeQuantMatrixMode qmMode,
                                 LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    switch (qmMode) {
        case QMMCustomLOQ0:
        case QMMUsePrevious: {
            if (frameConfig->nalType == NTIDR || !frameConfig->quantMatrix.set) {
                VNLogVerbose(
                    "  Defaulting loq1 quant-matrix (IDR frame or quant matrix not yet set)");
                quantMatrixSetDefault(&frameConfig->quantMatrix, globalConfig->scalingModes[LOQ0],
                                      globalConfig->transform, LOQ1);
                return true;
            }
            VNLogVerbose("  Leaving loq1 quant-matrix unchanged");
            return true;
        }

        case QMMUseDefault: {
            /* Note that the scaling mode for LOQ0 is still used for setting the default in LOQ1. */
            VNLogVerbose("  Defaulting loq1 quant-matrix (signalled as default)");
            quantMatrixSetDefault(&frameConfig->quantMatrix, globalConfig->scalingModes[LOQ0],
                                  globalConfig->transform, LOQ1);
            return true;
        }

        case QMMCustomLOQ1:
        case QMMCustomBothUnique: {
            VNLogVerbose("  Parsing custom loq1 quant-matrix");
            return quantMatrixParseLOQ(stream, LOQ1, frameConfig, globalConfig);
        }

        case QMMCustomBoth: {
            VNLogVerbose("  Copying custom loq0 quant-matrix into loq1 quant-matrix");
            const uint8_t* loq0QM = quantMatrixGetValues(&frameConfig->quantMatrix, LOQ0);
            uint8_t* loq1QM = quantMatrixGetValues(&frameConfig->quantMatrix, LOQ1);
            const uint32_t copySize = globalConfig->numLayers * sizeof(uint8_t);

            memcpy(loq1QM, loq0QM, copySize);
            return true;
        }
    }

    /* qmMode not caught by switch/case, so error. */
    return false;
}

static bool parseBlockPictureConfigQuantMatrix(ByteStream* stream, LdeQuantMatrixMode qmMode,
                                               LdeFrameConfig* frameConfig,
                                               const LdeGlobalConfig* globalConfig)
{
    VNCheckB(parseQuantMatrixLOQ0(stream, qmMode, frameConfig, globalConfig));
    VNCheckB(parseQuantMatrixLOQ1(stream, qmMode, frameConfig, globalConfig));
    frameConfig->quantMatrix.set = true;
    return true;
}

/* 7.3.6 (Table-10), everything outside the if(no_enhancement_bit_flag) test. */
static bool parseBlockPictureConfigMisc(ByteStream* stream, LdeQuantMatrixMode qmMode,
                                        uint8_t stepWidthLOQ1Enabled, uint8_t dequantOffsetEnabled,
                                        bool ditherControlPresent, LdeFrameConfig* frameConfig,
                                        const LdeGlobalConfig* globalConfig)
{
    uint8_t data;
    uint16_t data16;

    if (frameConfig->pictureType == PTField) {
        VNCheckB(bytestreamReadU8(stream, &data));

        /* field_type: 1 bit
         * reserved: 7 bits*/
        frameConfig->fieldType = (LdeFieldType)((data >> 7) & 0x01);
        VNLogVerbose("  Field type: %s", fieldTypeToString(frameConfig->fieldType));
    }

    if (stepWidthLOQ1Enabled) {
        VNCheckB(bytestreamReadU16(stream, &data16));

        /* step_width_sublayer1: 15 bits
         * level1_filtering_enabled_flag: 1 bit */
        frameConfig->stepWidths[LOQ1] = (data16 >> 1) & 0x7FFF;
        frameConfig->deblockEnabled = data16 & 0x0001;
    } else {
        frameConfig->stepWidths[LOQ1] = QMaxStepWidth;
    }
    VNLogVerbose("  Step-width LOQ-1: %d", frameConfig->stepWidths[LOQ1]);

    VNCheckB(parseBlockPictureConfigQuantMatrix(stream, qmMode, frameConfig, globalConfig));
    quantMatrixDebugLog(&frameConfig->quantMatrix, globalConfig->transform, LOQ0);
    quantMatrixDebugLog(&frameConfig->quantMatrix, globalConfig->transform, LOQ1);

    if (dequantOffsetEnabled) {
        /* dequant_offset_mode_flag: 1 bit
         * dequant_offset: 7 bits */
        VNCheckB(bytestreamReadU8(stream, &data));
        frameConfig->dequantOffsetMode = (LdeDequantOffsetMode)((data >> 7) & 0x01);
        frameConfig->dequantOffset = (data & 0x7F);
        VNLogVerbose("  Dequant offset mode: %s",
                     dequantOffsetModeToString(frameConfig->dequantOffsetMode));
        VNLogVerbose("  Dequant offset: %d", frameConfig->dequantOffset);
    } else {
        frameConfig->dequantOffset = -1;
    }

    bool ditheringEnabled = false;
    if (globalConfig->bitstreamVersion >= BitstreamVersionAlignWithSpec) {
        if (!ditherControlPresent && frameConfig->nalType == NTIDR) {
            /* As per 7.4.3.4, if the flag is absent but it's an IDR frame, then the flag is disabled */
            frameConfig->ditherEnabled = false;
        }
        ditheringEnabled = frameConfig->ditherEnabled;
    } else {
        /* Prior to BitstreamVersionAlignWithSpec, the dithering control flag was sent on EVERY
         * frame with dithering enabled (and would come with strength). */
        ditheringEnabled = ditherControlPresent && frameConfig->ditherEnabled;
    }

    if (ditheringEnabled) {
        /* Note: dithering is correctly defaulted to "disabled" by initialization. */
        VNCheckB(bytestreamReadU8(stream, &data));

        /* dithering_type: 2 bits
         * reserved_zero: 1 bit */
        frameConfig->ditherType = (LdeDitherType)((data >> 6) & 0x03);

        if (frameConfig->ditherType != 0) {
            /* dithering_strength: 5 bits */
            frameConfig->ditherStrength = (data >> 0) & 0x1F;
        }
    }

    VNLogVerbose("  Dithering type: %s", ditherTypeToString(frameConfig->ditherType));
    VNLogVerbose("  Dither strength: %u", frameConfig->ditherStrength);

    return true;
}

/* 7.3.6 (Table-10) & 7.4.3.4 */
bool parseBlockPictureConfig(ByteStream* stream, LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    uint8_t data;

    /* no_enhancement_bit_flag: 1 bit. (it's a "no enhancement" bit, so invert for "enabled"). */
    VNCheckB(bytestreamReadU8(stream, &data));
    frameConfig->entropyEnabled = (data & 0x80) ? 0 : 1;

    LdeQuantMatrixMode qmMode = QMMUsePrevious; /* Default, as per 7.4.3.4 */
    uint8_t stepWidthLOQ1Enabled = 0;
    uint8_t dequantOffsetEnabled = 0;
    bool ditherControlPresent = false;
    if (frameConfig->entropyEnabled) {
        VNLogVerbose("  Enhancement enabled");

        /* no_enhancement_bit_flag: 1 bit (already interpreted)
         * quant_matrix_mode: 3 bits */
        qmMode = (LdeQuantMatrixMode)((data >> 4) & 0x07);
        VNLogVerbose("  Quant-matrix mode: %s", quantMatrixModeToString(qmMode));

        /* dequant_offset_signalled_flag: 1 bit */
        dequantOffsetEnabled = (data >> 3) & 0x01;
        VNLogVerbose("  Dequant offset enabled: %u", dequantOffsetEnabled);

        /* picture_type_bit_flag: 1 bit */
        frameConfig->pictureType = (LdePictureType)((data >> 2) & 0x01);
        VNLogVerbose("  Picture type: %s", pictureTypeToString(frameConfig->pictureType));

        /* temporal_refresh: 1 bit */
        frameConfig->temporalRefresh = (data >> 1) & 0x01;
        VNLogVerbose("  Temporal refresh: %u", frameConfig->temporalRefresh);

        /* temporal_signalling_present_bit is inferred, rather than read, if !entropyEnabled */
        frameConfig->temporalSignallingPresent =
            globalConfig->temporalEnabled && !frameConfig->temporalRefresh;
        VNLogVerbose("  Temporal chunk enabled: %u", frameConfig->temporalSignallingPresent);

        /* step_width_sublayer1_enabled_flag: 1 bit */
        stepWidthLOQ1Enabled = (data >> 0) & 0x01;
        VNLogVerbose("  Step-width LOQ-1 enabled: %u", stepWidthLOQ1Enabled);

        uint16_t data16 = 0;
        VNCheckB(bytestreamReadU16(stream, &data16));

        /* step_width_sublayer2: 15 bits */
        frameConfig->stepWidths[LOQ0] = (data16 >> 1) & 0x7FFF;
        VNLogVerbose("  Step-width LOQ-0: %u", frameConfig->stepWidths[LOQ0]);

        /* dithering_control_flag: 1 bit */
        ditherControlPresent = true;
        frameConfig->ditherEnabled = (data16 >> 0) & 0x01;
        VNLogVerbose("  Dither control: %u", frameConfig->ditherEnabled);
    } else {
        /* no_enhancement_bit_flag: 1 bit (already interpreted)
         * reserved: 4 bits */
        VNLogVerbose("  Enhancement disabled");

        /* picture_type_bit_flag: 1 bit */
        frameConfig->pictureType = (LdePictureType)((data >> 2) & 0x01);
        VNLogVerbose("  Picture type: %s", pictureTypeToString(frameConfig->pictureType));

        /* temporal_refresh_bit_flag: 1 bit */
        frameConfig->temporalRefresh = ((data >> 1) & 0x01);
        VNLogVerbose("  Temporal refresh: %u", frameConfig->temporalRefresh);

        /* temporal_signalling_present_flag: 1 bit */
        frameConfig->temporalSignallingPresent = ((data >> 0) & 0x01);
        VNLogVerbose("  Temporal chunk enabled: %u", frameConfig->temporalSignallingPresent);

        if (frameConfig->globalConfigSet) {
            /* Same situation as with LCEVC enabled, excepting that
             * dither control is implicitly not signalled here. */
            VNLogVerbose("Resetting dither state on IDR with LCEVC disabled");
            frameConfig->ditherType = DTNone;
            frameConfig->ditherStrength = 0;
        }
    }

    /* Prior to BitstreamVersionAlignWithSpec, this data was only sent if enhancement was enabled */
    if (globalConfig->bitstreamVersion >= BitstreamVersionAlignWithSpec || frameConfig->entropyEnabled) {
        VNCheckB(parseBlockPictureConfigMisc(stream, qmMode, stepWidthLOQ1Enabled, dequantOffsetEnabled,
                                             ditherControlPresent, frameConfig, globalConfig));
    }

    frameConfig->frameConfigSet = true;

    return true;
}

bool chunkCheckAlloc(LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    assert(frameConfig);

    uint32_t chunkCount = 0;

    /* Determine number of desired chunks. */
    if (frameConfig->entropyEnabled) {
        for (int32_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
            chunkCount += (globalConfig->numTiles[plane][LOQ0] + globalConfig->numTiles[plane][LOQ1]) *
                          globalConfig->numLayers;
        }
    }

    if (frameConfig->temporalSignallingPresent) {
        for (int32_t plane = 0; plane < globalConfig->numPlanes; ++plane) {
            chunkCount += globalConfig->numTiles[plane][LOQ0];
        }
    }

    /* Reallocate chunk memory if needed. */
    if (chunkCount != frameConfig->numChunks || !frameConfig->chunks) {
        if (frameConfig->chunks) {
            VNFree(frameConfig->allocator, &frameConfig->chunkAllocation);
        }

        frameConfig->chunks = VNReallocateArray(frameConfig->allocator,
                                                &frameConfig->chunkAllocation, LdeChunk, chunkCount);
        frameConfig->numChunks = chunkCount;
    }

    if (!frameConfig->chunks) {
        VNLogError("Memory allocation for chunk data failed");
        return false;
    }

    VNLogVerbose("  Chunk count: %d", frameConfig->numChunks);

    return true;
}

bool parseChunk(ByteStream* stream, LdeChunk* chunk, bool* loqEntropyEnabled, TiledSizeDecoder* sizeDecoder)
{
    chunk->size = 0;

    if (chunk->entropyEnabled) {
        if (sizeDecoder) {
            int16_t chunkSize = tiledSizeDecoderRead(sizeDecoder);

            if (chunkSize < 0) {
                VNLogError("Failed to decode compressed chunk size");
                return false;
            }

            chunk->size = (size_t)chunkSize;
        } else {
            uint64_t chunkSizeMulti;
            VNCheckB(bytestreamReadMultiByte(stream, &chunkSizeMulti));

            if (chunkSizeMulti > INT_MAX) {
                VNLogError("Chunk data size is larger than INT_MAX");
                return false;
            }

            chunk->size = (size_t)chunkSizeMulti;
        }

        /* Signal that the LOQ has some data. */
        if (loqEntropyEnabled) {
            *loqEntropyEnabled = true;
        }

        chunk->data = bytestreamCurrent(stream);
        VNCheckB(bytestreamSeek(stream, chunk->size));

        VNLogVerbose("%s=%u", chunk->rleOnly ? "RLE" : "Huffman", (int32_t)chunk->size);
    } else {
        VNLogVerbose("disabled");
    }

    return true;
}

bool parseChunkFlags(BitStream* stream, LdeChunk* chunks, int32_t numChunks)
{
    for (int32_t i = 0; i < numChunks; ++i) {
        VNCheckB(bitstreamReadBit(stream, (uint8_t*)&chunks[i].entropyEnabled));
        VNCheckB(bitstreamReadBit(stream, &chunks[i].rleOnly));
    }

    return true;
}

bool parseCoefficientChunks(ByteStream* stream, const LdeGlobalConfig* globalConfig,
                            LdeFrameConfig* frameConfig, const LdeLOQIndex loq, const uint32_t planeIdx)
{
    LdeChunk* chunks;
    VNCheckB(getLayerChunks(globalConfig, frameConfig, planeIdx, loq, 0, &chunks));

    for (uint8_t layer = 0; layer < globalConfig->numLayers; ++layer) {
        VNLogVerbose("    [%d, %d, %2d]: ", planeIdx, (uint8_t)loq, layer);
        VNCheckB(parseChunk(stream, &chunks[layer], &frameConfig->loqEnabled[loq], NULL));
    }

    VNLogVerbose("    %s enabled: %d", loqIndexToString((LdeLOQIndex)loq), frameConfig->loqEnabled[loq]);

    return true;
}
