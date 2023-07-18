/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#pragma once

#define __STDC_LIMIT_MACROS

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#ifdef _WIN32
#if _MSC_VER <= 1700

#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId64
#define PRId64 "ld"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#else
#include <inttypes.h>
#endif
#else
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId64
#define PRId64 "ld"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIu64
#define PRIu64 "lu"
#endif
#endif

namespace lcevc_dec { namespace utility { // NOLINT(modernize-concat-nested-namespaces)

    // - Using declarations -----------------------------------------------------------------------

    using DataBuffer = std::vector<uint8_t>;

    // - Callback ---------------------------------------------------------------------------------

    template <typename T>
    class Callback
    {
    public:
        Callback()
            : m_callback(nullptr)
            , m_data(nullptr){};
        T m_callback;
        void* m_data;
    };

    // - Margins ----------------------------------------------------------------------------------

    struct Margins
    {
        uint32_t left;
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
    };

}} // namespace lcevc_dec::utility
