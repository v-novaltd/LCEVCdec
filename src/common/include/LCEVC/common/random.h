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

#ifndef VN_LCEVC_COMMON_RANDOM_H
#define VN_LCEVC_COMMON_RANDOM_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
//
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

typedef struct LdcRandom
{
    uint32_t state[4];
} LdcRandom;

/*------------------------------------------------------------------------------*/

/*! Initializes the random module.
 *
 * \param random [in]          The random module to initialize.
 * \param seed   [in]          The seed to initialize with, if 0 it will use time().
 */
void ldcRandomInitialize(LdcRandom* random, uint64_t seed);

/*! Calculates a new random value and updates the random modules state.
 *
 * \param random [in]          An initialized random module.
 *
 * \return                     A random value between 0 and 4294967295.
 */
uint32_t ldcRandomValue(LdcRandom* random);

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_RANDOM_H
