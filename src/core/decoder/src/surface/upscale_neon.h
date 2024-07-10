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