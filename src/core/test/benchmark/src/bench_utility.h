/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

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
