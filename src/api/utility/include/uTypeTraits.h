/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#pragma once

#include "uPlatform.h"
#include "uTypes.h"

namespace lcevc_dec::api_utility {
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// IntegralConstant - used to generate a compile time value constant that can be used for
// conditions checking.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <class T, T v>
struct IntegralConstant
{
    static const T Value = v;
};
typedef IntegralConstant<bool, true> TrueType;
typedef IntegralConstant<bool, false> FalseType;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// IsIntegral - Compile time helper that indicates if a type is integral. These have been
// limited to the explicit types we utilise.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename T>
struct IsIntegral : public FalseType
{};
template <typename T>
struct IsIntegral<const T> : public IsIntegral<T>
{}; // Helper to strip const from T.
template <typename T>
struct IsIntegral<volatile T> : public IsIntegral<T>
{}; // Helper to strip volatile from T.
template <typename T>
struct IsIntegral<volatile const T> : public IsIntegral<T>
{}; // Helper to strip volatile const from T.
template <>
struct IsIntegral<uint8_t> : public TrueType
{};
template <>
struct IsIntegral<uint16_t> : public TrueType
{};
template <>
struct IsIntegral<uint32_t> : public TrueType
{};
template <>
struct IsIntegral<uint64_t> : public TrueType
{};
template <>
struct IsIntegral<int8_t> : public TrueType
{};
template <>
struct IsIntegral<int16_t> : public TrueType
{};
template <>
struct IsIntegral<int32_t> : public TrueType
{};
template <>
struct IsIntegral<int64_t> : public TrueType
{};
template <>
struct IsIntegral<bool> : public TrueType
{};
#if defined(__APPLE__)
// in AppleClang, long/unsigned long are separate types
template <>
struct IsIntegral<long> : public TrueType
{};
template <>
struct IsIntegral<unsigned long> : public TrueType
{};
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// IsFloat - Compile time helper that indicates if a type is floating pointer. These have been
// limited to the explicit types we utilise.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename T>
struct IsFloat : public FalseType
{};
template <typename T>
struct IsFloat<const T> : public IsFloat<T>
{}; // Helper to strip const from T.
template <typename T>
struct IsFloat<volatile T> : public IsFloat<T>
{}; // Helper to strip volatile from T.
template <typename T>
struct IsFloat<volatile const T> : public IsFloat<T>
{}; // Helper to strip volatile const from T.
template <>
struct IsFloat<float> : public TrueType
{};
template <>
struct IsFloat<double> : public TrueType
{};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// IsSigned - Compile time helper that indicates if a type is signed. These have been
// limited to the explicit types we utilise.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename T>
struct IsSigned : public FalseType
{};
template <typename T>
struct IsSigned<const T> : public IsSigned<T>
{}; // Helper to strip const from T.
template <typename T>
struct IsSigned<volatile T> : public IsSigned<T>
{}; // Helper to strip volatile from T.
template <typename T>
struct IsSigned<volatile const T> : public IsSigned<T>
{}; // Helper to strip volatile const from T.
template <>
struct IsSigned<uint8_t> : public FalseType
{};
template <>
struct IsSigned<uint16_t> : public FalseType
{};
template <>
struct IsSigned<uint32_t> : public FalseType
{};
template <>
struct IsSigned<uint64_t> : public FalseType
{};
template <>
struct IsSigned<int8_t> : public TrueType
{};
template <>
struct IsSigned<int16_t> : public TrueType
{};
template <>
struct IsSigned<int32_t> : public TrueType
{};
template <>
struct IsSigned<int64_t> : public TrueType
{};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// IsEnum - Compile time helper that indicates if a type is an enum.
// @note: Uses C++11 feature std::is_enum, disable if on an unsupporting compiler.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename T>
struct IsEnum : public IntegralConstant<typename std::is_enum<T>::value_type, std::is_enum<T>::value>
{};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Low bit mask - Generates an integer with the lowest N contiguous bits set of the supplied
// integer type.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename INTEGER_TYPE, uint32_t Bits>
struct LowBitMask
{
    VNStaticAssert(Bits <= (sizeof(INTEGER_TYPE) * 8));
    static const INTEGER_TYPE Value = (1 << Bits) - 1;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MinValue - Generates the minimum value represented by the lowest N contiguous bits,
// accounting for a signed bit when INTEGER_TYPE is signed.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename INTEGER_TYPE, uint32_t Bits>
struct MinValue
{
    static const INTEGER_TYPE Value =
        IsSigned<INTEGER_TYPE>::Value ? ~(LowBitMask<INTEGER_TYPE, Bits>::Value) : 0;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MaxValue - Generates the maximum value represented by the lowest N contiguous bits.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename INTEGER_TYPE, uint32_t Bits>
struct MaxValue
{
    static const INTEGER_TYPE Value = LowBitMask<INTEGER_TYPE, Bits>::Value;
};
} // namespace lcevc_dec::api_utility
