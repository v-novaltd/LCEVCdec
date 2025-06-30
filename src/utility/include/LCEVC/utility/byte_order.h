/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

// Cross platform byte swapping, host<->network, and reading specific endianness from streams
//
#ifndef VN_LCEVC_UTILITY_BYTE_ORDER_H
#define VN_LCEVC_UTILITY_BYTE_ORDER_H

#include <cstdint>
#include <istream>

// Everything BUT MSVC defines common byte order macros
//
// Assumes that everything that MSVC targets is little endian.
//
#if defined(_MSC_VER)
#include <intrin.h>
#define __ORDER_LITTLE_ENDIAN__ 0
#define __ORDER_BIG_ENDIAN__ 1
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

namespace lcevc_dec::utility {

template <typename T>
struct ByteOrder
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    static inline T ToNetwork(T val) { return Reverse(val); }
    static inline T ToHost(T val) { return Reverse(val); }
#else
    static inline T ToNetwork(T val) { return val; }
    static inline T ToHost(T val) { return val; }
#endif
    static inline T Reverse(T val);
};

#if defined(_MSC_VER)
template <>
inline uint64_t ByteOrder<uint64_t>::Reverse(uint64_t val)
{
    return _byteswap_uint64(val);
}
template <>
inline int64_t ByteOrder<int64_t>::Reverse(int64_t val)
{
    return (int64_t)_byteswap_uint64((uint64_t)val);
}
template <>
inline uint32_t ByteOrder<uint32_t>::Reverse(uint32_t val)
{
    return _byteswap_ulong(val);
}
template <>
inline int32_t ByteOrder<int32_t>::Reverse(int32_t val)
{
    return (int32_t)_byteswap_ulong((uint32_t)val);
}
template <>
inline uint16_t ByteOrder<uint16_t>::Reverse(uint16_t val)
{
    return _byteswap_ushort(val);
}
template <>
inline int16_t ByteOrder<int16_t>::Reverse(int16_t val)
{
    return (int16_t)_byteswap_ushort((uint16_t)val);
}
#else
template <>
inline uint64_t ByteOrder<uint64_t>::Reverse(uint64_t val)
{
    return __builtin_bswap64(val);
}
template <>
inline int64_t ByteOrder<int64_t>::Reverse(int64_t val)
{
    return (int64_t)__builtin_bswap64((uint64_t)val);
}
template <>
inline uint32_t ByteOrder<uint32_t>::Reverse(uint32_t val)
{
    return __builtin_bswap32(val);
}
template <>
inline int32_t ByteOrder<int32_t>::Reverse(int32_t val)
{
    return (int32_t)__builtin_bswap32((uint32_t)val);
}
template <>
inline uint16_t ByteOrder<uint16_t>::Reverse(uint16_t val)
{
    return __builtin_bswap16(val);
}
template <>
inline int16_t ByteOrder<int16_t>::Reverse(int16_t val)
{
    return (int16_t)__builtin_bswap16((uint16_t)val);
}
#endif

template <>
inline uint8_t ByteOrder<uint8_t>::Reverse(uint8_t val)
{
    return val;
}

template <>
inline int8_t ByteOrder<int8_t>::Reverse(int8_t val)
{
    return val;
}

// Byte swapping templated on the integer types
//
template <typename T>
T swapByteOrder(T val);

template <>
inline uint64_t swapByteOrder<uint64_t>(uint64_t val)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(val);
#else
    return __builtin_bswap64(val);
#endif
}

template <>
inline int64_t swapByteOrder<int64_t>(int64_t val)
{
#if defined(_MSC_VER)
    return static_cast<int64_t>(_byteswap_uint64(static_cast<uint64_t>(val)));
#else
    return static_cast<int64_t>(__builtin_bswap64(static_cast<uint64_t>(val)));
#endif
}

template <>
inline uint32_t swapByteOrder<uint32_t>(uint32_t val)
{
#if defined(_MSC_VER)
    return _byteswap_ulong(val);
#else
    return __builtin_bswap32(val);
#endif
}

template <>
inline int32_t swapByteOrder<int32_t>(int32_t val)
{
#if defined(_MSC_VER)
    return static_cast<int32_t>(_byteswap_ulong(static_cast<uint32_t>(val)));
#else
    return static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(val)));
#endif
}

template <>
inline uint16_t swapByteOrder<uint16_t>(uint16_t val)
{
#if defined(_MSC_VER)
    return _byteswap_ushort(val);
#else
    return __builtin_bswap16(val);
#endif
}

template <>
inline int16_t swapByteOrder<int16_t>(int16_t val)
{
#if defined(_MSC_VER)
    return static_cast<int16_t>(_byteswap_ushort(static_cast<uint16_t>(val)));
#else
    return static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(val)));
#endif
}

template <>
inline uint8_t swapByteOrder<uint8_t>(uint8_t val)
{
    return val;
}

template <>
inline int8_t swapByteOrder<int8_t>(int8_t val)
{
    return val;
}

// Utility functions to convert network<->host order
//
template <typename T>
inline T toNetwork(T val)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return swapByteOrder(val);
#else
    return val;
#endif
}

template <typename T>
inline T toHost(T val)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return swapByteOrder(val);
#else
    return val;
#endif
}

//// Utility functions to read and write multibyte integer types in controlled endian orders
//
template <typename T>
bool readBigEndian(std::istream& stream, T& val)
{
    T tmp;
    if (!stream.read(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
        return false;
    }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    val = swapByteOrder(tmp);
#else
    val = tmp;
#endif
    return true;
}

template <typename T>
bool readLittleEndian(std::istream& stream, T& val)
{
    T tmp;
    if (!stream.read(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
        return false;
    }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    val = tmp;
#else
    val = swapByteOrder(tmp);
#endif
    return true;
}

template <typename T>
bool writeBigEndian(std::ostream& stream, T val)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (T tmp = swapByteOrder(val);
        !stream.write(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
        return false;
    }
#else
    if (!stream.write(static_cast<char*>(static_cast<void*>(&val)), sizeof(T))) {
        return false;
    }
#endif

    return true;
}

template <typename T>
bool writeLittleEndian(std::ostream& stream, T val)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    if (T tmp = swapByteOrder(val);
        !stream.write(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
        return false;
    }
#else
    if (!stream.write(static_cast<char*>(static_cast<void*>(&val)), sizeof(T))) {
        return false;
    }
#endif

    return true;
}

} // namespace lcevc_dec::utility
#endif // VN_LCEVC_UTILITY_BYTE_ORDER_H
