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

#ifndef VN_LCEVC_API_HANDLE_H
#define VN_LCEVC_API_HANDLE_H

#include <algorithm>
#include <cstdint>
#include <type_traits>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// UINTPTR_MAX is a good choice for an invalid handle, because the "index" component of it will be
// much larger than the maximum capacity of any of our pools (we assert in Pool to guarantee this).
static const uintptr_t kInvalidHandle = UINTPTR_MAX;

// A Handle is an index, bitwise-or'd with a generation. If a generation is odd, it means an object
// with that index is currently out there (i.e. allocated). An even generation means "not currently
// allocated" (for instance, a generation of 4 means the index got allocated and released twice).
//
// Also, out of mistrust for future coders, we give Handles type-protections akin to those of
// pointers/references.
//
// Possible improvement: we may want to extend this analogy by making reference-style handles (i.e.
// never null, and not reassignable) and pointer-style handles. Until then, one quick workaround is
// to name handles something like "nonNullHandle" when you've already null-checked them.
template <typename T>
struct Handle
{
    Handle() = delete;
    constexpr Handle(uintptr_t newH)
        : handle(newH)
    {}

    // You can convert between handles precisely when you would be able to convert between pointers
    template <typename TFrom>
    Handle(const Handle<TFrom>& otherH)
        : handle(otherH.handle)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
    }
    template <typename TFrom>
    Handle<T>& operator=(const Handle<TFrom>& other)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
        if (this != &other) {
            *this = other;
        }
        return *this;
    }

    template <typename TFrom>
    Handle(Handle<TFrom>&& other)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
        *this = std::move(other);
    }
    template <typename TFrom>
    Handle<T>& operator=(Handle<TFrom>&& other)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
        *this = std::move(other);
        return *this;
    }

    bool operator==(uintptr_t rawHandle) const { return handle == rawHandle; }
    bool operator!=(uintptr_t rawHandle) const { return handle != rawHandle; }
    bool operator==(Handle<T> other) const { return handle == other.handle; }
    bool operator!=(Handle<T> other) const { return handle != other.handle; }

    bool isValid() const { return handle != kInvalidHandle; }
    uintptr_t handle = 0;
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_HANDLE_H
