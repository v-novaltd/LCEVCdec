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

#ifndef VN_LCEVC_PIXEL_PROCESSING_UPSCALE_H
#define VN_LCEVC_PIXEL_PROCESSING_UPSCALE_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* \file
 * This file is the entry point for the surface upscaling functionality.
 */

/*------------------------------------------------------------------------------*/

/*! \brief upscale parameters to perform upscaling with. */
typedef struct ldppUpscaleArgs
{
    uint32_t planeIndex;
    LdpPictureLayout* srcLayout;
    LdpPictureLayout* dstLayout;
    LdpPicturePlaneDesc srcPlane;
    LdpPicturePlaneDesc dstPlane;
    bool applyPA;                 /**< Indicates that predicted-average should be applied */
    LdppDitherFrame* frameDither; /**< Indicates that dithering should be applied  */
    LdeScalingMode mode;          /**< The type of scaling to perform (1D or 2D). */
    bool forceScalar;             /**< Desired CPU acceleration features to use. */
} LdppUpscaleArgs;

/*------------------------------------------------------------------------------*/

/*! \brief Upscales a source surface to a destination surface using the supplied args.
 *
 *  \param allocator      The memory allocator.
 *  \param taskPool       The task pool to create a sliced blit task from
 *  \param parent         If not NULL, task to inherit dependencies from
 *  \param kernel         The kernel to use for upscaling.
 *  \param params         The arguments to use for upscaling.
 *
 *  \return True if the upscale operation was successful. */
bool ldppUpscale(LdcMemoryAllocator* allocator, LdcTaskPool* taslPool, LdcTask* parent,
                 const LdeKernel* kernel, const LdppUpscaleArgs* params);

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIXEL_PROCESSING_UPSCALE_H
