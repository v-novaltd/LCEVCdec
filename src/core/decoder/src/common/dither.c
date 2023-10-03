/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "common/dither.h"

#include "common/memory.h"
#include "common/random.h"
#include "context.h"

#include <assert.h>

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
}* Dither_t;

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
        if (dither->random)
            VN_FREE(memory, dither->random);
        VN_FREE(memory, dither->buffer);
        VN_FREE(memory, dither);
    }
}

bool ditherRegenerate(Dither_t dither, uint8_t strength, DitherType_t type)
{
    if (!dither) {
        return true;
    }

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

/*------------------------------------------------------------------------------*/