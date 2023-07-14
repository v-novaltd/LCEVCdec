/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_BITSTREAM_H_
#define VN_DEC_CORE_BITSTREAM_H_

#include "common/bytestream.h"

/*------------------------------------------------------------------------------*/

/*! \brief BitStream state
 *
 *  Contains state of a bit accessible stream that can only be read from.
 *
 *  The stream data is expected to be batched into 32-bit words stored in
 *  big-endian ordering. */
typedef struct BitStream
{
    ByteStream_t byteStream; /**< Byte stream tracking state of stream data. */
    uint32_t word;           /**< Current word loaded read from the byte stream. */
    uint8_t nextBit;         /**< Next bit to read from stream */
} BitStream_t;

/*------------------------------------------------------------------------------*/

/*! \brief initialize the bit stream state
 *  \param state        Stream to initialize.
 *  \param data         Pointer to stream data.
 *  \param dataLength   Size of stream data in bytes.
 *
 *  \return <0 on failure */
int32_t bitstreamInitialise(BitStream_t* stream, const uint8_t* data, size_t size);

/*! \brief read a single bit from the stream
 *
 *  \return bit <0 on failure */
int32_t bitstreamReadBit(BitStream_t* stream, uint8_t* out);

/*! \brief read a n bits from the stream.
 *
 *  \return <0 on failure */
int32_t bitstreamReadBits(BitStream_t* stream, uint8_t numBits, int32_t* out);

/*! \brief read a variable length exp-golomb encoded 32-bit unsigned integer
 *
 *  \return <0 on failure */
int32_t bitstreamReadExpGolomb(BitStream_t* stream, uint32_t* out);

/*! \brief get number of remaining bits
 *
 *  \return number of bits remaining in stream */
uint64_t bitstreamGetRemainingBits(const BitStream_t* stream);

/*! \brief get the number of bits read by the bitstream.
 *
 *  \return number of bits read from the stream. */
uint64_t bitstreamGetConsumedBits(const BitStream_t* stream);

/*! \brief get the number of bytes read by the bitstream - partially read bytes
 *         are rounded up.
 *
 *  \return number of bytes read from the stream. */
uint32_t bitstreamGetConsumedBytes(const BitStream_t* stream);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_BITSTREAM_H_ */