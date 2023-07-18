/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_UTIL_TIMESTAMPS_H_
#define VN_UTIL_TIMESTAMPS_H_

#include "uTypes.h"

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec { namespace utility { // NOLINT(modernize-concat-nested-namespaces)

    // - Constants --------------------------------------------------------------------------------

    constexpr uint64_t kInvalidTimehandle = UINT64_MAX;

    // - StampedBuffer ----------------------------------------------------------------------------

    // Struct containing a buffer and timehandle data.
    struct StampedBuffer
    {
        lcevc_dec::utility::DataBuffer buffer = {};
        uint64_t timehandle = kInvalidTimehandle;
        double inputTime = 0.0;
        double startTime = 0.0;
    };

    // - Timehandle manipulation ------------------------------------------------------------------

    // timehandle is a uint64_t number consisting of:
    // MSB uint16_t cc
    // LSB  int48_t timestamp
    // Note: the following four functions are endian independent
    VNInline uint64_t getTimehandle(const uint16_t cc, const int64_t timestamp)
    {
        return ((uint64_t)cc << 48) | (timestamp & 0x0000FFFFFFFFFFFF);
    }

    VNInline uint16_t timehandleGetCC(const uint64_t handle) { return (uint16_t)(handle >> 48); }

    VNInline int64_t timehandleGetTimestamp(const uint64_t handle)
    {
        return (int64_t)(handle << 16) >> 16;
    }

    VNInline bool timestampIsValid(const int64_t timestamp)
    {
        return (timestamp & 0x0000FFFFFFFFFFFF) == timestamp;
    }

}} // namespace lcevc_dec::utility

#endif // VN_UTIL_TIMESTAMPS_H_
