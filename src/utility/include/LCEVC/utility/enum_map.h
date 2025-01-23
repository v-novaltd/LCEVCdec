/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
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

#ifndef VN_API_ENUM_MAP_H_
#define VN_API_ENUM_MAP_H_

#include "lcevc_config.h"

#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lcevc_dec::utility {

// Simple map from enums to strings. This is just a wrapper around a variable-length array of enum-
// string pairs. Use this if you don't have a "Count" or "Max" enum value to set the length.
template <typename E>
using EnumMap = std::pair<E, const char*>[];

// Strictly sized map from enums to strings. Use this whenever you have a "Count" or "Max" enum
// value, because it allows you to catch missing enum values at compile-time. You can also provide
// as many synonyms as you like for any enum value, so long as you provide a large enough capacity.
// If too MANY values are provided, you'll get an index-out-of-bounds complaint. To catch too FEW
// values, simply add a static assert.
//
// Usage:
//
// static constexpr EnumMapArr<EnumType, EnumType::Count> kEnumMap{
//      EnumType::A, "A",
//      EnumType::B, "B",
// };
// static_assert(kEnumMap.enums == kEnumMap.numEnums(), "Too few entries");
//
// static constexpr EnumMapArr<EnumType, EnumType::Count, EnumType::Count * 2> kEnumMapWithSynonyms{
//      EnumType::One, "One",
//      EnumType::Two, "Two",
//      EnumType::Two, "2",
//      EnumType::One, "1",
// };
// static_assert(!kEnumMap.isMissingEnums(), "Some enum is missing");
// // optional:
// static_assert(!kEnumMapWithSynonyms.isOversized(), "Capacity is higher than necessary");

// How does this work? This is an array of enum-string pairs. While it would be possible to
// achieve this with a SINGLE constructor, the caller of that constructor would have to wrap
// each argument in "std::pair<myEnumType, const char*>(...)" which could be ugly and
// potentially bug-prone. Therefore, this function uses a PAIR of constructors that recurse, at
// compile-time, in pairs, tracking the number of pairs as they go. For instance, if you made
// an EnumMapArr for `enum E{A,B,Count};` the constructor would theoretically compile to something
// like this:
//
// EnumMapArr<E,E::Count>(E::A, "A", E::B, "B") =
// EnumMapArr<E,E::Count>(0, E::A, "A", E::B, "B") =
//{
//    {
//        {
//            size = 0+1+1; // Base case constructor
//        }
//        m_pairs[0+1].second = "B"; // Case 1 constructor, level 1
//        m_pairs[0+1].first = E::B; // Case 0 constructor, level 1
//    }
//    m_pairs[0].second = "A"; // Case 1 constructor, level 0
//    m_pairs[0].first = E::A; // Case 0 constructor, level 0
//}
template <typename E, size_t NumEnums, size_t Len = NumEnums>
class EnumMapArr
{
    static_assert(std::is_enum_v<E>);
    using EPair = std::pair<E, const char*>;
    using EArr = EPair[Len];

    constexpr bool contains(E enm, size_t len)
    {
        for (auto iter = m_pairs; iter < &(m_pairs[len]); iter++) {
            if (iter->first == enm) {
                return true;
            }
        }
        return false;
    }

public:
    // Base-case Constructor
    explicit constexpr EnumMapArr(const size_t idx)
        : m_size(idx)
    {}

    // Recursive case 2 constructor
    template <typename... Ts>
    constexpr EnumMapArr(const size_t idx, E firstVal, Ts... vals) // NOLINT
        : EnumMapArr(idx, vals...)
    {
        m_enums += (contains(firstVal, idx) ? 0 : 1);
        m_pairs[idx].first = firstVal;
    }

    // Recursive case 1 constructor
    template <typename... Ts>
    constexpr EnumMapArr(const size_t idx, const char* nextVal, Ts... vals) // NOLINT
        : EnumMapArr((idx + 1), vals...)
    {
        m_pairs[idx].second = nextVal;
    }

    // Entry point to the recursive constructors
    template <typename... Ts>
    constexpr EnumMapArr(E firstVal, Ts... vals) // NOLINT
        : EnumMapArr(0, firstVal, vals...)
    {}

    constexpr bool isMissingEnums() const { return m_enums < NumEnums; }
    constexpr bool isOversized() const { return m_size < Len; }

    const EPair& operator[](size_t idx) const { return m_pairs[idx]; }

private:
    size_t m_enums = 0;
    size_t m_size;
    EArr m_pairs;
};

#if VN_COMPILER(MSVC)
#define strcasecmp _stricmp
#endif

template <typename E, size_t NumEnums, size_t Len>
inline std::string_view enumToString(const EnumMapArr<E, NumEnums, Len>& map, E enm)
{
    for (auto i = 0u; i < Len; ++i) {
        if (map[i].first == enm) {
            return map[i].second;
        }
    }
    return {};
}

template <typename E, size_t NumEnums, size_t Len>
inline bool enumFromString(const EnumMapArr<E, NumEnums, Len>& map, std::string_view str,
                           E defaultValue, E& out)
{
    for (auto i = 0u; i < Len; ++i) {
        if (!strcasecmp(map[i].second, str.data())) {
            out = map[i].first;
            return true;
        }
    }

    out = defaultValue;
    return false;
}

template <typename E, size_t N>
inline std::string_view enumToString(const std::pair<E, const char*> (&map)[N], E enm)
{
    for (auto i = 0u; i < N; ++i) {
        if (map[i].first == enm) {
            return map[i].second;
        }
    }
    return {};
}

template <typename E, size_t N>
inline bool enumFromString(const std::pair<E, const char*> (&map)[N], std::string_view str,
                           E defaultValue, E& out)
{
    for (auto i = 0u; i < N; ++i) {
        if (!strcasecmp(map[i].second, str.data())) {
            out = map[i].first;
            return true;
        }
    }

    out = defaultValue;
    return false;
}

} // namespace lcevc_dec::utility

#endif // VN_API_ENUM_MAP_H_
