/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#ifndef CORE_TEST_UNIT_UTILITY_H_
#define CORE_TEST_UNIT_UTILITY_H_

extern "C"
{
#include "common/cmdbuffer.h"
#include "common/types.h"
#include "surface/surface.h"
}

// Returns the numerically smallest value for a given fixed point type.
int32_t fixedPointMinValue(FixedPoint_t fp);

// Returns the numerically largest value for a given fixed point type.
int32_t fixedPointMaxValue(FixedPoint_t fp);

// Populates a surface with random data.
void fillSurfaceWithNoise(Surface_t& surface);

void fillSurfaceRegionWithValue(Surface_t& surface, uint32_t x, uint32_t y, uint32_t width,
                                uint32_t height, int32_t value);

void fillSurfaceWithValue(Surface_t& surface, int32_t value);

int32_t calculateFixedPointHighPrecisionValue(FixedPoint_t lowPrecision, int32_t value);

// Populates a command buffer with random data.
//
// `entryOccupancyPercentage` is used to control how much data the
// command buffer contains in relation to the width and height and
// layer count (for a layerCount of 0 it is assumed that the buffer
// is filled with tile clears).
void fillCmdBufferWithNoise(CmdBuffer_t* cmdBuffer, FixedPoint_t fpType, int16_t width,
                            int16_t height, int32_t layerCount, double entryOccupancyPercentage);

// Compare 2 surfaces on a per-transform basis and reports which transforms mismatch.
//
// Note: This function handles partial transforms too.
void ExpectEqSurfacesTiled(int32_t transformSize, const Surface_t& value, const Surface_t& expected);

// Return an equivalent SIMD flag for the passed in flag - or just return
// the passed in flag. The return value depends on the platform and feature support.
CPUAccelerationFeatures_t simdFlag(CPUAccelerationFeatures_t x86Flag = CAFSSE);

#endif // CORE_TEST_UNIT_UTILITY_H_