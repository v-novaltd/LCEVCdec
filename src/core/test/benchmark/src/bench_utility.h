/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

#ifndef VN_DEC_BENCHMARK_UTILITY_H_
#define VN_DEC_BENCHMARK_UTILITY_H_

#include <cinttypes>

extern "C"
{
#include "common/types.h"
}

// -----------------------------------------------------------------------------

class Dimensions
{
public:
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;

    Dimensions Downscale(bool bHorizontal = true, bool bVertical = true) const
    {
        return Dimensions{bHorizontal ? (width + 1) >> 1 : width, bVertical ? (height + 1) >> 1 : height,
                          bHorizontal ? (stride + 1) >> 1 : stride};
    }
};

// @todo: enum class instead.
struct Resolution
{
    enum Enum
    {
        e4320p,
        e2160p,
        e1080p,
        e720p,
        e540p,
        e360p,
        eCount
    };

    static Dimensions getDimensions(Enum value)
    {
        static const Dimensions kDimension[eCount] = {
            {7680, 4320, 8192}, {3840, 2160, 4096}, {1920, 1080, 2048},
            {1280, 720, 2048},  {960, 540, 1024},   {640, 360, 1024},
        };

        return (value >= 0 && value < eCount) ? kDimension[value] : Dimensions{};
    }
};

// Helper function that will return an equivalent SIMD flag for the passed
// in flag - or just return the passed in flag, this depends on the platform
// and feature support.
CPUAccelerationFeatures_t simdFlag(CPUAccelerationFeatures_t x86Flag = CAFSSE);

// -----------------------------------------------------------------------------

#endif // VN_DEC_BENCHMARK_UTILITY_H_
