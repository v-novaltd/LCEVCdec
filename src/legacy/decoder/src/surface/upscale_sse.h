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

#ifndef VN_LCEVC_LEGACY_UPSCALE_SSE_H
#define VN_LCEVC_LEGACY_UPSCALE_SSE_H

#include "common/types.h"
#include "surface/upscale_common.h"

/*! \brief Retrieves a function pointer to a horizontal upscaling function using SSE.
 *
 *  \param ilv      The interleaving type being upscaled from & to.
 *  \param srcFP    The source data fixedpoint type to upscale from.
 *  \param dstFP    The destination data fixedpoint type to upscale to.
 *  \param baseFP   The base data fixedpoint type to read from for PA.
 *
 *  \return A valid function pointer on success otherwise NULL. */
UpscaleHorizontal_t ldlUpscaleGetHorizontalFunctionSSE(Interleaving_t ilv, FixedPoint_t srcFP,
                                                       FixedPoint_t dstFP, FixedPoint_t baseFP);

/*! \brief Retrieves a function pointer to a vertical upscaling function using SSE.
 *
 *  \param srcFP    The source data fixedpoint type to upscale from.
 *  \param dstFP    The destination data fixedpoint type to upscale to.
 *
 *  \return A valid function pointer on success otherwise NULL. */
UpscaleVertical_t ldlUpscaleGetVerticalFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP);

#endif // VN_LCEVC_LEGACY_UPSCALE_SSE_H
