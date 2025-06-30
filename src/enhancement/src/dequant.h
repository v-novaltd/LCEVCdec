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

#ifndef VN_LCEVC_ENHANCEMENT_DEQUANT_H
#define VN_LCEVC_ENHANCEMENT_DEQUANT_H

#include "config_parser_types.h"

#include <LCEVC/build_config.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/config_types.h>

#if VN_CORE_FEATURE(SSE)
#include <smmintrin.h>
#elif VN_CORE_FEATURE(NEON)
#include <LCEVC/common/neon.h>
#endif

#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

/*! \brief Restore the supplied quant-matrix to the standard defined default values. */
void quantMatrixSetDefault(LdeQuantMatrix* matrix, LdeScalingMode loq0Scaling,
                           LdeTransformType transform, LdeLOQIndex index);

/*! \brief Copies the LOQ-0 quant matrix to LOQ-1. */
void quantMatrixDuplicateLOQs(LdeQuantMatrix* matrix);

/*! \brief Retrieve a pointer to the quant-matrix for LOQ-0 based upon the scaling
 *         mode used. */
static inline uint8_t* quantMatrixGetValues(LdeQuantMatrix* matrix, LdeLOQIndex index)
{
    if (!matrix || ((uint32_t)index >= LOQEnhancedCount)) {
        return NULL;
    }
    return matrix->values[index];
}

/*! \brief Retrieve a const pointer to the quant-matrix for LOQ-0 based upon the scaling
 *         mode used. */
static inline const uint8_t* quantMatrixGetValuesConst(const LdeQuantMatrix* matrix, LdeLOQIndex index)
{
    if (!matrix || ((uint32_t)index >= LOQEnhancedCount)) {
        return NULL;
    }
    return matrix->values[index];
}

/*------------------------------------------------------------------------------*/

/*! \brief Contains dequantization settings for a single plane and LOQ. Must be aligned to 16bit
 *         boundaries, or else SSE generates a segfault */
#pragma pack(push, 16)
typedef struct Dequant
{
    int16_t stepWidth[TSCount][RCLayerCountDDS]; /**< Step-width per-temporal type per-layer. */
    int16_t offset[TSCount][RCLayerCountDDS];    /**< Offset per-temporal type per-layer. */

#if VN_CORE_FEATURE(SSE)
    __m128i stepWidthVector[TSCount][2]; /**< Step-widths packed into SIMD vector, maximum of 16-values. */
    __m128i offsetVector[TSCount][2]; /**< Offsets packed into SIMD vector, maximum of 16-values. */
#elif VN_CORE_FEATURE(NEON)
    int16x8_t stepWidthVector[TSCount][2];
    int16x8_t offsetVector[TSCount][2];
#endif
} Dequant;
#pragma pack(pop)

/*------------------------------------------------------------------------------*/

/*! \brief Calculates dequantization parameters to be used during decoding.
 *
 * \param[out] dequant       Destination for dequant params
 * \param[in]  globalConfig  Read config params from the global config.
 * \param[in]  frameConfig   Read config params from the frame config.
 * \param[in]  planeIdx      Plane index to get dequant parameters for (0-2)
 * \param[in]  loqIdx        LOQ index to get dequant parameters for (0-2)
 *
 * \return True on success, otherwise false.
 */
bool calculateDequant(Dequant* dequant, const LdeGlobalConfig* globalConfig,
                      const LdeFrameConfig* frameConfig, uint32_t planeIdx, LdeLOQIndex loqIdx);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_ENHANCEMENT_DEQUANT_H
