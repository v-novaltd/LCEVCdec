/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_CLOCK_H_
#define VN_API_CLOCK_H_

#include <uChrono.h>

namespace lcevc_dec::decoder {
class Clock
{
public:
    Clock()
        : m_creationTime(lcevc_dec::utility::GetTime<lcevc_dec::utility::MicroSecond>())
    {}

    VNInline uint64_t getTimeSinceStart() const
    {
        return lcevc_dec::utility::GetTime<lcevc_dec::utility::MicroSecond>() - m_creationTime;
    }

private:
    const uint64_t m_creationTime;
};
} // namespace lcevc_dec::decoder

#endif // VN_API_CLOCK_H_
