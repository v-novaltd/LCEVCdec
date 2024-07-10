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

#ifndef VN_DEC_CORE_COMMON_TILE_H_
#define VN_DEC_CORE_COMMON_TILE_H_

#include "common/types.h"

typedef struct Chunk Chunk_t;
typedef struct CmdBuffer CmdBuffer_t;
typedef struct DeserialisedData DeserialisedData_t;

typedef struct TileState
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    Chunk_t* chunks;
    Chunk_t* temporalChunk;
    CmdBuffer_t* cmdBuffer;
} TileState_t;

typedef struct CacheTileData
{
    TileState_t* tiles;
    uint32_t tileCount;
} CacheTileData_t;

/*! \brief Initialises the tileData struct and internal tiles for the stream parameters given by
 *         the deserialised data for a given plane.
 *
 *
 *  \param tileData     Input CacheTileData pointer.
 *  \param memory       Memory struct for allocation.
 *  \param data         An initialised deserialised data struct.
 *  \param planeIndex   Plane index of deserailised data to use.
 *  \param loq          LOQ index to determine tile sizes.
 *
 *  \return 0 on success.
 */
int32_t tileDataInitialize(CacheTileData_t* tileData, Memory_t memory,
                           const DeserialisedData_t* data, int32_t planeIndex, LOQIndex_t loq);

#endif // VN_DEC_CORE_COMMON_TILE_H_
