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

#ifndef VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_APPLICATOR_H
#define VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_APPLICATOR_H

#include "apply_cmdbuffer_common.h"

#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/transform_unit.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/types.h>
#include <stdbool.h>
#include <stdio.h>

/*- Macros --------------------------------------------------------------------------------------*/

#define VN_PLANE_GETLINE(pl, offset) (pl->firstSample + (offset * pl->rowByteStride))

#define VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()                                                         \
    const LdeCmdBufferCpu* cmdBuffer = &enhancementTile->buffer;                                       \
    const int32_t transformSize = cmdBuffer->transformSize;                                            \
    const int32_t layerSize = transformSize * sizeof(int16_t);                                         \
    const uint8_t tuWidthShift = (transformSize == 16) ? 2 : 1;                                        \
    const LdeTransformType transformType = (transformSize == 16) ? TransformDDS : TransformDD;         \
                                                                                                       \
    const LdeCmdBufferCpuEntryPoint* entryPoint = &cmdBuffer->entryPoints[entryPointIdx];              \
    uint32_t tuIndex = entryPoint->initialJump;                                                        \
    TUState tuState;                                                                                   \
    if (!ldeTuStateInitialize(&tuState, enhancementTile->tileWidth, enhancementTile->tileHeight,       \
                              enhancementTile->tileX, enhancementTile->tileY, tuWidthShift)) {         \
        return false;                                                                                  \
    }                                                                                                  \
    tuIndex += ldeTuCoordsBlockAlignedIndex(&tuState, enhancementTile->tileX, enhancementTile->tileY); \
    int32_t cmdOffset = entryPoint->commandOffset;                                                     \
    int32_t dataOffset = entryPoint->dataOffset;                                                       \
    uint8_t* dataPtr = NULL;                                                                           \
    uint16_t rowPixelStride = (bitdepthFromFixedPoint(fixedPoint) > 8) ? plane->rowByteStride >> 1     \
                                                                       : plane->rowByteStride;         \
                                                                                                       \
    ApplyCmdBufferArgs args = {                                                                        \
        .firstSample = (int16_t*)VN_PLANE_GETLINE(plane, 0),                                           \
        .rowPixelStride = rowPixelStride,                                                              \
        .x = 0,                                                                                        \
        .y = 0,                                                                                        \
        .width = enhancementTile->planeWidth,                                                          \
        .height = enhancementTile->planeHeight,                                                        \
        .highlight = highlight,                                                                        \
        .fixedPoint = fixedPoint,                                                                      \
    };

/*- Highlight -----------------------------------------------------------------------------------*/
/* Other residual application functions are defined differently for different SIMD implementations,
 * but we don't bother with highlight (since it's just a debug feature) */

#define VN_APPLY_CMDBUFFER_HIGHLIGHT(PixelType, transformWidth)                                     \
    PixelType highlightValue = (PixelType)fixedPointHighlightValue(args->fixedPoint);               \
    PixelType* pixels = (PixelType*)args->firstSample + (args->y * args->rowPixelStride) + args->x; \
    for (int32_t row = 0; row < (transformWidth); ++row) {                                          \
        for (int32_t column = 0; column < (transformWidth); ++column) {                             \
            pixels[column] = highlightValue;                                                        \
        }                                                                                           \
        pixels += args->rowPixelStride;                                                             \
    }

static void highlightDD_U8(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 2)
}

static void highlightDD_U16(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 2)
}

static void highlightDD_S16(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 2)
}

static void highlightDDS_U8(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 4)
}

static void highlightDDS_U16(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 4)
}

static void highlightDDS_S16(const ApplyCmdBufferArgs* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 4)
}

/*- Constants -----------------------------------------------------------------------------------*/

static const ApplyCmdBufferFunction kHighlightTable[TransformCount][LdpFPCount] = {
    {
        highlightDD_U8,
        highlightDD_U16,
        highlightDD_U16,
        highlightDD_U16,
        highlightDD_S16,
        highlightDD_S16,
        highlightDD_S16,
        highlightDD_S16,
    },
    {
        highlightDDS_U8,
        highlightDDS_U16,
        highlightDDS_U16,
        highlightDDS_U16,
        highlightDDS_S16,
        highlightDDS_S16,
        highlightDDS_S16,
        highlightDDS_S16,
    },
};

static const ApplyCmdBufferFunction kAddTable[TransformCount][LdpFPCount] = {
    {
        addDD_U8,
        addDD_U10,
        addDD_U12,
        addDD_U14,
        addDD_S16,
        addDD_S16,
        addDD_S16,
        addDD_S16,
    },
    {
        addDDS_U8,
        addDDS_U10,
        addDDS_U12,
        addDDS_U14,
        addDDS_S16,
        addDDS_S16,
        addDDS_S16,
        addDDS_S16,
    },
};

/*- Functions -----------------------------------------------------------------------------------*/

static ApplyCmdBufferFunction getApplyFunction(LdeCmdBufferCpuCmd command, LdeTransformType transformType,
                                               LdpFixedPoint fpType, bool highlight)
{
    if (highlight) {
        if (command == CBCCClear) {
            return clear;
        }
        return kHighlightTable[transformType][fpType];
    }
    switch (command) {
        case CBCCAdd: return kAddTable[transformType][fpType];
        case CBCCSet: return transformType == TransformDD ? &setDD : &setDDS;
        case CBCCSetZero: return transformType == TransformDD ? &setZeroDD : &setZeroDDS;
        case CBCCClear: break;
    }
    return &clear;
}

