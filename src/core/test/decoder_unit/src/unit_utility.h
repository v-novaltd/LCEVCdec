/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

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

// Compare 2 surfaces on a per-transform basis and reports which transforms mismatch.
//
// Note: This function handles partial transforms too.
void ExpectEqSurfacesTiled(int32_t transformSize, const Surface_t& value, const Surface_t& expected);

// Return an equivalent SIMD flag for the passed in flag - or just return
// the passed in flag. The return value depends on the platform and feature support.
CPUAccelerationFeatures_t simdFlag(CPUAccelerationFeatures_t x86Flag = CAFSSE);

#endif // CORE_TEST_UNIT_UTILITY_H_
