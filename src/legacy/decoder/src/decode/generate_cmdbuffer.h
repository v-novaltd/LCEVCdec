/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#ifndef VN_LCEVC_LEGACY_GENERATE_CMDBUFFER_H
#define VN_LCEVC_LEGACY_GENERATE_CMDBUFFER_H

#include "common/platform.h"

/*! \file
 *
 *   This module implements the functionality for taking intermediate entropy decoded
 *   coefficients/runs and building commands buffers with complete transforms worth of
 *   residual data (and temporal tile clear signals when reduced signaling is enabled).
 */

/*------------------------------------------------------------------------------*/

typedef struct BlockClearJumps* BlockClearJumps_t;
typedef struct CmdBuffer Cmdbuffer_t;
typedef struct DecodeParallel* DecodeParallel_t;
typedef struct DecodeParallelArgs DecodeParallelArgs_t;
typedef struct DeserialisedData DeserialisedData_t;
typedef struct TileState TileState_t;
typedef struct TransformCoeffs* TransformCoeffs_t;
typedef struct TUState TUState_t;

/*------------------------------------------------------------------------------*/

void generateCommandBuffers(const DeserialisedData_t* data, const DecodeParallelArgs_t* args,
                            Cmdbuffer_t* cmdbuffer, int32_t planeIndex, TransformCoeffs_t* coeffs,
                            TransformCoeffs_t temporalCoeffs, const BlockClearJumps_t blockClears,
                            const TUState_t* tuState);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_LEGACY_GENERATE_CMDBUFFER_H
