/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_HANDLE_H_
#define VN_API_HANDLE_H_

#include <mutex>
#include <type_traits>
#include <variant>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

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
    Handle(uintptr_t newH)
        : handle(newH)
    {}

    // You can convert between handles precisely when you would be able to convert between pointers
    template <typename TFrom>
    Handle(const Handle<TFrom>& otherH)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
        handle = otherH.handle;
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
        *this = other;
    }
    template <typename TFrom>
    Handle<T>& operator=(Handle<TFrom>&& other)
    {
        static_assert(std::is_convertible<TFrom*, T*>::value);
        return *this = other;
    }

    bool operator==(uintptr_t rawHandle) const { return handle == rawHandle; }
    bool operator!=(uintptr_t rawHandle) const { return handle != rawHandle; }
    bool operator==(Handle<T> other) const { return handle == other.handle; }
    bool operator!=(Handle<T> other) const { return handle != other.handle; }

    uintptr_t handle;
};

// UINTPTR_MAX is a good choice for an invalid handle, because the "index" component of it will be
// much larger than the maximum capacity of any of our pools (we assert in Pool to guarantee this).
static const uintptr_t kInvalidHandle = UINTPTR_MAX;

} // namespace lcevc_dec::decoder

#endif // VN_API_HANDLE_H_
