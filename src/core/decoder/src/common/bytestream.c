/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "common/bytestream.h"

#include "common/memory.h"
#include "common/types.h"

/*------------------------------------------------------------------------------*/

/*! uint64_t can use at most 10-bytes for signaling. */
static const uint32_t kMultiByteMaxBytes = 10;

/*------------------------------------------------------------------------------*/

/*! \brief Helper function for validating that a proposed change to the streams
 *         offset will be valid. */
static inline bool offsetValidation(const ByteStream_t* state, size_t changeAmount)
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

int32_t bytestreamInitialise(ByteStream_t* stream, const uint8_t* data, size_t size)
{
    if (data == NULL || size == 0) {
        return -1;
    }

    stream->data = data;
    stream->size = size;
    stream->offset = 0;
    return 0;
}

int32_t bytestreamReadU64(ByteStream_t* stream, uint64_t* out)
{
    if (!offsetValidation(stream, sizeof(uint64_t))) {
        return -1;
    }

    stream->offset += readU64(stream->data + stream->offset, out);
    return 0;
}

int32_t bytestreamReadU32(ByteStream_t* stream, uint32_t* out)
{
    if (!offsetValidation(stream, sizeof(uint32_t))) {
        return -1;
    }

    stream->offset += readU32(stream->data + stream->offset, out);
    return 0;
}

int32_t bytestreamReadU16(ByteStream_t* stream, uint16_t* out)
{
    if (!offsetValidation(stream, sizeof(uint16_t))) {
        return -1;
    }

    stream->offset += readU16(stream->data + stream->offset, out);
    return 0;
}

int32_t bytestreamReadU8(ByteStream_t* stream, uint8_t* out)
{
    if (!offsetValidation(stream, sizeof(uint8_t))) {
        return -1;
    }

    stream->offset += readU8(stream->data + stream->offset, out);
    return 0;
}

int32_t bytestreamReadN8(ByteStream_t* stream, uint8_t* out, int32_t numBytes)
{
    if (!offsetValidation(stream, numBytes)) {
        return -1;
    }

    memoryCopy(out, stream->data + stream->offset, numBytes);
    stream->offset += numBytes;
    return 0;
}

int32_t bytestreamReadMultiByte(ByteStream_t* stream, uint64_t* out)
{
    if (stream->offset > stream->size) {
        return -1;
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
        return -1;
    }

    if (byteCount >= maxReadBytes && !valid) {
        return -1;
    }

    if (!offsetValidation(stream, byteCount)) {
        return -1;
    }

    stream->offset += byteCount;
    *out = dst;

    return 0;
}

int32_t bytestreamSeek(ByteStream_t* stream, size_t offset)
{
    if (!offsetValidation(stream, offset)) {
        return -1;
    }

    stream->offset += offset;
    return 0;
}

const uint8_t* bytestreamCurrent(const ByteStream_t* stream)
{
    if (stream->offset >= stream->size) {
        return NULL;
    }

    return (stream->data + stream->offset);
}

size_t bytestreamRemaining(const ByteStream_t* stream)
{
    if (stream->offset > stream->size) {
        return 0;
    }

    return stream->size - stream->offset;
}

size_t byteStreamGetSize(const ByteStream_t* stream) { return stream->size; }

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

int32_t readU8(const uint8_t* ptr, uint8_t* out)
{
    *out = *ptr;
    return 1;
}

/*------------------------------------------------------------------------------*/