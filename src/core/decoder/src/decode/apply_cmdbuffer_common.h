/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_
#define VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_

#include "lcevc_config.h"
#include "surface/surface.h"

#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct CmdBuffer CmdBuffer_t;
typedef struct CmdBufferEntryPoint CmdBufferEntryPoint_t;
typedef struct Highlight Highlight_t;
typedef struct TileState TileState_t;

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferArgs
{
    const Surface_t* surface;
    int16_t* surfaceData;
    uint16_t surfaceStride;
    uint32_t x;
    uint32_t y;
    const int16_t* residuals;
    const Highlight_t* highlight;
} ApplyCmdBufferArgs_t;

/*------------------------------------------------------------------------------*/

typedef void (*ApplyCmdBufferFunction_t)(const ApplyCmdBufferArgs_t* args);

typedef bool (*CmdBufferApplicator_t)(const TileState_t* tile, size_t entryPointIdx,
                                      const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorBlockScalar(const TileState_t* tile, size_t entryPointIdx,
                                    const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorBlockNEON(const TileState_t* tile, size_t entryPointIdx,
                                  const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorBlockSSE(const TileState_t* tile, size_t entryPointIdx,
                                 const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorSurfaceScalar(const TileState_t* tile, size_t entryPointIdx,
                                      const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorSurfaceNEON(const TileState_t* tile, size_t entryPointIdx,
                                    const Surface_t* surface, const Highlight_t* highlight);

bool cmdBufferApplicatorSurfaceSSE(const TileState_t* tile, size_t entryPointIdx,
                                   const Surface_t* surface, const Highlight_t* highlight);

#define VN_UNUSED_CMDBUFFER_APPLICATOR() \
    VN_UNUSED(tile);                     \
    VN_UNUSED(entryPointIdx);            \
    VN_UNUSED(surface);                  \
    VN_UNUSED(highlight);                \
    return false;

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_ */
