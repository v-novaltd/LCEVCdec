/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#include "bitstream.h"

#include <LCEVC/common/check.h>

/*------------------------------------------------------------------------------*/

/*! \brief Helper function to load a new DWORD from the bytestream. */
static inline bool loadWord(BitStream* stream)
{
    const size_t remaining = bytestreamRemaining(&stream->byteStream);

    if (remaining <= 0) {
        return false;
    }

    if (remaining >= 4) {
        /* Enough to read a complete word */
        if (!bytestreamReadU32(&stream->byteStream, &stream->word)) {
            return false;
        }

        stream->nextBit = 0;
    } else {
        /* Not enough for a word, read in as much as possible. */
        stream->word = 0;

        uint8_t byte = 0;

        for (size_t i = 0; i < remaining; ++i) {
            if (!bytestreamReadU8(&stream->byteStream, &byte)) {
                return false;
            }

            stream->word <<= 8;
            stream->word |= byte;
        }

        stream->nextBit = (uint8_t)(8 * (4 - remaining));
        stream->word <<= stream->nextBit;
    }

    return true;
}

/*! \brief Determines if the next word needs to be loaded and then loads if needed.  */
static inline bool checkLoadNextWord(BitStream* stream)
{
    if (stream->nextBit == 32 && !loadWord(stream)) {
        return false;
    }

    return true;
}

/*------------------------------------------------------------------------------*/

bool bitstreamInitialize(BitStream* stream, const uint8_t* data, size_t size)
{
    if (!bytestreamInitialize(&stream->byteStream, data, size)) {
        return false;
    }

    stream->word = 0;
    stream->nextBit = 0;

    return loadWord(stream);
}

bool bitstreamReadBit(BitStream* stream, uint8_t* out)
{
    assert(out);

    if (streamComplete(stream) || !checkLoadNextWord(stream)) {
        return false;
    }

    *out = stream->word >> 31;
    stream->word <<= 1;
    stream->nextBit++;

    return true;
}

void bitstreamReadBitUnchecked(BitStream* stream, uint8_t* out)
{
    assert(out && !streamComplete(stream));
    if (stream->nextBit == 32) {
        loadWordUnchecked(stream);
    }
    *out = stream->word >> 31;
    stream->word <<= 1;
    stream->nextBit++;
}

bool bitstreamReadBits(BitStream* stream, uint8_t numBits, int32_t* out)
{
    assert(out && (numBits <= kMaxBitsAtOnce));

    if (streamComplete(stream) || !checkLoadNextWord(stream)) {
        return false;
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

        if (!loadWord(stream)) {
            return false;
        }

        stream->nextBit += readRemaining;
        const int32_t trailBits = (int32_t)(stream->word >> (32 - readRemaining));
        stream->word <<= readRemaining;
        *out |= trailBits;
    }

    return true;
}

bool bitstreamReadExpGolomb(BitStream* stream, uint32_t* out)
{
    if (!stream || !out) {
        return false;
    }

    /* Read the prefix zeros.*/
    uint8_t bit = 0;
    VNCheckB(bitstreamReadBit(stream, &bit));

    uint32_t readCount = 0;
    while (!bit) {
        readCount++;
        VNCheckB(bitstreamReadBit(stream, &bit));
    }

    /* Loop skipped when there are no prefix zeros, meaning the return value
     * is correctly 0. */
    uint32_t value = 1;
    while (readCount--) {
        VNCheckB(bitstreamReadBit(stream, &bit));
        value = (value << 1) | bit;
    }

    *out = value - 1;
    return true;
}

/*------------------------------------------------------------------------------*/
