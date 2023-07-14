/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_ENTROPY_H_
#define VN_DEC_CORE_ENTROPY_H_

#include "decode/deserialiser.h"
#include "decode/huffman.h"

/*------------------------------------------------------------------------------*/

enum EntropyConstants
{
    EntropyNoData = -2
};

typedef enum EntropyDecoderType
{
    EDTDefault = 0,
    EDTTemporal,
    EDTSizeUnsigned,
    EDTSizeSigned,
    EDTCount
} EntropyDecoderType_t;

typedef struct EntropyDecoder
{
    Context_t* ctx;
    uint8_t currHuff;
    uint32_t rawOffset;
    BitStream_t stream;
    HuffmanDecodeState_t huffman[3];
    HuffmanStream_t hstream;
    bool rleOnly;
    const uint8_t* rleData;
    bool entropyEnabled;
    const Chunk_t* chunk;
    EntropyDecoderType_t type;
} EntropyDecoder_t;

/*------------------------------------------------------------------------------*/

/*! \brief Initialize a entropy decoder into a default state from for decompressing.
 *
 *  \param ctx    decoding context
 *  \param state  state of the decoder to initialize
 *  \param chunk  chunk to use for this layer.
 *  \param type   specifies the type of layer decoder to prepare for this chunk.
 *
 *  \return 0 on success, otherwise -1. */
int32_t entropyInitialise(Context_t* ctx, EntropyDecoder_t* state, const Chunk_t* chunk,
                          EntropyDecoderType_t type);

/*! \brief Release the entropy decoder, cleaning up any memory allocated during decode.
 *
 *  \param state  layer decoder state to clean up. */
void entropyRelease(EntropyDecoder_t* state);

/*! \brief Decode the next residual pel from a stream.
 *
 *  \param state  layer decoder state to decode from
 *  \param pel    s8.7 fixed point residual to decode into
 *
 *  \return the number of zeros following this pel (<0 on fail) */
int32_t entropyDecode(EntropyDecoder_t* state, int16_t* pel);

/*! \brief Decode the next temporal signal from a temporal stream.
 *
 *  \param state  layer decoder state to decode from
 *  \param pel    u8.0 location to decode temporal signal into
 *
 *  \return the number of zeros following this pel (<0 on fail) */
int32_t entropyDecodeTemporal(EntropyDecoder_t* state, uint8_t* pel);

/*! \brief Decode the next size signal from a compressed size stream.
 *
 *  \param state  layer decoder state to decode from
 *  \param size   int16_t location to decode size into
 *
 *  \return -1 on success otherwise 0. */
int32_t entropyDecodeSize(EntropyDecoder_t* state, int16_t* size);

/*! \brief Retrieve the number of bytes consumed by the entropy decoder.
 *
 *  \param state  layer decoder state.
 *
 *  \return the number of bytes read by the decoder. */
uint32_t entropyGetConsumedBytes(const EntropyDecoder_t* state);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_ENTROPY_H_ */