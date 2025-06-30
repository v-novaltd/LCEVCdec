/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include "chunk.h"

#include <assert.h>
#include <stdbool.h>

bool temporalChunkEnabled(const LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig)
{
    /* 8.3.5.2 */
    if (frameConfig->entropyEnabled) {
        /* "if no_enhancement_bit_flag is set to 0", step 1 */
        return globalConfig->temporalEnabled && !frameConfig->temporalRefresh;
    }

    /* "if no_enhancement_bit_flag is set to 1", step 1 */
    return globalConfig->temporalEnabled && !frameConfig->temporalRefresh &&
           frameConfig->temporalSignallingPresent;
}

uint32_t getLayerChunkIndex(const LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig,
                            const LdeLOQIndex loq, const uint32_t planeIdx, const uint32_t tileIdx,
                            const uint32_t layer)
{
    return frameConfig->tileChunkResidualIndex[planeIdx][loq] + (tileIdx * globalConfig->numLayers) + layer;
}

bool getLayerChunks(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                    const uint32_t planeIdx, const LdeLOQIndex loq, const uint32_t tileIdx,
                    LdeChunk** chunks)
{
    if (!globalConfig || !chunks) {
        return false;
    }

    if (planeIdx >= globalConfig->numPlanes) {
        return false;
    }

    if (loq != LOQ0 && loq != LOQ1) {
        return false;
    }

    if (frameConfig->entropyEnabled && frameConfig->chunks) {
        const uint32_t chunkIndex =
            getLayerChunkIndex(frameConfig, globalConfig, loq, planeIdx, tileIdx, 0);

        if (tileIdx >= globalConfig->numTiles[planeIdx][loq]) {
            return false;
        }

        assert(chunkIndex <= frameConfig->numChunks);

        *chunks = &frameConfig->chunks[chunkIndex];
    } else {
        *chunks = NULL;
    }

    return true;
}

bool getTemporalChunk(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                      const uint32_t planeIdx, const uint32_t tileIdx, LdeChunk** chunk)

{
    if (!globalConfig || !chunk) {
        return false;
    }

    if (planeIdx > globalConfig->numPlanes) {
        return false;
    }

    if (temporalChunkEnabled(frameConfig, globalConfig) && frameConfig->chunks) {
        const uint32_t chunkIndex = frameConfig->tileChunkTemporalIndex[planeIdx] + tileIdx;

        if (tileIdx >= globalConfig->numTiles[planeIdx][LOQ0]) {
            return false;
        }

        assert(chunkIndex < frameConfig->numChunks);

        *chunk = &frameConfig->chunks[chunkIndex];
    } else {
        *chunk = NULL;
    }

    return true;
}
