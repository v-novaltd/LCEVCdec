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
#define VN_EXPECT_DEATH(expression, message, outcome) EXPECT_DEATH(expression, "")
#endif

// Helper types and consts ------------------------------------------------------------------------

using EnhancementWithData = std::pair<const uint8_t*, uint32_t>;
using EventCountArr = std::array<std::atomic<uint32_t>, LCEVC_EventCount>;
using SmartBuffer = std::shared_ptr<std::vector<uint8_t>>;

constexpr uint32_t kI420NumPlanes = 3; // i.e. Y, U, and V
constexpr uint32_t kNV12NumPlanes = 2; // i.e. Y and interleaved UV

static const uint32_t kMaxNumPlanes = lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes;

static const std::vector<int32_t> kAllEvents = {
    LCEVC_Log,
    LCEVC_Exit,
    LCEVC_CanSendBase,
    LCEVC_CanSendEnhancement,
    LCEVC_CanSendPicture,
    LCEVC_CanReceive,
    LCEVC_BasePictureDone,
    LCEVC_OutputPictureDone,
};

// Helper functions -------------------------------------------------------------------------------

// Helper for using the default stream data:
EnhancementWithData getEnhancement(int64_t pts, const std::vector<uint8_t> kValidEnhancements[3]);

// Helpers for threading:

static inline bool equal(std::atomic<uint32_t>& lhs, uint32_t rhs) { return lhs == rhs; }
static inline bool greaterThan(std::atomic<uint32_t>& lhs, uint32_t rhs) { return lhs > rhs; }

// These are essentially an implementation of the atomic "wait" function (with a timeout), but
// std::atomic::wait is only available in C++20.
template <typename Fn, typename... Args>
static bool atomicWaitUntilTimeout(bool& wasTimeout, std::chrono::milliseconds manualTimeout,
                                   Fn pred, Args&&... args)
{
    // Note that we don't do any checks inside this function: if you do ASSERT_TRUE in here, it
    // only quits the function, not the test.
    const std::chrono::milliseconds waitIncrement(1);
    std::chrono::time_point curTime = std::chrono::high_resolution_clock::now();
    const std::chrono::time_point waitEnd = curTime + manualTimeout;
    while ((curTime < waitEnd) && (!pred(args...))) {
        std::this_thread::sleep_for(waitIncrement);
        curTime = std::chrono::high_resolution_clock::now();
    }
    wasTimeout = (curTime >= waitEnd);
    return pred(args...);
}

template <typename Fn, typename... Args>
static bool atomicWaitUntil(bool& wasTimeout, Fn pred, Args&&... args)
{
    // In tests, 3ms was enough, but 50ms is not enough on a debug build with coverage enabled, so
    // we use 200ms.
    return atomicWaitUntilTimeout(wasTimeout, std::chrono::milliseconds(200), pred, args...);
}

// Helpers for PictureExternal:

void setupPictureExternal(LCEVC_PictureBufferDesc& bufferDescOut, SmartBuffer& bufferOut,
                          LCEVC_PicturePlaneDesc planeDescArrOut[kMaxNumPlanes],
                          LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                          LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access);

#endif // VN_API_TEST_UNIT_UTILS_H_
