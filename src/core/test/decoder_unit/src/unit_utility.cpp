/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "unit_utility.h"

#include "lcevc_config.h"
#include "unit_rng.h"

#include <gtest/gtest.h>

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

int32_t calculateFixedPointHighPrecisionValue(FixedPoint_t lowPrecision, int32_t value)
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

void generateRandomResiduals() {}

template <typename T>
void fillCmdBufferWithNoiseT(CmdBuffer_t* cmdBuffer, FixedPoint_t fpType, int16_t width,
                             int16_t height, int32_t layerCount, double entryOccupancyPercentage)
{
    const auto blockSize =
        (layerCount == 0) ? 32 : transformTypeDimensions(transformTypeFromLayerCount(layerCount));

    int16_t data[RCLayerMaxCount] = {0};

    const int32_t blocksAcross = (width + blockSize - 1) / blockSize;
    const int32_t blocksDown = (height + blockSize - 1) / blockSize;
    const int32_t maxBlockCount = blocksAcross * blocksDown;
    const auto entryCount = static_cast<int32_t>(maxBlockCount * std::min(entryOccupancyPercentage, 1.0));

    const auto indices = generateRandomIndices(maxBlockCount, entryCount);

    RNG rngData(static_cast<uint32_t>(fixedPointMaxValue(fpType)));

    const int32_t offset = fixedPointOffset(fpType);

    const auto fillData = [&data, &layerCount, &rngData, &offset]() {
        for (int32_t i = 0; i < layerCount; ++i) {
            data[i] = static_cast<T>(static_cast<int32_t>(rngData()) - offset);
        }
    };

    cmdBufferReset(cmdBuffer, layerCount);

    for (auto index : indices) {
        const auto blockY = static_cast<int16_t>(index / blocksAcross);
        const auto blockX = static_cast<int16_t>(index % blocksAcross);

        if (layerCount == 0) {
            cmdBufferAppendCoord(cmdBuffer, blockX, blockY);
        } else {
            fillData();
            cmdBufferAppend(cmdBuffer, blockX, blockY, data);
        }
    }
}

using CmdBufferNoiseFunctionT =
    std::function<void(CmdBuffer_t*, FixedPoint_t, int16_t, int16_t, int32_t, double)>;

static const CmdBufferNoiseFunctionT kCmdBufferNoiseFunctions[FPCount] = {
    &fillCmdBufferWithNoiseT<uint8_t>,  &fillCmdBufferWithNoiseT<uint16_t>,
    &fillCmdBufferWithNoiseT<uint16_t>, &fillCmdBufferWithNoiseT<uint16_t>,
    &fillCmdBufferWithNoiseT<int16_t>,  &fillCmdBufferWithNoiseT<int16_t>,
    &fillCmdBufferWithNoiseT<int16_t>,  &fillCmdBufferWithNoiseT<int16_t>,
};

void fillCmdBufferWithNoise(CmdBuffer_t* cmdBuffer, FixedPoint_t fpType, int16_t width,
                            int16_t height, int32_t layerCount, double entryOccupancyPercentage)
{
    kCmdBufferNoiseFunctions[fpType](cmdBuffer, fpType, width, height, layerCount, entryOccupancyPercentage);
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
    const uint32_t pixelSize = fixedPointByteSize(value.type);

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