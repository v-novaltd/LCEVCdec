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

#include "common/bitstream.h"

/*------------------------------------------------------------------------------*/

/*! \brief Helper function to load a new DWORD from the bytestream. */
static inline int32_t loadWord(BitStream_t* stream)
{
    const size_t remaining = bytestreamRemaining(&stream->byteStream);

    if (remaining <= 0) {
        return -1;
    }

    if (remaining >= 4) {
        /* Enough to read a complete word */
        if (bytestreamReadU32(&stream->byteStream, &stream->word) != 0) {
            return -1;
        }

        stream->nextBit = 0;
    } else {
        /* Not enough for a word, read in as much as possible. */
        stream->word = 0;

        uint8_t byte = 0;

        for (size_t i = 0; i < remaining; ++i) {
            if (bytestreamReadU8(&stream->byteStream, &byte) != 0) {
                return -1;
            }

            stream->word <<= 8;
            stream->word |= byte;
        }

        stream->nextBit = (uint8_t)(8 * (4 - remaining));
        stream->word <<= stream->nextBit;
    }

    return 0;
}

/*! \brief Determines if the next word needs to be loaded and then loads if needed.  */
static inline int32_t checkLoadNextWord(BitStream_t* stream)
{
    if ((stream->nextBit == 32) && (loadWord(stream) < 0)) {
        return -1;
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

int32_t bitstreamInitialise(BitStream_t* stream, const uint8_t* data, size_t size)
{
    if (bytestreamInitialise(&stream->byteStream, data, size) != 0) {
        return -1;
    }

    stream->word = 0;
    stream->nextBit = 0;

    return loadWord(stream);
}

int32_t bitstreamReadBit(BitStream_t* stream, uint8_t* out)
{
    assert(out);

    if (streamComplete(stream) || (checkLoadNextWord(stream) != 0)) {
        return -1;
    }

    *out = stream->word >> 31;
    stream->word <<= 1;
    stream->nextBit++;

    return 0;
}

void bitstreamReadBitUnchecked(BitStream_t* stream, uint8_t* out)
{
    assert(out && !streamComplete(stream));
    if (stream->nextBit == 32) {
        loadWordUnchecked(stream);
    }
    *out = stream->word >> 31;
    stream->word <<= 1;
    stream->nextBit++;
}

int32_t bitstreamReadBits(BitStream_t* stream, uint8_t numBits, int32_t* out)
{
    assert(out && (numBits <= kMaxBitsAtOnce));

    // @todo(bob): handle attempt to read more than is available.
    if (streamComplete(stream) || checkLoadNextWord(stream) != 0) {
        return -1;
    }

    /* Load up current word bits. */
    const uint8_t wordRemaining = 32 - stream->nextBit;
    *out = (int32_t)(stream->word >> (32 - numBits));

    if (wordRemaining >= numBits) {
        stream->nextBit += numBits;
        stream->word <<= numBits;
    } else {
        /* Handle outstanding bits to be read in. */
        const uint8_t readRemaining = numBits - wordRemaining;

        if (loadWord(stream) != 0) {
            return -1;
        }

        stream->nextBit += readRemaining;
        const int32_t trailBits = (int32_t)(stream->word >> (32 - readRemaining));
        stream->word <<= readRemaining;
        *out |= trailBits;
    }

    return 0;
}

int32_t bitstreamReadExpGolomb(BitStream_t* stream, uint32_t* out)
{
    if (!stream || !out) {
        return -1;
    }

    /* Read the prefix zeros.*/
    int32_t res = 0;
    uint8_t bit = 0;
    VN_CHECK(bitstreamReadBit(stream, &bit));

    uint32_t readCount = 0;
    while (!bit) {
        readCount++;
        VN_CHECK(bitstreamReadBit(stream, &bit));
    }

    /* Loop skipped when there are no prefix zeros, meaning the return value
     * is correctly 0. */
    uint32_t value = 1;
    while (readCount--) {
        VN_CHECK(bitstreamReadBit(stream, &bit));
        value = (value << 1) | bit;
    }

    *out = value - 1;
    return 0;
}

/*------------------------------------------------------------------------------*/
