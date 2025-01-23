/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
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

#ifndef VN_DEC_CORE_APPLY_CMDBUFFER_APPLICATOR_H_
#define VN_DEC_CORE_APPLY_CMDBUFFER_APPLICATOR_H_

#include "common/cmdbuffer.h"
#include "common/tile.h"
#include "common/types.h"
#include "context.h"
#include "decode/apply_cmdbuffer_common.h"
#include "decode/transform_unit.h"

/*- Macros --------------------------------------------------------------------------------------*/

#define VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()                                              \
    const CmdBuffer_t* buffer = tile->cmdBuffer;                                            \
    const int32_t layerCount = buffer->layerCount;                                          \
    const int32_t layerSize = layerCount * sizeof(int16_t);                                 \
    const uint8_t tuWidthShift = (layerCount == 16) ? 2 : 1;                                \
    const TransformType_t transformType = (layerCount == 16) ? TransformDDS : TransformDD;  \
                                                                                            \
    const CmdBufferEntryPoint_t* entryPoint = &buffer->entryPoints[entryPointIdx];          \
    uint32_t tuIndex = entryPoint->initialJump;                                             \
    TUState_t tuState;                                                                      \
    if (tuStateInitialise(&tuState, (uint16_t)tile->width, (uint16_t)tile->height, tile->x, \
                          tile->y, tuWidthShift) < 0) {                                     \
        return false;                                                                       \
    }                                                                                       \
    tuIndex += tuCoordsBlockAlignedIndex(&tuState, tile->x, tile->y);                       \
    int32_t cmdOffset = entryPoint->commandOffset;                                          \
    int32_t dataOffset = entryPoint->dataOffset;                                            \
    uint8_t* dataPtr = NULL;

/*- Highlight -----------------------------------------------------------------------------------*/
/* Other residual application functions are defined differently for different SIMD implementations,
 * but we don't bother with highlight (since it's just a debug feature) */

#define VN_APPLY_CMDBUFFER_HIGHLIGHT(Pixel_t, transformSize, highlightArg)                     \
    Pixel_t* pixels = (Pixel_t*)args->surfaceData + (args->y * args->surfaceStride) + args->x; \
    const Pixel_t highlightValue = (Pixel_t)args->highlight->highlightArg;                     \
    for (int32_t row = 0; row < (transformSize); ++row) {                                      \
        for (int32_t column = 0; column < (transformSize); ++column) {                         \
            pixels[column] = highlightValue;                                                   \
        }                                                                                      \
        pixels += args->surfaceStride;                                                         \
    }

static void highlightDD_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 2, valUnsigned)
}

static void highlightDD_U16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint16_t, 2, valUnsigned)
}

static void highlightDD_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 2, valSigned)
}

static void highlightDDS_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 4, valUnsigned)
}

static void highlightDDS_U16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint16_t, 4, valUnsigned)
}

static void highlightDDS_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 4, valSigned)
}

/*- Constants -----------------------------------------------------------------------------------*/

