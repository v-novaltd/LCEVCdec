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

#include "bytestream.h"

#include <LCEVC/common/limit.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

/*! uint64_t can use at most 10-bytes for signaling. */
static const uint32_t kMultiByteMaxBytes = 10;

/*------------------------------------------------------------------------------*/

/*! \brief Helper function for validating that a proposed change to the streams
 *         offset will be valid. */
static inline bool offsetValidation(const ByteStream* state, size_t changeAmount)
{
    const size_t proposedOffset = state->offset + changeAmount;

    /* Check we don't go past the end of the bitstream */
    const bool isPastEnd = proposedOffset > state->size;

    /* Check we don't overflow the calculation */
    const bool isOverflow = proposedOffset < state->offset;

    /* Check we don't overflow the calculation but end in a valid range.*/
    const bool isOverflowInRange = changeAmount > state->size;

    return !(isPastEnd || isOverflow || isOverflowInRange);
}

bool bytestreamInitialize(ByteStream* stream, const uint8_t* data, size_t size)
{
    if (data == NULL || size == 0) {
        return false;
    }

    stream->data = data;
    stream->size = size;
    stream->offset = 0;
    return true;
}

bool bytestreamReadU64(ByteStream* stream, uint64_t* out)
{
    if (!offsetValidation(stream, sizeof(uint64_t))) {
        return false;
    }

    stream->offset += readU64(stream->data + stream->offset, out);
    return true;
}

bool bytestreamReadU32(ByteStream* stream, uint32_t* out)
{
    if (!offsetValidation(stream, sizeof(uint32_t))) {
        return false;
    }

    stream->offset += readU32(stream->data + stream->offset, out);
    return true;
}

bool bytestreamReadU16(ByteStream* stream, uint16_t* out)
{
    if (!offsetValidation(stream, sizeof(uint16_t))) {
        return false;
    }

    stream->offset += readU16(stream->data + stream->offset, out);
    return true;
}

bool bytestreamReadU8(ByteStream* stream, uint8_t* out)
{
    if (!offsetValidation(stream, sizeof(uint8_t))) {
        return false;
    }

    *out = *(stream->data + stream->offset);
    stream->offset += 1;
    return true;
}

bool bytestreamReadN8(ByteStream* stream, uint8_t* out, int32_t numBytes)
{
    if (!offsetValidation(stream, numBytes)) {
        return false;
    }

    memcpy(out, stream->data + stream->offset, numBytes);
    stream->offset += numBytes;
    return true;
}

bool bytestreamReadMultiByte(ByteStream* stream, uint64_t* out)
{
    if (stream->offset > stream->size) {
        return false;
    }

    const uint8_t* ptr = stream->data + stream->offset;
    const size_t maxReadBytes = minSize(stream->size - stream->offset, kMultiByteMaxBytes);
    size_t byteCount = 0;
    uint64_t dst = 0;
    bool valid = false;

    while (byteCount < maxReadBytes) {
        const uint8_t val = ptr[byteCount++];

        dst = (dst << 7) | (val & 0x7F);

        if ((val & 0x80) == 0) {
            valid = true;
            break;
        }
    }

    if (byteCount == kMultiByteMaxBytes && !valid) {
        return false;
    }

    if (byteCount >= maxReadBytes && !valid) {
        return false;
    }

    if (!offsetValidation(stream, byteCount)) {
        return false;
    }

    stream->offset += byteCount;
    *out = dst;

    return true;
}

bool bytestreamSeek(ByteStream* stream, size_t offset)
{
    if (!offsetValidation(stream, offset)) {
        return false;
    }

    stream->offset += offset;
    return true;
}

const uint8_t* bytestreamCurrent(const ByteStream* stream)
{
    if (stream->offset >= stream->size) {
        return NULL;
    }

    return (stream->data + stream->offset);
}

size_t byteStreamGetSize(const ByteStream* stream) { return stream->size; }

/*------------------------------------------------------------------------------*/

int32_t readU64(const uint8_t* ptr, uint64_t* out)
{
    /* clang-format off */
	*out =    (((uint64_t)ptr[0]) << 56) | (((uint64_t)ptr[1]) << 48)
			| (((uint64_t)ptr[2]) << 40) | (((uint64_t)ptr[3]) << 32)
			| (((uint64_t)ptr[4]) << 24) | (((uint64_t)ptr[5]) << 16)
			| (((uint64_t)ptr[6]) << 8)  |   (uint64_t)ptr[7];
    /* clang-format on */
    return 8;
}

int32_t readU32(const uint8_t* ptr, uint32_t* out)
{
    *out = ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) | ((uint32_t)ptr[2] << 8) |
           (uint32_t)ptr[3];
    return 4;
}

int32_t readU16(const uint8_t* ptr, uint16_t* out)
{
    *out = (uint16_t)((ptr[0] << 8) | ptr[1]);
    return 2;
}

/*------------------------------------------------------------------------------*/
