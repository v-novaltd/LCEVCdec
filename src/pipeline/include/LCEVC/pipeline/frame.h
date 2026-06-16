/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_FRAME_H
#define VN_LCEVC_PIPELINE_FRAME_H

#include <LCEVC/enhancement/config_types.h>
#include <LCEVC/pipeline/enhancement_tile.h>
#include <LCEVC/pipeline/picture.h>

// NOLINTBEGIN(modernize-use-using)

// Frame
//
// THe general description of a frame shared by all pipelines.
//
// Each pipeline will declare an implementation specific extension of this.
//
typedef struct LdpFrame
{
    uint64_t timestamp;

    // Shared configuration (global_config/sequence_config)
    LdeGlobalConfig* globalConfig;

    // Per frame configuration (picture_config)
    LdeFrameConfig config;

    // Input base picture
    LdpPicture* basePicture;

    // Output enhanced picture
    LdpPicture* outputPicture;

    // Deadline for this frame in microseconds relative to threadTimeMicroseconds()
    uint64_t deadline;

    // Base description
    uint32_t baseWidth;
    uint32_t baseHeight;
    uint8_t baseBitdepth;
    LdpColorFormat baseFormat;
    uint32_t baseSarNum;
    uint32_t baseSarDen;

    // Final info about decoded frame
    LdpDecodeInformation decodeInformation;

    // Userdata from SendBase
    void* userData;

    // 1 or more generated command buffers
    uint32_t enhancementTileCount;
    LdpEnhancementTile* enhancementTiles;
} LdpFrame;

// NOLINTEND(modernize-use-using)
#endif // VN_LCEVC_PIPELINE_FRAME_H
