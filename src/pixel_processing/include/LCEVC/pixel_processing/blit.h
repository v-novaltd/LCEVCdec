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

#ifndef VN_LCEVC_PIXEL_PROCESSING_BLIT_H
#define VN_LCEVC_PIXEL_PROCESSING_BLIT_H

#include <LCEVC/common/task_pool.h>
#include <LCEVC/pipeline/picture.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \file
 *
 * This file is the entry point for the plane blitting functionality.
 *
 * In the context of this library a blit operation performs a per-pixel operation
 * between 2 planes, typically these planes have the same dimensions with
 * different fixed-point representations.
 *
 * The following operations are available:
 *
 * # BMAdd
 * This mode is present to support adding a plane of residuals to a
 * destination plane. It is expected that the plane containing residuals
 * contains residuals in the "high-precision" fixed-point format of the
 * destination plane. During this operation the addition is saturated into
 * the range of the destination plane fixed-point format.
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
 * prefer to reference the source plane where possible.
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

/*! \brief Used to control the type of blit operation required. */
typedef enum LdppBlendingMode
{
    BMAdd, /**< f(a,b) = a + b */
    BMCopy /**< f(a,b) = b */
} LdppBlendingMode;

/*! \brief Blits a source plane to a destination plane using the specified
 *         blending mode.
 *
 * \param taskPool       The task pool to create a sliced blit task from
 * \param forceScalar    Doesn't use SSE or NEON accelerated functions when true.
 * \param planeIndex     The plane index in src/dst layout
 * \param srcLayout      The source plane picture layout
 * \param dstLayout      The destination picture layout
 * \param srcPlane       The source plane to blit from.
 * \param dstPlane       The destination plane to blit to.
 * \param blending       The blending operation to apply during the blit.
 *
 * \return True if the blit operation was successful. */
bool ldppPlaneBlit(LdcTaskPool* taskPool, LdcTask* parent, bool forceScalar, uint32_t planeIndex,
                   const LdpPictureLayout* srcLayout, const LdpPictureLayout* dstLayout,
                   LdpPicturePlaneDesc* srcPlane, LdpPicturePlaneDesc* dstPlane,
                   LdppBlendingMode blending);

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIXEL_PROCESSING_BLIT_H
