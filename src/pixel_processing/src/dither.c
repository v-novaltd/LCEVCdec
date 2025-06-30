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

#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include <assert.h>

/*------------------------------------------------------------------------------*/

static const size_t kDitherBufferSize = 16384;
static const uint8_t kMaxDitherStrength = 31;

/*------------------------------------------------------------------------------*/

static void ldppDitherRegenerate(LdppDitherGlobal* dither, uint64_t seed)
{
    LdcRandom random;
    ldcRandomInitialize(&random, seed);

    for (size_t i = 0; i < kDitherBufferSize; ++i) {
        dither->buffer[i] = (uint16_t)(ldcRandomValue(&random) >> 16);
    }
}

bool ldppDitherGlobalInitialize(LdcMemoryAllocator* memoryAllocator, LdppDitherGlobal* dither, uint64_t seed)
{
    dither->allocator = memoryAllocator;

    /* Prepare buffer & RNG */
    dither->buffer = VNAllocateAlignedArray(memoryAllocator, &dither->allocationBuffer, uint16_t,
                                            kBufferRowAlignment, kDitherBufferSize);

    if (!dither->buffer) {
        ldppDitherGlobalRelease(dither);
        return false;
    }

    ldppDitherRegenerate(dither, seed);

    return true;
}

void ldppDitherGlobalRelease(LdppDitherGlobal* dither)
{
    if (dither) {
        VNFree(dither->allocator, &dither->allocationBuffer);
    }
}

bool ldppDitherFrameInitialise(LdppDitherFrame* frame, LdppDitherGlobal* global, uint64_t seed, uint8_t strength)
{
    if (strength > kMaxDitherStrength) {
        return false;
    }

    frame->global = global;
    frame->strength = strength;
    frame->frameSeed = seed;

    return true;
}

void ldppDitherSliceInitialise(LdppDitherSlice* slice, LdppDitherFrame* frame, uint32_t offset,
                               uint32_t planeIndex)
{
    slice->global = frame->global;
    slice->strength = frame->strength;

    uint64_t seed = frame->frameSeed;
    seed ^= offset;
    seed ^= ((uint64_t)planeIndex) << 32;
    ldcRandomInitialize(&slice->random, seed);
}

const uint16_t* ldppDitherGetBuffer(LdppDitherSlice* dither, size_t length)
{
    assert(dither->global != NULL);
    assert(dither->global->buffer != NULL);

    if (length > kDitherBufferSize) {
        return NULL;
    }

    const size_t position = ldcRandomValue(&dither->random) % (kDitherBufferSize - length);
    return &dither->global->buffer[position];
}

int8_t ldppDitherGetShiftS16(LdpFixedPoint bitDepth)
{
    switch (bitDepth) {
        case LdpFPS8: return 7;
        case LdpFPS10: return 5;
        case LdpFPS12: return 3;
        case LdpFPS14: return 1;
        default: return 0;
    }
}

/*------------------------------------------------------------------------------*/
