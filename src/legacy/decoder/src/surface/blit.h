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

#ifndef VN_LCEVC_LEGACY_BLIT_H
#define VN_LCEVC_LEGACY_BLIT_H

#include "common/types.h"

/*! \file
 *
 * This file is the entry point for the surface blitting functionality.
 *
 * In the context of this library a blit operation performs a per-pixel operation
 * between 2 surfaces, typically these surfaces have the same dimensions with
 * different fixed-point representations.
 *
 * The following operations are available:
 *
 * # BMAdd
 * This mode is present to support adding a surface of residuals to a
 * destination surface. It is expected that the surface containing residuals
 * contains residuals in the "high-precision" fixed-point format of the
 * destination surface. During this operation the addition is saturated into
 * the range of the destination surface fixed-point format.
 *
 * # BMCopy
 * There are 3 different types of copy implemented for format conversions:
 *
 * ## [Unsigned -> Signed]
 * This is referred to as a promotion copy due to the widening of the representable
 * range of values.
 *
 * Example:
 * > U8 -> S8.7
 *
 *
 * ## [Signed -> Unsigned]
 * This is referred to as a demotion copy due to the contracting of the representable
 * range of values.
 *
 * Example:
 * > S8.7 -> U8
 *
 * \note For the depth shift up case we embed the integral shift up into the
 *       conversion shift down and ensure that the target types signed offset
 *       is respected.
 *
 * ## [Unsigned N-bits -> Unsigned M-bits]
 * This is a literal depth shift between 2 formats for both promoting and demoting.
 *
 * Example:
 * > U8 -> U10
 *
 * \note Currently the depth shift down case is not performing rounding, this is
 *       by-design to remain compatible with other implementations.
 *
 *
 * ## [Unsigned N-bits -> Unsigned N-bits]
 * This performs a copy, generally the caller should try to avoid this case and
 * prefer to reference the source surface where possible.
 *
 * Example:
 * > U10 -> U10
 *
 * ## [Signed -> Signed]
 * This performs a copy without needing to perform any per-pixel operations. This
 * is because the shift of the radix is implied by the representation & range of values.
 *
 * Example:
 * > S8.7 -> S10.5
 *
 * \note The copy does not need to perform conversion since the shift of the radix is
 *       implied by the representation & range of values, this falls back to a normal copy
 *       with the destination having the requested fixed-point representation.
 */

/*------------------------------------------------------------------------------*/

typedef struct Logger* Logger_t;
typedef struct ThreadManager ThreadManager_t;
typedef struct Surface Surface_t;

/*------------------------------------------------------------------------------*/

/*! \brief Used to control the type of blit operation required. */
typedef enum BlendingMode
{
    BMAdd, /**< f(a,b) = a + b */
    BMCopy /**< f(a,b) = b */
} BlendingMode_t;

/*! \brief Blits a source surface to a destination surface using the specified
 *         blending mode.
 *
 * \param log            The logger.
 * \param threadManager  The thread manager.
 * \param cpuFeatures    Bitfield indicating the available CPU acceleration features.
 * \param src            The source surface to blit from.
 * \param dst            The destination surface to blit to.
 * \param blending       The blending operation to apply during the blit.
 *
 * \return True if the blit operation was successful. */
bool surfaceBlit(Logger_t log, ThreadManager_t* threadManager, CPUAccelerationFeatures_t cpuFeatures,
                 const Surface_t* src, const Surface_t* dst, BlendingMode_t blending);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_LEGACY_BLIT_H
