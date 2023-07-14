/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_BYTESTREAM_H_
#define VN_DEC_CORE_BYTESTREAM_H_

#include "common/platform.h"

/*------------------------------------------------------------------------------*/

/*! \brief ByteStream state
 *
 *  Contains state of a byte accessible stream that can only seek in the forward
 *  direction.
 *
 *  Note: The stream data is expected to contain values in big-endian ordering.
 */
typedef struct ByteStream
{
    const uint8_t* data; /**< Pointer to the start of stream. */
    size_t offset;       /**< Byte offset from the start of the stream. */
    size_t size;         /**< Size of the stream in bytes. */
} ByteStream_t;

/*------------------------------------------------------------------------------*/

/*! \brief Initialise the byte stream state
 *  \param stream  Stream to initialise.
 *  \param data    Pointer to stream data.
 *  \param size    Length of stream.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamInitialise(ByteStream_t* stream, const uint8_t* data, size_t size);

/*! \brief Endian-safe uint64_t read from a byte stream.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamReadU64(ByteStream_t* stream, uint64_t* out);

/*! \brief Endian-safe uint32_t read from a byte stream.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamReadU32(ByteStream_t* stream, uint32_t* out);

/*! \brief Endian-safe uint16_t read from a byte stream.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamReadU16(ByteStream_t* stream, uint16_t* out);

/*! \brief uint8_t read from a byte stream
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamReadU8(ByteStream_t* stream, uint8_t* out);

/*! \brief read multiple uint8_t bytes from the byte stream.
    \return 0 on success, -1 on failure. */
int32_t bytestreamReadN8(ByteStream_t* stream, uint8_t* out, int32_t numBytes);

/*! \brief Read variable length encoded uint64_t from a byte stream.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamReadMultiByte(ByteStream_t* stream, uint64_t* out);

/*! \brief Forward direction only byte stream seek relative to current offset.
 *  \return 0 on success, -1 on failure. */
int32_t bytestreamSeek(ByteStream_t* stream, size_t offset);

/*! \brief Get the pointer to the current stream location.
 *  \return Pointer to location within the stream, or NULL upon error. */
const uint8_t* bytestreamCurrent(const ByteStream_t* stream);

/*! \brief Retrieve the remaining number of bytes to be read.
 *  \return Number of bytes remaining in the stream, or 0 upon error */
size_t bytestreamRemaining(const ByteStream_t* stream);

/*! \brief Retrieve the byte size of the overall stream.
    \return Number of bytes the stream was initialised with. */
size_t byteStreamGetSize(const ByteStream_t* stream);

/*------------------------------------------------------------------------------*/

/*! \brief Endian aware uint64_t read from a pointer.
 *  \return number of bytes read. */
int32_t readU64(const uint8_t* ptr, uint64_t* out);

/*! \brief Endian aware uint32_t read from a pointer.
 *  \return number of bytes read. */
int32_t readU32(const uint8_t* ptr, uint32_t* out);

/*! \brief Endian aware uint16_t read from a pointer.
 *  \return number of bytes read. */
int32_t readU16(const uint8_t* ptr, uint16_t* out);

/*! \brief Endian aware uint8_t read from a pointer.
 *  \return number of bytes read. */
int32_t readU8(const uint8_t* ptr, uint8_t* out);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_BYTESTREAM_H_ */