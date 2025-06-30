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

#include "common/random.h"

#include "common/memory.h"
#include "context.h"

#include <time.h>

/*------------------------------------------------------------------------------*/

static inline uint64_t splitMix64Next(uint64_t* value)
{
    uint64_t x = (*value += 0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
    return x ^ (x >> 31);
}

static inline uint32_t rotl(const uint32_t x, int amount)
{
    return (x << amount) | (x >> (32 - amount));
}

typedef struct Random
{
    Memory_t memory;
    uint32_t state[4];
} * Random_t;

/*------------------------------------------------------------------------------*/

bool randomInitialize(Memory_t memory, Random_t* random, uint64_t seed)
{
    Random_t result = VN_CALLOC_T(memory, struct Random);

    if (!result) {
        return false;
    }

    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    result->memory = memory;

    /* Prepare initial state based upon seed. */
    uint64_t x = splitMix64Next(&seed);
    result->state[0] = (uint32_t)x;
    result->state[1] = (uint32_t)(x >> 32);
    x = splitMix64Next(&seed);
    result->state[2] = (uint32_t)x;
    result->state[3] = (uint32_t)(x >> 32);

    *random = result;
    return true;
}

void randomRelease(Random_t random)
{
    if (random) {
        VN_FREE(random->memory, random);
    }
}

uint32_t randomValue(Random_t random)
{
    const uint32_t result = random->state[0] + random->state[3];
    const uint32_t tmp = random->state[1] << 9;

    random->state[2] ^= random->state[0];
    random->state[3] ^= random->state[1];
    random->state[1] ^= random->state[2];
    random->state[0] ^= random->state[3];
    random->state[2] ^= tmp;
    random->state[3] = rotl(random->state[3], 11);

    return result;
}

/*------------------------------------------------------------------------------*/
