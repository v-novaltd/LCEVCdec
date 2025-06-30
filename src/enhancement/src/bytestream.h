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

#ifndef VN_LCEVC_ENHANCEMENT_BYTESTREAM_H
#define VN_LCEVC_ENHANCEMENT_BYTESTREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
} ByteStream;

/*------------------------------------------------------------------------------*/

/*! \brief Initialize the byte stream state
 *  \param stream  Stream to initialize.
 *  \param data    Pointer to stream data.
 *  \param size    Length of stream.
 *  \return True on success, otherwise false. */
bool bytestreamInitialize(ByteStream* stream, const uint8_t* data, size_t size);

/*! \brief Endian-safe uint64_t read from a byte stream.
 *  \return True on success, otherwise false. */
bool bytestreamReadU64(ByteStream* stream, uint64_t* out);

/*! \brief Endian-safe uint32_t read from a byte stream.
 *  \return True on success, otherwise false. */
bool bytestreamReadU32(ByteStream* stream, uint32_t* out);

/*! \brief Endian-safe uint16_t read from a byte stream.
 *  \return True on success, otherwise false. */
bool bytestreamReadU16(ByteStream* stream, uint16_t* out);

/*! \brief uint8_t read from a byte stream
 *  \return True on success, otherwise false. */
bool bytestreamReadU8(ByteStream* stream, uint8_t* out);

/*! \brief read multiple uint8_t bytes from the byte stream.
    \return True on success, otherwise false. */
bool bytestreamReadN8(ByteStream* stream, uint8_t* out, int32_t numBytes);

/*! \brief Read variable length encoded uint64_t from a byte stream.
 *  \return True on success, otherwise false. */
bool bytestreamReadMultiByte(ByteStream* stream, uint64_t* out);

/*! \brief Forward direction only byte stream seek relative to current offset.
 *  \return True on success, otherwise false. */
bool bytestreamSeek(ByteStream* stream, size_t offset);

/*! \brief Get the pointer to the current stream location.
 *  \return Pointer to location within the stream, or NULL upon error. */
const uint8_t* bytestreamCurrent(const ByteStream* stream);

/*! \brief Retrieve the remaining number of bytes to be read.
 *  \return Number of bytes remaining in the stream, or 0 upon error */
static inline size_t bytestreamRemaining(const ByteStream* stream)
{
    if (stream->offset > stream->size) {
        return 0;
    }

    return stream->size - stream->offset;
}

/*! \brief Retrieve the byte size of the overall stream.
    \return Number of bytes the stream was initialized with. */
size_t byteStreamGetSize(const ByteStream* stream);

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

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_BYTESTREAM_H
