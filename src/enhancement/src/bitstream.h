/* Copyright (c) V-Nova International Limited 2022-2026. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_BITSTREAM_H
#define VN_LCEVC_ENHANCEMENT_BITSTREAM_H

#include "bytestream.h"

#include <LCEVC/common/stdlib.h>

/*------------------------------------------------------------------------------*/

#ifndef NDEBUG
static const uint8_t kMaxBitsAtOnce = 31;
#endif

/*------------------------------------------------------------------------------*/

/*! \brief BitStream state
 *
 *  Contains state of a bit accessible stream that can only be read from.
 *
 *  The stream data is expected to be batched into 32-bit words stored in
 *  big-endian ordering. */
typedef struct BitStream
{
    ByteStream byteStream; /**< Byte stream tracking state of stream data. */
    uint32_t word;         /**< Current word loaded read from the byte stream. */
    uint8_t nextBit;       /**< Next bit to read from stream */
} BitStream;

/*------------------------------------------------------------------------------*/

/*! \brief initialize the bit stream state
 *  \param stream       Stream to initialize.
 *  \param data         Pointer to stream data.
 *  \param size         Size of stream data in bytes.
 *
 *  \return True on success, otherwise false */
bool bitstreamInitialize(BitStream* stream, const uint8_t* data, size_t size);

/*! \brief read a single bit from the stream
 *
 *  \return bit True on success, otherwise false */
bool bitstreamReadBit(BitStream* stream, uint8_t* out);

/*! \brief read n bits from the stream.
 *
 *  \return True on success, otherwise false */
bool bitstreamReadBits(BitStream* stream, uint8_t numBits, int32_t* out);

/*! \brief Helper function to determine if the bitstream is complete. */
static inline bool streamComplete(const BitStream* stream)
{
    const bool byteStreamComplete = bytestreamRemaining(&stream->byteStream) == 0;
    const bool wordComplete = (stream->nextBit == 32);
    return byteStreamComplete && wordComplete;
}

/*! \brief Read a variable length exponential-Golomb encoded 32-bit unsigned integer. Exponential-
 *         Golomb coding is used for some VUI (Video Usability Information) data.
 *
 *  \return True on success, otherwise false */
bool bitstreamReadExpGolomb(BitStream* stream, uint32_t* out);

/*! \brief get number of remaining bits
 *
 *  \return number of bits remaining in stream */
static inline size_t bitstreamGetRemainingBits(const BitStream* stream)
{
    const size_t wordBitsRemaining = 32 - stream->nextBit;
    const size_t byteBitsRemaining = bytestreamRemaining(&stream->byteStream) * 8;
    return wordBitsRemaining + byteBitsRemaining;
}

/*! \brief get the number of bits read by the bitstream.
 *
 *  \return number of bits read from the stream. */
static inline size_t bitstreamGetConsumedBits(const BitStream* stream)
{
    const size_t remainingBits = bitstreamGetRemainingBits(stream);
    const size_t overallSize = byteStreamGetSize(&stream->byteStream) * 8;
    return overallSize - remainingBits;
}

/*! \brief get the number of bytes read by the bitstream - partially read bytes
 *         are rounded up.
 *
 *  \return number of bytes read from the stream. */
static inline uint32_t bitstreamGetConsumedBytes(const BitStream* stream)
{
    const size_t consumedBits = bitstreamGetConsumedBits(stream);
    return (uint32_t)((consumedBits + 7) >> 3);
}

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_BITSTREAM_H
