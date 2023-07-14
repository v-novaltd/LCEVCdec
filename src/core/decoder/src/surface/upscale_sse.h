/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_UPSCALE_SSE_H_
#define VN_DEC_CORE_UPSCALE_SSE_H_

#include "surface/upscale_common.h"

/*! \brief Retrieves a function pointer to a horizontal upscaling function using SSE.
 *
 *  \param ilv      The interleaving type being upscaled from & to.
 *  \param srcFP    The source data fixedpoint type to upscale from.
 *  \param dstFP    The destination data fixedpoint type to upscale to.
 *  \param baseFP   The base data fixedpoint type to read from for PA.
 *
 *  \return A valid function pointer on success otherwise NULL. */
UpscaleHorizontal_t upscaleGetHorizontalFunctionSSE(Interleaving_t ilv, FixedPoint_t srcFP,
                                                    FixedPoint_t dstFP, FixedPoint_t baseFP);

/*! \brief Retrieves a function pointer to a vertical upscaling function using SSE.
 *
 *  \param srcFP    The source data fixedpoint type to upscale from.
 *  \param dstFP    The destination data fixedpoint type to upscale to.
 *
 *  \return A valid function pointer on success otherwise NULL. */
UpscaleVertical_t upscaleGetVerticalFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP);

#endif /* VN_DEC_CORE_UPSCALE_SSE_H_ */