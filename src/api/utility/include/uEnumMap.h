/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#pragma once

#include "uPlatform.h"
#include "uString.h"
#include "uTypeTraits.h"

#include <array>
#include <utility>
#include <vector>

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace lcevc_dec { namespace utility {

    // Flexibly sized map from enums to strings. This is suitable for enums where not every value
    // has an enum entry, or where the last entry might change. For example, this class would be
    // used for an enum which reserves spaces in the middle for future additions.
    template <typename E>
    class EnumMapVec
    {
        VNStaticAssert(IsEnum<E>::Value);

    public:
        EnumMapVec(E value, const char* name) { m_pairs.push_back(EnumStringPair(value, name)); }

        EnumMapVec& operator()(E value, const char* name)
        {
            m_pairs.push_back(EnumStringPair(value, name));
            return *this;
        }

        bool findEnum(E& res, const std::string& name, E failedReturn) const
        {
            for (typename EnumStringVec::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it) {
                if (string::IEquals((*it).name, name)) {
                    res = (*it).value;
                    return true;
                }
            }

            res = failedReturn;
            return false;
        }

        bool findName(const char** res, E value, const char* failedReturn) const
        {
            for (typename EnumStringVec::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it) {
                if ((*it).value == value) {
                    *res = (*it).name.c_str();
                    return true;
                }
            }

            *res = failedReturn;
            return false;
        }

    private:
        struct EnumStringPair
        {
            EnumStringPair(E valueIn, const char* nameIn)
                : value(valueIn)
                , name(nameIn)
            {}
            E value;
            const std::string name;
        };

        using EnumStringVec = std::vector<EnumStringPair>;

        EnumStringVec m_pairs;
    };

    // Strictly sized map from enums to strings. This is suitable for enums with no gaps, and a
    // known final value. To use this class, construct it with an argument list which alternates
    // between enums and strings. Then on the line below, you can static_assert that size equals
    // capacity() to check that every enum is accounted for.
    //
    // How does this work? This is an array of enum-string pairs. While it would be possible to
    // achieve this with a SINGLE constructor, the caller of that constructor would have to wrap
    // each argument in "std::pair<myEnumType, const char*>(...)" which could be ugly and
    // potentially bug-prone. Therefore, this function uses a PAIR of constructors that recurse, at
    // compile-time, in pairs, tracking the number of pairs as they go. For instance, if you made
    // an EnumMapArr for `enum E{A,B,Count};` the constructor would theoretically compile something
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

    template <typename E, size_t Len>
    class EnumMapArr
    {
        VNStaticAssert(IsEnum<E>::Value);
        using EPair = std::pair<E, const char*>;
        using EArr = std::array<EPair, Len>;

    public:
        // Base-case Constructor
        constexpr EnumMapArr(size_t idx)
            : size(idx)
        {}

        // Recursive case 2 constructor
        template <typename... Ts>
        constexpr EnumMapArr(size_t idx, E firstVal, Ts... vals) // NOLINT
            : EnumMapArr(idx, vals...)
        {
            m_pairs[idx].first = firstVal;
        }

        // Recursive case 1 constructor
        template <typename... Ts>
        constexpr EnumMapArr(size_t idx, const char* nextVal, Ts... vals) // NOLINT
            : EnumMapArr((idx + 1), vals...)
        {
            m_pairs[idx].second = nextVal;
        }

        // Entry point to the recursive constructors
        template <typename... Ts>
        constexpr EnumMapArr(E firstVal, Ts... vals) // NOLINT
            : EnumMapArr(0, firstVal, vals...)
        {}

        static constexpr size_t capacity() { return Len; }

        bool findEnum(E& res, const std::string& name, E failedReturn) const
        {
            for (typename EArr::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it) {
                if (string::IEquals((*it).second, name)) {
                    res = (*it).first;
                    return true;
                }
            }

            res = failedReturn;
            return false;
        }

        bool findName(const char** res, E value, const char* failedReturn) const
        {
            const char* name = operator[](value);
            if (name == nullptr) {
                *res = failedReturn;
                return false;
            }

            *res = name;
            return true;
        }

        const char* operator[](E value) const
        {
            for (typename EArr::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it) {
                if ((*it).first == value) {
                    return (*it).second;
                }
            }
            return nullptr;
        }

        const size_t size;

    private:
        EArr m_pairs;
    };

    template <typename T>
    static const char* ToString2Helper(bool (*ToStringFn)(const char**, T), T val)
    {
        const char* res;
        ToStringFn(&res, val);
        return res;
    }

    template <typename T>
    static T FromString2Helper(bool (*FromStringFn)(T&, const std::string&), const std::string val)
    {
        T res;
        FromStringFn(res, val);
        return res;
    }
}} // namespace lcevc_dec::utility