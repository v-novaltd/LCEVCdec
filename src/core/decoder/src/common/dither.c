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

#include "common/dither.h"

#include "common/memory.h"
#include "common/random.h"
#include "common/types.h"
#include "context.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

static const size_t kDitherBufferSize = 16384;
static const uint8_t kMaxDitherStrength = 128;

typedef struct Dither
{
    Memory_t memory;
    int8_t* buffer;
    Random_t random;
    bool enabled;
    uint8_t strength;
    bool strengthIsOverridden;
    DitherType_t type;
    perseus_pipeline_mode pipelineMode;
    BitDepth_t bitDepth;
} * Dither_t;

/*------------------------------------------------------------------------------*/

bool ditherInitialize(Memory_t memory, Dither_t* dither, uint64_t seed, bool enabled, int32_t overrideStrength)
{
    Dither_t result = VN_CALLOC_T(memory, struct Dither);

    if (!result) {
        return false;
    }

    result->memory = memory;
    result->enabled = enabled;
    result->strengthIsOverridden = (overrideStrength >= 0 && overrideStrength <= kMaxDitherStrength);
    if (result->strengthIsOverridden) {
        result->strength = (uint8_t)overrideStrength;
        result->type = DTUniform;
    }

    if (enabled) {
        /* Prepare buffer & RNG */
        result->buffer = VN_CALLOC_T_ARR(memory, int8_t, kDitherBufferSize);
        const bool randomInitialized = randomInitialize(memory, &result->random, seed);

        if (!result->buffer || !randomInitialized) {
            ditherRelease(result);
            return false;
        }
    }

    *dither = result;
    return true;
}

void ditherRelease(Dither_t dither)
{
    if (dither) {
        Memory_t memory = dither->memory;
        if (dither->random) {
            VN_FREE(memory, dither->random);
        }
        VN_FREE(memory, dither->buffer);
        VN_FREE(memory, dither);
    }
}

bool ditherRegenerate(Dither_t dither, uint8_t strength, DitherType_t type,
                      perseus_pipeline_mode pipelineMode, BitDepth_t bitDepth)
{
    if (!dither) {
        return true;
    }

    dither->pipelineMode = pipelineMode;
    dither->bitDepth = bitDepth;

    if (!dither->strengthIsOverridden) {
        dither->strength = strength;
        dither->type = type;
    }

    if (dither->strength > kMaxDitherStrength) {
        return false;
    }

    if (!ditherIsEnabled(dither)) {
        return true;
    }

    if (dither->strength == 0 || dither->type == DTNone) {
        memorySet(dither->buffer, 0, kDitherBufferSize);
        return true;
    }

    /* Populate dither buffers with values between [-strength, strength]. */
    Random_t random = dither->random;
    const uint32_t saturation = (2 * dither->strength) + 1;

    for (size_t i = 0; i < kDitherBufferSize; ++i) {
        const uint32_t value = randomValue(random) % saturation;
        dither->buffer[i] = (int8_t)(value - dither->strength);
    }

    return true;
}

bool ditherIsEnabled(Dither_t dither)
{
    return !(!dither || !dither->enabled || /* Globally disabled*/
             dither->type == DTNone ||      /* Signalled disabled type*/
             (dither->strength == 0)        /* Signalled 0 strength (implicit disabled) */
    );
}

const int8_t* ditherGetBuffer(Dither_t dither, size_t length)
{
    assert(dither->buffer != NULL);

    if (length > kDitherBufferSize) {
        return NULL;
    }

    const size_t position = randomValue(dither->random) % (kDitherBufferSize - length);
    return &dither->buffer[position];
}

int8_t ditherGetShiftS16(Dither_t dither)
{
    if (dither->pipelineMode == PPM_PRECISION) {
        switch (dither->bitDepth) {
            case Depth8: return 7;
            case Depth10: return 5;
            case Depth12: return 3;
            case Depth14: return 1;
            default: return 0;
        }
    }
    return 0;
}

/*------------------------------------------------------------------------------*/