static uint32_t getJump(const uint8_t* commandPtr, int32_t* cmdOffsetOut)
{
    const uint8_t jumpSignal = *commandPtr & 0x3F;
    uint32_t jump = 0;
    if (jumpSignal < CBCKBigJumpSignal) {
        jump = jumpSignal;
        (*cmdOffsetOut)++;
    } else if (jumpSignal == CBCKBigJumpSignal) {
        jump = commandPtr[1] + (commandPtr[2] << 8);
        (*cmdOffsetOut) += 3;
    } else {
        jump = commandPtr[1] + (commandPtr[2] << 8) + (commandPtr[3] << 16);
        (*cmdOffsetOut) += 4;
    }
    return jump;
}

/*! \brief This function is the loop to apply residuals in cmdbuffer temporal format to a standard
 *         raster plane. It exists in this .h file separately as it is shared between the scalar,
 *         NEON and SSE implementations.
 *
 * \param enhancementTile Cmdbuffer and tile location to apply to
 * \param entryPointIdx   An entrypoint index to apply
 * \param plane           Plane to apply to.
 * \param fixedPoint      Plane datatype
 * \param highlight       Set true to use highlight residual functions instead of ADD, SET and
 *                        SETZERO. Highlight mode is not SIMD optimized. */
bool cmdBufferApplicatorBlockTemplate(const LdpEnhancementTile* enhancementTile,
                                      size_t entryPointIdx, const LdpPicturePlaneDesc* plane,
                                      LdpFixedPoint fixedPoint, bool highlight)
{
    VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()

    ldeTuCoordsBlockAlignedRaster(&tuState, tuIndex, &args.x, &args.y);

    const size_t dataSize = ldeCmdBufferCpuGetResidualSize(cmdBuffer);
    for (uint32_t count = 0; count < entryPoint->count; count++) {
        const uint8_t* commandPtr = cmdBuffer->data.start + cmdOffset;
        const LdeCmdBufferCpuCmd command = (const LdeCmdBufferCpuCmd)(*commandPtr & 0xC0);

        const uint32_t jump = getJump(commandPtr, &cmdOffset);
        tuIndex += jump;
        ldeTuCoordsBlockAlignedRaster(&tuState, tuIndex, &args.x, &args.y);

        assert(args.x < args.width && args.y < args.height);

        if (command == CBCCAdd || command == CBCCSet) {
            dataOffset += layerSize;
            dataPtr = cmdBuffer->data.currentResidual + dataSize - dataOffset;
            args.residuals = (int16_t*)dataPtr;
        }
        const ApplyCmdBufferFunction applyFn =
            getApplyFunction(command, transformType, fixedPoint, args.highlight);
        applyFn(&args);
    }
    return true;
}

/*! \brief This function is the loop to apply residuals in cmdbuffer surface format to a standard
 *         raster plane. It exists in this .h file separately as it is shared between the scalar,
 *         NEON and SSE implementations.
 *
 * \param enhancementTile Cmdbuffer and tile location to apply to
 * \param entryPointIdx   An entrypoint index to apply
 * \param plane           Plane to apply to.
 * \param fixedPoint      Plane datatype
 * \param highlight       Set true to use highlight residual functions instead of ADD, SET and
 *                        SETZERO. Highlight mode is not SIMD optimized.
 */
bool cmdBufferApplicatorSurfaceTemplate(const LdpEnhancementTile* enhancementTile,
                                        size_t entryPointIdx, const LdpPicturePlaneDesc* plane,
                                        LdpFixedPoint fixedPoint, bool highlight)
{
    VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()

    ldeTuCoordsSurfaceRaster(&tuState, tuIndex, &args.x, &args.y);

    /* If we're applying in surface-raster order, we know we're adding (because we only use this
     * order when temporal is disabled). So, the apply function is just the add function, or else
     * highlight. */
    const ApplyCmdBufferFunction applyFn = args.highlight ? kHighlightTable[transformType][fixedPoint]
                                                          : kAddTable[transformType][fixedPoint];

    const size_t dataSize = ldeCmdBufferCpuGetResidualSize(cmdBuffer);
    for (uint32_t count = 0; count < entryPoint->count; count++) {
        const uint8_t* commandPtr = cmdBuffer->data.start + cmdOffset;

        const uint32_t jump = getJump(commandPtr, &cmdOffset);
        tuIndex += jump;
        TuStateReturn ret = ldeTuCoordsSurfaceRaster(&tuState, tuIndex, &args.x, &args.y);
        if (ret == TUError) {
            return false;
        }
        assert(args.x < args.width && args.y < args.height);

        dataOffset += layerSize;
        dataPtr = cmdBuffer->data.currentResidual + dataSize - dataOffset;
        args.residuals = (int16_t*)dataPtr;
        applyFn(&args);
    }
    return true;
}

#endif // VN_LCEVC_PIXEL_PROCESSING_APPLY_CMDBUFFER_APPLICATOR_H
