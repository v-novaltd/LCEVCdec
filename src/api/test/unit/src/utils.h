/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_TEST_UNIT_UTILS_H_
#define VN_API_TEST_UNIT_UTILS_H_

#include <LCEVC/lcevc_dec.h>
#include <gtest/gtest.h>
#include <uPictureFormatDesc.h>

#include <atomic>
#include <chrono>
#include <thread>

// Macros -----------------------------------------------------------------------------------------

// This allows us to test in builds where assertion-deaths are disabled (like Release), while
// confirming that the assertion does occur in other builds (like Debug).
#ifdef NDEBUG
#define VN_EXPECT_DEATH(expression, message, outcome) EXPECT_EQ(expression, outcome)
#else
#define VN_EXPECT_DEATH(expression, message, outcome) EXPECT_DEATH(expression, message)
#endif

// Helper types and consts ------------------------------------------------------------------------

using EventCountArr = std::array<std::atomic<uint32_t>, LCEVC_EventCount>;
using SmartBuffer = std::shared_ptr<std::vector<uint8_t>>;

constexpr uint32_t kI420NumPlanes = 3; // i.e. Y, U, and V
constexpr uint32_t kNV12NumPlanes = 2; // i.e. Y and interleaved UV

static const uint32_t kMaxNumPlanes = lcevc_dec::utility::PictureFormatDesc::kMaxNumPlanes;

// Helper functions -------------------------------------------------------------------------------

// Helpers for threading:

static inline bool equal(std::atomic<uint32_t>& lhs, uint32_t rhs) { return lhs == rhs; }
static inline bool greaterThan(std::atomic<uint32_t>& lhs, uint32_t rhs) { return lhs > rhs; }

template <typename Fn, typename... Args>
static bool atomicWaitUntil(bool& wasTimeout, Fn pred, Args&&... args)
{
    // Note that we don't do any checks inside this function: if you do ASSERT_TRUE in here, it
    // only quits the function, not the test.

    // This is essentially an implementation of the atomic "wait" function (plus a manual timeout),
    // but std::atomic::wait is only available in C++20.
    const std::chrono::milliseconds timeout(50); // in tests, 3ms was enough, so 50ms is plenty.
    const std::chrono::milliseconds waitIncrement(1);
    std::chrono::time_point curTime = std::chrono::high_resolution_clock::now();
    const std::chrono::time_point waitEnd = curTime + timeout;
    while ((curTime < waitEnd) && (!pred(args...))) {
        std::this_thread::sleep_for(waitIncrement);
        curTime = std::chrono::high_resolution_clock::now();
    }
    wasTimeout = (curTime >= waitEnd);
    return pred(args...);
}

// Helpers for PictureExternal:

void setupBuffers(std::vector<SmartBuffer>& buffersOut, LCEVC_ColorFormat format, uint32_t width,
                  uint32_t height);
void setupBufferDesc(LCEVC_PictureBufferDesc (&descArrOut)[kMaxNumPlanes],
                     const std::vector<SmartBuffer>& buffers,
                     LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access);

namespace lcevc_dec::decoder {
class PictureExternal;
}
bool initPictureExternalAndBuffers(lcevc_dec::decoder::PictureExternal& out, std::vector<SmartBuffer>& buffersOut,
                                   LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                                   LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access);

// To save some casting and in-place construction:
static inline bool initPictureExternalAndBuffers(lcevc_dec::decoder::PictureExternal& out,
                                          std::vector<SmartBuffer>& buffersOut, int32_t format,
                                          uint32_t width, uint32_t height,
                                          uintptr_t accelBufferHandle, int32_t access)
{
    return initPictureExternalAndBuffers(out, buffersOut, static_cast<LCEVC_ColorFormat>(format),
                                         width, height, LCEVC_AccelBufferHandle{accelBufferHandle},
                                         static_cast<LCEVC_Access>(access));
}

#endif // VN_API_TEST_UNIT_UTILS_H_
