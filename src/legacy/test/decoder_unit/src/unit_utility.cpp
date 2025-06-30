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

#include "unit_utility.h"

#include "unit_rng.h"

#include <gtest/gtest.h>
#include <LCEVC/build_config.h>

#include <functional>
#include <set>

int32_t fixedPointMinValue(FixedPoint_t fp)
{
    switch (fp) {
        case FPU8:
        case FPU10:
        case FPU12:
        case FPU14: return 0;
        case FPS8:
        case FPS10:
        case FPS12:
        case FPS14: return -32768;
        default: break;
    }
    return 0;
}

int32_t fixedPointMaxValue(FixedPoint_t fp)
{
    switch (fp) {
        case FPU8: return 255;
        case FPU10: return 1023;
        case FPU12: return 4095;
        case FPU14: return 16383;
        case FPS8:
        case FPS10:
        case FPS12:
        case FPS14: return 32767;
        default: break;
    }
    return 0;
}

static int32_t fixedPointOffset(FixedPoint_t fp)
{
    switch (fp) {
        case FPS8:
        case FPS10:
        case FPS12:
        case FPS14: return 16384;
        default: break;
    }
    return 0;
}

template <typename T>
void fillSurfaceWithNoiseT(Surface_t& surface)
{
    T* dst = (T*)surface.data;
    const uint32_t count = surface.stride * surface.height;

    RNG data(static_cast<uint32_t>(fixedPointMaxValue(surface.type)));

    const int32_t offset = fixedPointOffset(surface.type);

    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = static_cast<T>(data() - offset);
    }
}

using SurfaceNoiseFunctionT = std::function<void(Surface_t&)>;

static const SurfaceNoiseFunctionT kSurfaceNoiseFunctions[FPCount] = {
    &fillSurfaceWithNoiseT<uint8_t>,  &fillSurfaceWithNoiseT<uint16_t>,
    &fillSurfaceWithNoiseT<uint16_t>, &fillSurfaceWithNoiseT<uint16_t>,
    &fillSurfaceWithNoiseT<int16_t>,  &fillSurfaceWithNoiseT<int16_t>,
    &fillSurfaceWithNoiseT<int16_t>,  &fillSurfaceWithNoiseT<int16_t>,
};

void fillSurfaceWithNoise(Surface_t& surface) { kSurfaceNoiseFunctions[surface.type](surface); }

template <typename T>
void fillSurfaceRegionWithValueT(Surface_t& surface, uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, int32_t value)
{
    const uint32_t maxHeight = std::min(surface.height, y + height);
    const uint32_t maxWidth = std::min(surface.width, x + width);

    for (uint32_t row = y; row < maxHeight; ++row) {
        auto* pixels = reinterpret_cast<T*>(surfaceGetLine(&surface, row));
        for (uint32_t pixel = x; pixel < maxWidth; ++pixel) {
            pixels[pixel] = static_cast<T>(value);
        }
    }
}

using SurfaceFillRegionFunctionT =
    std::function<void(Surface_t&, uint32_t, uint32_t, uint32_t, uint32_t, int32_t)>;

static const SurfaceFillRegionFunctionT kSurfaceFillRegionFunctions[FPCount] = {
    &fillSurfaceRegionWithValueT<uint8_t>,  &fillSurfaceRegionWithValueT<uint16_t>,
    &fillSurfaceRegionWithValueT<uint16_t>, &fillSurfaceRegionWithValueT<uint16_t>,
    &fillSurfaceRegionWithValueT<int16_t>,  &fillSurfaceRegionWithValueT<int16_t>,
    &fillSurfaceRegionWithValueT<int16_t>,  &fillSurfaceRegionWithValueT<int16_t>,
};

void fillSurfaceRegionWithValue(Surface_t& surface, uint32_t x, uint32_t y, uint32_t width,
                                uint32_t height, int32_t value)
{
    kSurfaceFillRegionFunctions[surface.type](surface, x, y, width, height, value);
}

