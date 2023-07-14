/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_DECODE_SERIAL_H_
#define VN_DEC_CORE_DECODE_SERIAL_H_

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
typedef struct Memory* Memory_t;
typedef struct Surface Surface_t;

/*------------------------------------------------------------------------------*/

typedef struct DecodeSerial* DecodeSerial_t;

/*! \brief Initialise some data that apply_residual may require during decoding.
 *  \param ctx     Decoder context.
 *  \return 0 on success */
bool decodeSerialInitialize(Memory_t memory, DecodeSerial_t* decode, bool generateCmdBuffers);

/*! \brief Release any data that apply_residual may have allocated during decoding
 *         this should only be called when releasing the decoder.
 *  \param ctx     Decoder context.
 *  \return 0 on success */
void decodeSerialRelease(DecodeSerial_t decode);

/*! \brief Contains all the parameters needed to perform residual decoding. */
typedef struct DecodeSerialArgs
{
    Surface_t* dst[3]; /* Where to decode residuals into. */
    LOQIndex_t loq;    /* LOQ being applied */
} DecodeSerialArgs_t;

/*! \brief Apply residuals to an LoQ
 *  \param ctx     Decoder context.
 *  \param args  The parameters to use for this residual application.
 *  \return 0 on success */
int32_t decodeSerial(Context_t* ctx, const DecodeSerialArgs_t* args);

CmdBuffer_t* decodeSerialGetTileClearCmdBuffer(const DecodeSerial_t decode);
CmdBuffer_t* decodeSerialGetResidualCmdBuffer(const DecodeSerial_t decode,
                                              TemporalSignal_t temporal, LOQIndex_t loq);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DECODE_SERIAL_H_ */
