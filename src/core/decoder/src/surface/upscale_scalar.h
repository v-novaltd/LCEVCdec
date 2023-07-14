/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_UPSCALE_SCALAR_H_
#define VN_DEC_CORE_UPSCALE_SCALAR_H_

#include "surface/upscale_common.h"

/*------------------------------------------------------------------------------*/

void horizontalU8Planar(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2],
                        const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                        const Kernel_t* kernel, bool dither);

void horizontalS16Planar(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2],
                         const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                         const Kernel_t* kernel, bool dither);

void horizontalU8NV12(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                      uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel, bool dither);

void horizontalU8RGB(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                     uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel, bool dither);

void horizontalU8RGBA(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                      uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel, bool dither);

void horizontalUNPlanar(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2],
                        const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                        const Kernel_t* kernel, bool dither, uint16_t maxValue);

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