void fillSurfaceWithValue(Surface_t& surface, int32_t value)
{
    kSurfaceFillRegionFunctions[surface.type](surface, 0, 0, surface.width, surface.height, value);
}

// Helper function as Core functions have types being cast which wrap when we're
// generating out of range values.
template <int32_t Shift>
int32_t fixedPointUnsignedToSigned(int32_t value)
{
    return (value << Shift) - 0x4000;
}

int32_t calculateldlFixedPointHighPrecisionValue(FixedPoint_t lowPrecision, int32_t value)
{
    switch (lowPrecision) {
        case FPU8: return fixedPointUnsignedToSigned<7>(value);
        case FPU10: return fixedPointUnsignedToSigned<5>(value);
        case FPU12: return fixedPointUnsignedToSigned<3>(value);
        case FPU14: return fixedPointUnsignedToSigned<1>(value);
        default: break;
    }
    return value;
}

// Generates unique `count` indices between 0 and `maxIndex`, if this cannot be satisfied
// because there are too many indices to generate then an empty set is returned.
//
// Noting that the closer `count` is to `maxIndex` the longer this function will take to
// run, and statistically can take forever, this actually holds true for any none-zero
// value of `count` but would require repeating the random values consistently.
std::set<int32_t> generateRandomIndices(uint32_t maxIndex, uint32_t count)
{
    RNG data(static_cast<uint32_t>(maxIndex - 1));
    std::set<int32_t> res;

    // Do nothing.
    if (count >= maxIndex) {
        return res;
    }

    // Keep inserting random data until full.
    while (res.size() < count) {
        res.insert(static_cast<int32_t>(data()));
    }

    return res;
}

void ExpectEqSurfacesTiled(int32_t transformSize, const Surface_t& value, const Surface_t& expected)
{
    EXPECT_GT(transformSize, 0);
    EXPECT_EQ(value.width, expected.width);
    EXPECT_EQ(value.height, expected.height);
    EXPECT_EQ(value.type, expected.type);

    const auto tS = static_cast<uint32_t>(transformSize);

    const uint32_t transformsAcross = (value.width + tS - 1) / tS;
    const uint32_t transformsDown = (value.height + tS - 1) / tS;
    const uint32_t pixelSize = ldlFixedPointByteSize(value.type);

    for (uint32_t tY = 0; tY < transformsDown; ++tY) {
        const uint32_t pixelY = tY * transformSize;
        for (uint32_t tX = 0; tX < transformsAcross; ++tX) {
            const uint32_t pixelX = tX * transformSize;
            const uint32_t pixelCount = std::min(value.width, pixelX + transformSize) - pixelX;
            const uint32_t tileLineSize = pixelCount * pixelSize;

            for (uint32_t y = pixelY; y < std::min(value.height, (pixelY + transformSize)); ++y) {
                const auto xOffset = pixelX * pixelSize;
                const uint8_t* valueLine = surfaceGetLine(&value, y) + xOffset;
                const uint8_t* expectedLine = surfaceGetLine(&expected, y) + xOffset;
                const auto compResult = memcmp(valueLine, expectedLine, tileLineSize);

                EXPECT_EQ(compResult, 0) << "Transform mismatch - tile=[" << tX << "," << tY
                                         << "], pixel=[" << pixelX << "," << pixelY << "]";
                if (compResult != 0) {
                    break;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

CPUAccelerationFeatures_t simdFlag(CPUAccelerationFeatures_t x86Flag)
{
    if (x86Flag != CAFNone) {
#if VN_CORE_FEATURE(SSE) || VN_CORE_FEATURE(AVX2)
        return x86Flag;
#endif
#if VN_CORE_FEATURE(NEON)

        return CAFNEON;
#endif
    }

    return CAFNone;
}

// -----------------------------------------------------------------------------
