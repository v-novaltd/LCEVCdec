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

#ifndef VN_DEC_CORE_UPSCALE_H_
#define VN_DEC_CORE_UPSCALE_H_

#include "surface/surface.h"

/* \file
 * This file is the entry point for the surface upscaling functionality.
 */

/*! \brief upscale kernel */
typedef struct Kernel
{
    int16_t coeffs[2][8]; /**< upscale kernels of length 'len', ordered with forward kernel first. */
    uint8_t length;       /**< length (taps) of upscale kernels */
    bool isPreBakedPA; /**< true if predicted-average computation has been pre-baked into this kernel */
} Kernel_t;

/*------------------------------------------------------------------------------*/

/*! \brief upscale parameters to perform upscaling with. */
typedef struct UpscaleArgs
{
    const Surface_t* src; /**< The input surface to upscale from. */
    const Surface_t* dst; /**< The destination surface to upscale to. */
    bool applyPA;         /**< Indicates that predicted-average should be applied */
    bool applyDither;     /**< Indicates that dithering should be applied  */
    UpscaleType_t type;   /**< The upscale type to apply. */
    ScalingMode_t mode;   /**< The type of scaling to perform (1D or 2D). */
    CPUAccelerationFeatures_t preferredAccel; /**< Desired CPU acceleration features to use. */
} UpscaleArgs_t;

/*------------------------------------------------------------------------------*/

/*! \brief Upscales a source surface to a destination surface using the supplied
 *         args.
 *
 *  \param ctx   Decoder context.
 *  \param args  The arguments to use for upscaling.
 *
 *  \return True if the upscale operation was successful. */
bool upscale(Memory_t memory, Logger_t log, Context_t* ctx, const UpscaleArgs_t* params);

/*! \brief Obtain a kernel to use for upscaling based upon the type supplied
 *         and the current bitstream and configuration settings.
 *
 *  \param ctx   Decoder context.
 *  \param type  The type of scaling to option the kernel form.
 *  \param out   The kernel to write to. If this function fails, this is
 *               left unmodified.
 *
 * \return True if a valid kernel can be retrieved. */
bool upscaleGetKernel(Logger_t log, Context_t* ctx, UpscaleType_t type, Kernel_t* out);

/*! \brief Determine whether predicted average computation should be applied,
 *         depending on the current configuration and bitstream signaling.
 *
 *  \param  ctx Decoder context
 *
 *  \return True if pa should be applied. */
bool upscalePAIsEnabled(Logger_t log, Context_t* ctx);

/*------------------------------------------------------------------------------*/

#endif /* __DPI_UPSCALE_H__ */