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

#ifndef VN_DEC_CORE_UPSCALE_SCALAR_H_
#define VN_DEC_CORE_UPSCALE_SCALAR_H_

#include "common/types.h"
#include "surface/upscale_common.h"

#include <stdint.h>

/*------------------------------------------------------------------------------*/

void horizontalU8Planar(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                        uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel);

void horizontalS16Planar(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                         uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel);

void horizontalU8NV12(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                      uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel);

void horizontalU8RGB(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                     uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel);

void horizontalU8RGBA(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                      uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel);

void horizontalUNPlanar(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                        const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                        const Kernel_t* kernel, uint16_t maxValue);

/*------------------------------------------------------------------------------*/

/*! \brief Retrieves a function pointer to a horizontal upscaling function that uses NEON.
 *
 *  \param interleaving  The interleaving type to upscale for.
 *  \param srcFP         The source data fixedpoint type to upscale from.
 *  \param dstFP         The destination data fixedpoint type to upscale to.
 *  \param baseFP        The base data fixedpoint type to read from for PA.
 *
 *  \return A valid function pointer on success otherwise NULL. */
UpscaleHorizontal_t upscaleGetHorizontalFunction(Interleaving_t interleaving, FixedPoint_t srcFP,
                                                 FixedPoint_t dstFP, FixedPoint_t baseFP);

/*! \brief Retrieves a function pointer to a vertical upscaling function that uses NEON.
 *         upscaling with the supplied source and destination fixedpoint types.
 *
 *  \param srcFP    The source data fixedpoint type to upscale from.
 *  \param dstFP    The destination data fixedpoint type to upscale to.
 *
 * \return A valid function pointer if available, otherwise NULL. */
UpscaleVertical_t upscaleGetVerticalFunction(FixedPoint_t srcFP, FixedPoint_t dstFP);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_UPSCALE_SCALAR_H_ */
