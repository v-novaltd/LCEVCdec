/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_COMMON_H
#define VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_COMMON_H

#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/types.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferArgs
{
    int16_t* firstSample;
    uint16_t rowPixelStride; // This is a pixel stride not a byte stride, a 10bit 1920 row = 1920
    LdpFixedPoint fixedPoint;
    uint32_t x;
    uint32_t y;
    uint16_t width;
    uint16_t height;
    const int16_t* residuals;
    bool highlight;
} ApplyCmdBufferArgs;

typedef struct TileDesc
{
    uint16_t startX;
    uint16_t startY;
    uint16_t width;
    uint16_t height;
} TileDesc;

enum ApplyCmdBufferConstants
{
    ACBKBlockSize = 32,
};

/*------------------------------------------------------------------------------*/

typedef void (*ApplyCmdBufferFunction)(const ApplyCmdBufferArgs* args);

typedef bool (*CmdBufferApplicator)(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                    const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                    bool highlight);

bool cmdBufferApplicatorBlockScalar(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                    const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                    bool highlight);

bool cmdBufferApplicatorBlockNEON(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                  const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                  bool highlight);

bool cmdBufferApplicatorBlockSSE(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                 const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                 bool highlight);

bool cmdBufferApplicatorSurfaceScalar(const LdpEnhancementTile* enhancementTile,
                                      size_t entryPointIdx, const LdpPicturePlaneDesc* plane,
                                      LdpFixedPoint fixedPoint, bool highlight);

bool cmdBufferApplicatorSurfaceNEON(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                    const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                    bool highlight);

bool cmdBufferApplicatorSurfaceSSE(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                   const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint,
                                   bool highlight);

#define VN_UNUSED_CMDBUFFER_APPLICATOR() \
    VNUnused(enhancementTile);           \
    VNUnused(entryPointIdx);             \
    VNUnused(plane);                     \
    VNUnused(fixedPoint);                \
    VNUnused(highlight);                 \
    return false;

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_COMMON_H
