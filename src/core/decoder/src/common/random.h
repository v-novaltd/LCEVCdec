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

#ifndef VN_DEC_CORE_RANDOM_H_
#define VN_DEC_CORE_RANDOM_H_

#include "common/platform.h"

/*! \file
 * \brief This file provides a simple random module.
 *
 * This random implementation uses a simple xor-shift-rotate algorithm for
 * random number generation (RNG) - this was selected as a consistently and
 * efficient performing RNG approach across most platforms.
 *
 * The characteristics of the RNG have not been defined with respect to the
 * quality of the generation relating specifically to distribution - this is
 * not important for our use-case - subjectively this solution provides pleasing
 * results during live viewing.
 */

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;

/*! Opaque handle to the random module. */
typedef struct Random* Random_t;

/*------------------------------------------------------------------------------*/

/*! Initializes the random module.
 *
 * \param ctx The decoder context.
 * \param random The random module to initialize.
 * \param seed The seed to initialise with, if 0 it will use time().
 *
 * \return false if memory allocation failed, otherwise true. */
bool randomInitialize(Memory_t memory, Random_t* random, uint64_t seed);

/*! Releases the random module and any associated memory. */
void randomRelease(Random_t random);

/*! Calculates a new random value and updates the random modules state.
 *
 * \return A random value between 0 and 4294967295. */
uint32_t randomValue(Random_t random);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_RANDOM_H_ */
