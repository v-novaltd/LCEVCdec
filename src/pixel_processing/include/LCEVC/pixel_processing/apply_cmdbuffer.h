/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_H
#define VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_H

#include <LCEVC/common/task_pool.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief Applies a CPU cmdbuffer to a plane
 *
 * \param[in]    taskPool        A task pool for multi-threaded apply of a cmdbuffer with entrypoints,
 *                               can be null if cmdbuffer doesn't have entrypoints
 * \param[in]    enhancementTile Structure containing CPU cmdbuffer and tile metadata for tiling mode
 * \param[in]    fixedPoint      Datatype of the plane
 * \param[inout] plane           Plane of pixels to apply residuals to
 * \param[in]    rasterOrder     Toggle between block order or raster order apply, given by
 *                               `temporalEnabled` in the global config
 * \param[in]    forceScalar     Set to true to disable SIMD
 * \param[in]    highlight       Set to true to ignore residual values and apply maximum values at
 *                               residual locations for debugging residual distribution
 */
bool ldppApplyCmdBuffer(LdcTaskPool* taskPool, LdcTask* parent, LdpEnhancementTile* enhancementTile,
                        LdpFixedPoint fixedPoint, const LdpPicturePlaneDesc* plane,
                        bool rasterOrder, bool forceScalar, bool highlight);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_H
