/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_DEC_CORE_DECODE_SERIAL_H_
#define VN_DEC_CORE_DECODE_SERIAL_H_

#include "common/stats.h"
#include "common/types.h"

/*
    @todo (Bob): Will be addressed at a later date
        * Remove S7 format conversion from this function.
        * Do not directly reference deserialiser, this includes passing on params:
            * Array of chunks for each dst surface
            * Transform type to apply
            * Scaling mode for loq_index == LOQ_0.
            * Step-width info
            * dequant info (pre-calculated step-widths maybe...)
        * Remove AC_MaxResidualParallel.
        * Remove direct access to ctx to grab internal DecodeSerial_t instance.
*/

/*------------------------------------------------------------------------------*/

typedef struct CmdBuffer CmdBuffer_t;
typedef struct CmdBufferEntryPoint CmdBufferEntryPoint_t;
typedef struct Context Context_t;
typedef struct FrameStats* FrameStats_t;
typedef struct Logger* Logger_t;
typedef struct Memory* Memory_t;
typedef struct Surface Surface_t;
typedef struct TileState TileState_t;

/*------------------------------------------------------------------------------*/

typedef struct DecodeSerial* DecodeSerial_t;

/*! \brief Initialize some data that gets preserved between different calls to applyResidualJob,
 *         or between applyResidualJob and applyCmdBuffer.
 *
 *  \param memory  Decoder's memory manager.
 *  \param decode  Struct of persistent decoding data (tile cache, command buffers, etc).
 *
 *  \return true on success */
bool decodeSerialInitialize(Memory_t memory, DecodeSerial_t* decode);

/*! \brief Release the persistent data used by applyResidualJob. Only call this when releasing the
 *         decoder (otherwise it's not exactly "persistent data" is it?).
 *
 *  \param decode  Struct of persistent decoding data. */
void decodeSerialRelease(DecodeSerial_t decode);

/*! \brief Contains (almost) all the parameters needed to perform residual decoding. TODO: isolate
 *         the member variables in Context_t that are used, and drag them into this struct
 *         instead. */
typedef struct DecodeSerialArgs
{
    Surface_t* dst[3]; /* Where to decode residuals into. */
    LOQIndex_t loq;    /* LOQ being applied */
    Memory_t memory;
    Logger_t log;
    FrameStats_t stats;
    bool tuCoordsAreInSurfaceRasterOrder;
    bool applyTemporal;
} DecodeSerialArgs_t;

/*! \brief Apply residuals to an LoQ
 *  \param ctx   Decoder context.
 *  \param args  The parameters to use for this residual application.
 *  \return 0 on success */
int32_t decodeSerial(Context_t* ctx, const DecodeSerialArgs_t* args);

uint32_t decodeSerialGetTileCount(const DecodeSerial_t decode, uint8_t planeIdx);

TileState_t* decodeSerialGetTile(const DecodeSerial_t decode, uint8_t planeIdx);

CmdBuffer_t* decodeSerialGetCmdBuffer(DecodeSerial_t decode, uint8_t planeIdx, uint8_t tileIdx);

CmdBufferEntryPoint_t* decodeSerialGetCmdBufferEntryPoint(DecodeSerial_t decode, uint8_t planeIdx,
                                                          uint8_t tileIdx, uint16_t entryPointIndex);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DECODE_SERIAL_H_ */
