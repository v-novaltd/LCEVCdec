/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_UPSCALE_NEON_H_
#define VN_DEC_CORE_UPSCALE_NEON_H_

#include "surface/upscale_common.h"

/*------------------------------------------------------------------------------*/

/* Retrieve a function pointer to a horizontal upscaling function using NEON that
 * supports upscaling with the supplied interleaving, source, destination and base
 * fixedpoint types.
 *
 * \param ctx      Decoder context.
 * \param ilv      The interleaving type to upscale for.
 * \param srcFP    The source data fixedpoint type to upscale from.
 * \param dstFP    The destination data fixedpoint type to upscale to.
 * \param baseFP   The base data fixedpoint type to read from for PA.
 *
 * \return A valid function pointer on success otherwise NULL.
 */
UpscaleHorizontal_t upscaleGetHorizontalFunctionNEON(Interleaving_t ilv, FixedPoint_t srcFP,
                                                     FixedPoint_t dstFP, FixedPoint_t baseFP);

/* Retrieve a function pointer to a vertical upscaling function using NEON that
 * supports upscaling with the supplied source and destination fixedpoint types.
 *
 * \param ctx      Decoder context.
 * \param srcFP    The source data fixedpoint type to upscale from.
 * \param dstFP    The destination data fixedpoint type to upscale to.
 *
 * \return A valid function pointer on success otherwise NULL.
 */
UpscaleVertical_t upscaleGetVerticalFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_UPSCALE_NEON_H_ */