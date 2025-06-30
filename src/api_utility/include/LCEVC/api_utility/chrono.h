/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_LCEVC_API_UTILITY_CHRONO_H
#define VN_LCEVC_API_UTILITY_CHRONO_H

#include <chrono>
#include <cstdint>
#include <ratio>
#include <string>

namespace lcevc_dec::utility {

using NanoSecond = std::chrono::duration<int64_t, std::nano>;
using MicroSecond = std::chrono::duration<int64_t, std::micro>;
using MilliSecond = std::chrono::duration<int64_t, std::milli>;

using Seconds = std::chrono::duration<double>;

using NanoSecondF64 = std::chrono::duration<double, std::nano>;
using MicroSecondF64 = std::chrono::duration<double, std::micro>;
using MilliSecondF64 = std::chrono::duration<double, std::milli>;

using TimePoint = std::chrono::high_resolution_clock::time_point;

inline TimePoint getTimePoint() { return std::chrono::high_resolution_clock::now(); }

template <typename DurationType>
inline typename DurationType::rep getTime()
{
    const TimePoint now = getTimePoint();
    DurationType ticks = std::chrono::duration_cast<DurationType>(now.time_since_epoch());
    return ticks.count();
}

template <typename DurationType>
inline typename DurationType::rep getTimeSincePoint(const TimePoint& tp)
{
    const TimePoint now = getTimePoint();
    DurationType ticks = std::chrono::duration_cast<DurationType>(now - tp);
    return ticks.count();
}

template <typename DurationType>
inline typename DurationType::rep getTimeBetweenPoints(const TimePoint& start, const TimePoint& end)
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
    using Type = typename DurationType::rep;

    void Start()
    {
        m_start = getTimePoint();
        m_running = true;
    }

    Type Stop()
    {
        m_running = false;
        return GetElapsedTime();
    }

    Type Restart()
    {
        Type res = Stop();
        Start();
        return res;
    }

    Type GetElapsedTime() { return getTimeSincePoint<DurationType>(m_start); }

    bool IsRunning() const { return m_running; }

private:
    TimePoint m_start;
    bool m_running = false;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Helper class for performing timing within a scope. It stores the result at the (optional) memory
// location provided by the user.
template <typename DurationType>
class ScopedTimer
{
public:
    using Type = typename DurationType::rep;

    explicit ScopedTimer(Type* output = nullptr)
        : m_output(output)
    {}

    ~ScopedTimer()
    {
        if (m_output) {
            *m_output = getElapsedTime();
        }
    }

    // Not copyable or movable, due to const member variables
    ScopedTimer(const ScopedTimer& other) = delete;
    ScopedTimer(ScopedTimer&& other) = delete;
    ScopedTimer& operator=(const ScopedTimer& other) = delete;
    ScopedTimer& operator=(ScopedTimer&& other) noexcept = delete;

    // Convenience function that provides the elapsed time since the call to
    // the constructor of this object.
    Type getElapsedTime() const { return getTimeSincePoint<DurationType>(m_start); }

private:
    const TimePoint m_start = getTimePoint();
    Type* const m_output;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename DurationType, uint32_t SampleSize = 10>
class AverageTimer
{
public:
    typedef typename DurationType::rep Type;

    AverageTimer()
        : m_nextIndex(0)
        , m_totalIndex(0)
        , m_total(Type(0))
        , m_average(Type(0))
        , m_lastDur(Type(0))
    {
        for (uint32_t i = 0; i < SampleSize; ++i)
            m_samples[i] = 0;
    }

    void Start()
    {
        m_start = getTimePoint();
        m_running = true;
    }

    Type Stop()
    {
        Type duration = getTimeSincePoint<DurationType>(m_start);
        m_total += (duration - m_samples[m_nextIndex]);
        m_samples[m_nextIndex] = duration;
        m_nextIndex = (m_nextIndex + 1) % SampleSize;
        m_totalIndex = m_totalIndex + 1;
        m_totalIndex = m_totalIndex < SampleSize ? m_totalIndex : SampleSize;
        m_average = m_total / m_totalIndex;
        m_lastDur = duration;
        m_running = false;
        return duration;
    }

    Type GetAverageTime() const { return m_average; }

    Type GetLastDuration() const { return m_lastDur; }

    bool IsRunning() const { return m_running; }

private:
    TimePoint m_start;
    uint32_t m_nextIndex;
    uint32_t m_totalIndex;
    Type m_samples[SampleSize];
    Type m_total;
    Type m_average;
    Type m_lastDur;
    bool m_running = false;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
} // namespace lcevc_dec::utility

#endif // VN_LCEVC_API_UTILITY_CHRONO_H
