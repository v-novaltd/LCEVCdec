// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
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

//// Utility functions to read and write multibyte integer types in contorlled endian orders
//
template <typename T>
bool readBigEndian(std::istream& stream, T& val)
{
    T tmp;
    if (stream.read(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        val = swapByteOrder(tmp);
#else
        val = tmp;
#endif
        return true;
    }

    return false;
}

template <typename T>
bool readLittleEndian(std::istream& stream, T& val)
{
    T tmp;
    if (stream.read(static_cast<char*>(static_cast<void*>(&tmp)), sizeof(T))) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        val = tmp;
#else
        val = swapByteOrder(tmp);
#endif
        return true;
    }

    return false;
}

} // namespace lcevc_dec::utility
#endif
