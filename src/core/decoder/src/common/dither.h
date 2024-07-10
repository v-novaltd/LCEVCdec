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

#ifndef VN_DEC_CORE_DITHERING_H_
#define VN_DEC_CORE_DITHERING_H_

#include "common/types.h"

/*! \file
 * \brief This file provides a simple dithering module.
 *
 * Dithering is a method of introducing noise into a decoded image to provide an
 * apparent image sharpening.
 *
 * For LCEVC dithering controls are signaled within the bit-stream: - these are
 * strength and type.
 *
 * # Dither Strength
 * For strength the value is the maximum amount (+/-) that a pixel may be perturbed by
 * the random noise.
 *
 * # Dither Type
 * Currently there is:
 *     - None (i.e. disabled)
 *     - Uniform (i.e. uniformly random).
 *
 * In our implementation we opt to use a xor-shift-rotate algorithm for performance
 * reasons, we have not performed analysis to determine if it is uniform, but it
 * does appear to provide subjectively sound noise.
 *
 * # Usage
 * ## Initialization/Cleanup
 * This must be called before any dithering is performed, this is used
 * to prepare the random number generator with a seed, which may be useful
 * for debugging, and a global flag to toggle dithering functionality.
 *
 *    Dither_t dither;
 *    ditherInitialize(ctx, &dither, ditherSeed, ditherEnabled);
 *    ...
 *    ...
 *    ditherRelease(dither);
 *
 * ## Regeneration
 * This must be called before any dithering is performed, this should be
 * called at most, once per-frame with the signaled dither properties.
 *
 *    ditherRegenerate(dither, signaledStrength, signaledType);
 *
 * ## Query
 * This must be called whenever dithering values are required. A user is
 * then expected to add the N number of values returned from this function
 * to their destination.
 *
 *    const int8_t* ditherValues = ditherGetBuffer(dither, numberOfValues);
 *
 * # Note
 * Dithering values are NOT rescaled to the final output bit-depth, this is
 * by design.
 */

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;

/*! Opaque handle to the dither module. */
typedef struct Dither* Dither_t;

/*------------------------------------------------------------------------------*/

/*! Initializes the dither module
 *
 * \param ctx              The decoder context.
 * \param dither           The dither module to initialize.
 * \param seed             The seed to initialise the random number generator
 *                         with, if 0 it will use time()
 * \param enabled          Global flag controlling dither functionality.
 * \param overrideStrength A strength which overrides the stream's strength (if
 *                         positive, and less than or equal to
 *                         kMaxDitherStrength).
 *
 * \return false if memory allocation failed, otherwise true.
 */
bool ditherInitialize(Memory_t ctx, Dither_t* dither, uint64_t seed, bool enabled, int32_t overrideStrength);

/*! Releases the dither module and any associated memory. */
void ditherRelease(Dither_t dither);

/*! Regenerates the internal dither buffer.
 *
 * This function early-exits if dithering has been disabled globally.
 *
 * \param dither The dither module.
 * \param strength The signaled dither strength, must be in the range 0 to 128.
 * \param type The signaled dither type
 *
 * \return true on success, otherwise false
 */
bool ditherRegenerate(Dither_t dither, uint8_t strength, DitherType_t type);

/*! Determines if dithering is enabled
 *
 * Dithering is enabled via several properties:
 *
 *    1. Global flag that is set during initialization that will override any
 *       stream configuration.
 *    2. Stream signaled dithering type, whereby None disables dithering.
 *    3. Stream signaled strength, whereby a value of 0 disables dithering.
 *
 * \return true if dithering is enabled, otherwise false
 */
bool ditherIsEnabled(Dither_t dither);

/*! Query the dithering module for a pointer to an array of values
 * that contain random noise that is at least `length` elements
 * long.
 *
 * This function will return a NULL pointer if dithering has
 * been disabled globally.
 *
 * If dithering has been disabled by the bit-stream configuration
 * a valid pointer may be returned, but the contents is undefined.
 *
 * A user is expected to control calls to this function by first
 * checking if dithering is enabled via the `ditherIsEnabled()` API.
 *
 * \param dither  The dither module.
 * \param length  The number of elements to query.
 *
 * \return A valid pointer on success, otherwise NULL, noting if `length`
 *         is greater than 16384 the function will return NULL.
 */
const int8_t* ditherGetBuffer(Dither_t dither, size_t length);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_DITHERING_H_ */