static const ApplyCmdBufferFunction_t kHighlightTable[TransformCount][FPCount] = {
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

static const ApplyCmdBufferFunction_t kAddTable[TransformCount][FPCount] = {
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

static ApplyCmdBufferFunction_t getApplyFunction(CmdBufferCmd_t command, TransformType_t transformType,
                                                 FixedPoint_t fpType, bool highlight)
{
    switch (command) {
        case CBCAdd:
            return (highlight ? kHighlightTable[transformType][fpType] : kAddTable[transformType][fpType]);
        case CBCSet: return (transformType == TransformDD ? &setDD : &setDDS);
        case CBCSetZero: return (transformType == TransformDD ? &setZeroDD : &setZeroDDS);
        case CBCClear: break;
    }
    return &clear;
}

static uint32_t getJump(const uint8_t* commandPtr, int32_t* cmdOffsetOut)
{
    const uint8_t jumpSignal = *commandPtr & 0x3F;
    uint32_t jump = 0;
    if (jumpSignal < CBKBigJump) {
        jump = jumpSignal;
        (*cmdOffsetOut)++;
    } else if (jumpSignal == CBKBigJump) {
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
 * \param buffer        The buffer to apply.
 * \param entryPoint    An entrypoint to the buffer, for a single entrypoint pass zero offsets
 *                      and jumps with a correct count.
 * \param surface       Surface to apply to.
 * \param highlight     A highlight struct if using debugging with highlight residuals.
 */
bool cmdBufferApplicatorBlockTemplate(const TileState_t* tile, size_t entryPointIdx,
                                      const Surface_t* surface, const Highlight_t* highlight)
{
    VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()

    ApplyCmdBufferArgs_t args = {
        .surface = surface,
        .surfaceData = (int16_t*)surfaceGetLine(surface, 0),
        .surfaceStride = (uint16_t)surfaceGetStrideInPixels(surface),
        .x = 0,
        .y = 0,
        .highlight = highlight,
    };
    tuCoordsBlockAlignedRaster(&tuState, tuIndex, &args.x, &args.y);

    const size_t dataSize = cmdBufferGetDataSize(buffer);
    for (uint32_t count = 0; count < entryPoint->count; count++) {
        const uint8_t* commandPtr = buffer->data.start + cmdOffset;
        const CmdBufferCmd_t command = (const CmdBufferCmd_t)(*commandPtr & 0xC0);

        const uint32_t jump = getJump(commandPtr, &cmdOffset);
        tuIndex += jump;
        if (tuCoordsBlockAlignedRaster(&tuState, tuIndex, &args.x, &args.y) < 0) {
            return false;
        }

        if (args.x >= surface->width || args.y >= surface->height) {
            return false;
        }

        if (command == CBCAdd || command == CBCSet) {
            dataOffset += layerSize;
            dataPtr = buffer->data.currentData + dataSize - dataOffset;
            args.residuals = (int16_t*)dataPtr;
        }
        const ApplyCmdBufferFunction_t applyFn = getApplyFunction(
            command, transformType, surface->type, args.highlight && args.highlight->enabled);
        applyFn(&args);
    }
    return true;
}

/*! \brief This function is the loop to apply residuals in cmdbuffer surface format to a standard
 *         raster plane. It exists in this .h file separately as it is shared between the scalar,
 *         NEON and SSE implementations.
 *
 * \param buffer        The buffer to apply.
 * \param entryPoint    An entrypoint to the buffer, for a single entrypoint pass zero offsets
 *                      and jumps with a correct count.
 * \param surface       Surface to apply to.
 * \param highlight     A highlight struct if using debugging with highlight residuals.
 */
bool cmdBufferApplicatorSurfaceTemplate(const TileState_t* tile, size_t entryPointIdx,
                                        const Surface_t* surface, const Highlight_t* highlight)
{
    VN_COMMON_CMDBUFFER_APPLICATOR_SETUP()

    ApplyCmdBufferArgs_t args = {
        .surface = surface,
        .surfaceData = (int16_t*)surfaceGetLine(surface, 0),
        .surfaceStride = (uint16_t)surfaceGetStrideInPixels(surface),
        .x = 0,
        .y = 0,
        .highlight = highlight,
    };
    tuCoordsSurfaceRaster(&tuState, tuIndex, &args.x, &args.y);

    /* If we're applying in surface-rastered order, we know we're adding (because we only use this
     * order when temporal is disabled). So, the apply function is just the add function, or else
     * highlight. */
    const ApplyCmdBufferFunction_t applyFn =
        ((args.highlight && args.highlight->enabled) ? kHighlightTable[transformType][surface->type]
                                                     : kAddTable[transformType][surface->type]);

    const size_t dataSize = cmdBufferGetDataSize(buffer);
    for (uint32_t count = 0; count < entryPoint->count; count++) {
        const uint8_t* commandPtr = buffer->data.start + cmdOffset;

        const uint32_t jump = getJump(commandPtr, &cmdOffset);
        tuIndex += jump;
        if (tuCoordsSurfaceRaster(&tuState, tuIndex, &args.x, &args.y) < 0) {
            return false;
        }

        dataOffset += layerSize;
        dataPtr = buffer->data.currentData + dataSize - dataOffset;
        args.residuals = (int16_t*)dataPtr;
        applyFn(&args);
    }
    return true;
}

#endif // VN_DEC_CORE_APPLY_CMDBUFFER_APPLICATOR_H_
