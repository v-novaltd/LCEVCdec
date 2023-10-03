/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#pragma once

#include "uTypes.h"

#include <chrono>

namespace lcevc_dec::api_utility {
using NanoSecond = std::chrono::duration<int64_t, std::nano>;
using MicroSecond = std::chrono::duration<int64_t, std::micro>;
using MilliSecond = std::chrono::duration<int64_t, std::milli>;

using Seconds = std::chrono::duration<double>;

using NanoSecondF64 = std::chrono::duration<double, std::nano>;
using MicroSecondF64 = std::chrono::duration<double, std::micro>;
using MilliSecondF64 = std::chrono::duration<double, std::milli>;

using TimePoint = std::chrono::high_resolution_clock::time_point;

VNInline TimePoint GetTimePoint() { return std::chrono::high_resolution_clock::now(); }

template <typename DurationType>
VNInline typename DurationType::rep GetTime()
{
    TimePoint now = GetTimePoint();
    DurationType ticks = std::chrono::duration_cast<DurationType>(now.time_since_epoch());
    return ticks.count();
}

template <typename DurationType>
VNInline typename DurationType::rep GetTimeSincePoint(TimePoint& tp)
{
    TimePoint now = GetTimePoint();
    DurationType ticks = std::chrono::duration_cast<DurationType>(now - tp);
    return ticks.count();
}

template <typename DurationType>
VNInline typename DurationType::rep GetTimeBetweenPoints(const TimePoint& start, const TimePoint& end)
{
    return std::chrono::duration_cast<DurationType>(end - start).count();
}

// Helper function for formatting the supplied buffer with a data/time string.
// use UTC because it saves any confusion during the daylight saving switch.
// it's also simpler to always think in UTC time rather than whatever local time happens to be in.
void FormatBufferWithUTCDateTime(char* buffer, size_t bufferSize, time_t time_value);
void FormatBufferWithUTCDateTime(char* buffer, size_t bufferSize);
std::string UTCDateTimeString(time_t time_value);
std::string UTCDateTimeString();

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename DurationType>
class Timer
{
public:
    typedef typename DurationType::rep Type;

    VNInline Timer()
        : m_bRunning(false)
    {}

    VNInline void Start()
    {
        m_start = GetTimePoint();
        m_bRunning = true;
    }

    VNInline Type Stop()
    {
        m_bRunning = false;
        return GetElapsedTime();
    }

    VNInline Type Restart()
    {
        Type res = Stop();
        Start();
        return res;
    }

    VNInline Type GetElapsedTime() { return GetTimeSincePoint<DurationType>(m_start); }

    VNInline bool IsRunning() const { return m_bRunning; }

private:
    TimePoint m_start;
    bool m_bRunning;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Helper class for performing timing within a scope. It stores the result on
// a memory location provided by the user, the type of the location needs to
// match the duration types representation type.
template <typename DurationType>
class ScopedTimer
{
public:
    typedef typename DurationType::rep Type;

    VNInline ScopedTimer(Type* output)
        : m_output(output)
    {
        m_start = GetTimePoint();
    }

    ~ScopedTimer()
    {
        if (m_output)
            *m_output = GetElapsedTime();
    }

    // Convenience function that provides the elapsed time since the call to
    // the constructor of this object.
    VNInline Type GetElapsedTime() { return GetTimeSincePoint<DurationType>(m_start); }

private:
    TimePoint m_start;
    Type* m_output;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename DurationType, uint32_t SampleSize = 10>
class AverageTimer
{
public:
    typedef typename DurationType::rep Type;

    VNInline AverageTimer()
        : m_nextIndex(0)
        , m_totalIndex(0)
        , m_total(Type(0))
        , m_average(Type(0))
        , m_lastDur(Type(0))
        , m_bRunning(false)
    {
        for (uint32_t i = 0; i < SampleSize; ++i)
            m_samples[i] = 0;
    }

    VNInline void Start()
    {
        m_start = GetTimePoint();
        m_bRunning = true;
    }

    VNInline Type Stop()
    {
        Type duration = GetTimeSincePoint<DurationType>(m_start);
        m_total += (duration - m_samples[m_nextIndex]);
        m_samples[m_nextIndex] = duration;
        m_nextIndex = (m_nextIndex + 1) % SampleSize;
        m_totalIndex = m_totalIndex + 1;
        m_totalIndex = m_totalIndex < SampleSize ? m_totalIndex : SampleSize;
        m_average = m_total / m_totalIndex;
        m_lastDur = duration;
        m_bRunning = false;
        return duration;
    }

    VNInline Type GetAverageTime() const { return m_average; }

    VNInline Type GetLastDuration() const { return m_lastDur; }

    VNInline bool IsRunning() const { return m_bRunning; }

private:
    TimePoint m_start;
    uint32_t m_nextIndex;
    uint32_t m_totalIndex;
    Type m_samples[SampleSize];
    Type m_total;
    Type m_average;
    Type m_lastDur;
    bool m_bRunning;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
} // namespace lcevc_dec::api_utility