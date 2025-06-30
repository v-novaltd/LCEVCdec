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

#ifndef VN_LCEVC_ENHANCEMENT_TRANSFORM_H
#define VN_LCEVC_ENHANCEMENT_TRANSFORM_H

#include "config_parser_types.h"

#include <LCEVC/enhancement/bitstream_types.h>

/*! \file
 *
 * LCEVC has standardized 2 different shaped transforms, this file provides
 * an abstraction to the implementations of these different transforms to convert
 * coefficients into residuals that can be applied to the pixels of an image.
 *
 * The 2 transforms are:
 *
 *  * DD, a.k.a 2x2
 *  * DDS, a.k.a 4x4
 *
 * Additionally when there is 1D upscaling from LOQ-1 to LOQ-0 the transform for
 * LOQ-0 has a specialized implementation.
 *
 * This module has 2 "modes" of operation:
 *
 *    1. Applying the transform to dequantized coefficients.
 *    2. Applying the transform to raw coefficients by first dequantizing them.
 *
 * The first mode is essentially deprecated.
 */

/*------------------------------------------------------------------------------*/

typedef void (*TransformFunction)(const int16_t* coeffs, int16_t* residuals);

/*! \brief Retrieve a function pointer to a transform function.
 *
 * \param transform       The transform type.
 * \param scaling         The scaling mode for the target LOQ.
 * \param forceScalar     Doesn't use SSE or NEON accelerated functions when true
 *
 * \return A valid function pointer if a function is available, otherwise NULL. */
TransformFunction transformGetFunction(LdeTransformType transform, LdeScalingMode scaling, bool forceScalar);

/*------------------------------------------------------------------------------*/

typedef struct Dequant Dequant;
typedef void (*DequantTransformFunction)(const Dequant* dequant, TemporalSignal temporalSignal,
                                         const int16_t* coeffs, int16_t* residuals);

/*! \brief Retrieve a function pointer to a transform function that also
 *         performs dequantization.
 *
 * \param transform       The transform type.
 * \param scaling         The scaling mode for the target LOQ.
 * \param forceScalar     Doesn't use SSE or NEON accelerated functions when true
 *
 * \return A valid function pointer if a function is available, otherwise NULL. */
DequantTransformFunction dequantTransformGetFunction(LdeTransformType transform,
                                                     LdeScalingMode scaling, bool forceScalar);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_TRANSFORM_H
