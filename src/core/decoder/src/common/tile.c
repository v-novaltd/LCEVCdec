/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/tile.h"

#include "common/memory.h"
#include "decode/deserialiser.h"

int32_t tileDataInitialize(CacheTileData_t* tileData, Memory_t memory,
                           const DeserialisedData_t* data, int32_t planeIndex, LOQIndex_t loq)
{
    /* Validate args. */
    if (planeIndex < 0 || planeIndex > 2) {
        return -1;
    }

    const uint32_t tileCount = data->tileCount[planeIndex][loq];
    const int32_t tileWidth = data->tileWidth[planeIndex];
    const int32_t tileHeight = data->tileHeight[planeIndex];

    if (tileData->tileCount != tileCount) {
        TileState_t* newPtr = VN_REALLOC_T_ARR(memory, tileData->tiles, TileState_t, tileCount);

        if (!newPtr) {
            return -1;
        }

        tileData->tiles = newPtr;
        tileData->tileCount = tileCount;

        memset(tileData->tiles, 0, tileCount * sizeof(TileState_t));
    }

    uint32_t planeWidth = 0;
    uint32_t planeHeight = 0;
    deserialiseCalculateSurfaceProperties(data, loq, planeIndex, &planeWidth, &planeHeight);
    for (int32_t tileIndex = 0; tileIndex < (int32_t)tileCount; ++tileIndex) {
        const int32_t tileIndexX = tileIndex % data->tilesAcross[planeIndex][loq];
        const int32_t tileIndexY = tileIndex / data->tilesAcross[planeIndex][loq];
        assert(tileIndexY < data->tilesDown[planeIndex][loq]);

        TileState_t* tile = &tileData->tiles[tileIndex];
        tile->x = tileIndexX * tileWidth;
        tile->y = tileIndexY * tileHeight;

        tile->width = minS32(tileWidth, (int32_t)planeWidth - tile->x);
        tile->height = minS32(tileHeight, (int32_t)planeHeight - tile->y);

        if (deserialiseGetTileLayerChunks(data, planeIndex, loq, tileIndex, &tile->chunks) != 0) {
            return -1;
        }

        if (loq == LOQ0) {
            if (deserialiseGetTileTemporalChunk(data, planeIndex, tileIndex, &tile->temporalChunk) != 0) {
                return -1;
            }
        } else {
            tile->temporalChunk = NULL;
        }
    }

    return 0;
}
