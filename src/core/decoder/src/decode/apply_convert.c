/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "decode/apply_convert.h"

#include "common/cmdbuffer.h"
#include "common/tile.h"
#include "decode/transform_unit.h"
#include "surface/surface.h"

#include <assert.h>

typedef struct ConvertArgs
{
    const Surface_t* src;
    int32_t srcSkip;
    int32_t srcOffset;
    Surface_t* dst;
    int32_t dstSkip;
    int32_t dstOffset;
} ConvertArgs_t;

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DD transform */
static void convertDD_S87_S8(const ConvertArgs_t* args, int32_t x, int32_t y)
{
    const Surface_t* src = args->src;
    Surface_t* dst = args->dst;
    uint8_t* dstPels = (uint8_t*)dst->data;
    const int16_t* srcPels = (const int16_t*)src->data;
    const int32_t dstStride = (int32_t)dst->stride;
    const int32_t srcStride = (int32_t)src->stride;
    const int32_t dstSkip = args->dstSkip;
    const int32_t srcSkip = args->srcSkip;
    const uint32_t dstOffset = args->dstOffset + (x * dstSkip) + (y * dstStride);
    const uint32_t srcOffset = args->srcOffset + (x * srcSkip) + (y * srcStride);

    assert(src->type == FPS8);

    /* clang-format off */
	dstPels[dstOffset]                       = (uint8_t)(srcPels[srcOffset] >> 8);
	dstPels[dstOffset + dstSkip]             = (uint8_t)(srcPels[srcOffset + srcSkip] >> 8);
	dstPels[dstOffset + dstStride]           = (uint8_t)(srcPels[srcOffset + srcStride] >> 8);
	dstPels[dstOffset + dstSkip + dstStride] = (uint8_t)(srcPels[srcOffset + srcSkip + srcStride] >> 8);
    /* clang-format on */
}

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DDS transform */
static void convertDDS_S87_S8(const ConvertArgs_t* args, int32_t x, int32_t y)
{
    const Surface_t* src = args->src;
    Surface_t* dst = args->dst;
    uint8_t* dstPels = (uint8_t*)dst->data;
    const int16_t* srcPels = (const int16_t*)src->data;
    const int32_t dstStride = (int32_t)dst->stride;
    const int32_t srcStride = (int32_t)src->stride;
    const int32_t dstSkip = args->dstSkip;
    const int32_t srcSkip = args->srcSkip;
    const int32_t dstOffset = args->dstOffset + (x * dstSkip) + (y * dstStride);
    const int32_t srcOffset = args->srcOffset + (x * srcSkip) + (y * srcStride);

    assert(src->type == FPS8);

    /* clang-format off */
	dstPels[dstOffset]                               = (uint8_t)(srcPels[srcOffset] >> 8);
	dstPels[dstOffset + dstSkip]                     = (uint8_t)(srcPels[srcOffset + srcSkip] >> 8);
	dstPels[dstOffset + dstStride]                   = (uint8_t)(srcPels[srcOffset + srcStride] >> 8);
	dstPels[dstOffset + dstSkip + dstStride]         = (uint8_t)(srcPels[srcOffset + srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip]                 = (uint8_t)(srcPels[srcOffset + 2 * srcSkip] >> 8);
	dstPels[dstOffset + 3 * dstSkip]                 = (uint8_t)(srcPels[srcOffset + 3 * srcSkip] >> 8);
	dstPels[dstOffset + 2 * dstSkip + dstStride]     = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + dstStride]     = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 2 * dstStride]               = (uint8_t)(srcPels[srcOffset + 2 * srcStride] >> 8);
	dstPels[dstOffset + dstSkip + 2 * dstStride]     = (uint8_t)(srcPels[srcOffset + srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstStride]               = (uint8_t)(srcPels[srcOffset + 3 * srcStride] >> 8);
	dstPels[dstOffset + dstSkip + 3 * dstStride]     = (uint8_t)(srcPels[srcOffset + srcSkip + 3 * srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + 3 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + 3 * srcStride] >> 8);
    /* clang-format on */
}

/*------------------------------------------------------------------------------*/

bool applyConvert(const TileState_t* tile, const Surface_t* src, Surface_t* dst, bool temporalEnabled)
{
    const CmdBuffer_t* buffer = tile->cmdBuffer;
    const int32_t layerCount = buffer->layerCount;
    const uint8_t tuWidthShift = (layerCount == 16) ? 2 : 1;
    const TransformType_t transformType = (layerCount == 16) ? TransformDDS : TransformDD;

    uint32_t tuIndex = 0;
    TUState_t tuState;
    if (tuStateInitialise(&tuState, (uint16_t)tile->width, (uint16_t)tile->height, tile->x, tile->y,
                          tuWidthShift) < 0) {
        return false;
    }
    tuIndex += tuCoordsBlockAlignedIndex(&tuState, tile->x, tile->y);
    int32_t cmdOffset = 0;
    uint32_t x = 0;
    uint32_t y = 0;

    const ConvertArgs_t args = {
        .src = src,
        .srcSkip = 1,
        .srcOffset = 0,
        .dst = dst,
        .dstSkip = 1,
        .dstOffset = 0,
    };

    for (uint32_t count = 0; count < buffer->count; count++) {
        const uint8_t* commandPtr = buffer->data.start + cmdOffset;
        const CmdBufferCmd_t command = (const CmdBufferCmd_t)(*commandPtr & 0xC0);
        const uint8_t jumpSignal = *commandPtr & 0x3F;
        uint32_t jump = 0;
        if (jumpSignal < CBKBigJump) {
            jump = jumpSignal;
            cmdOffset++;
        } else if (jumpSignal == CBKBigJump) {
            jump = commandPtr[1] + (commandPtr[2] << 8);
            cmdOffset += 3;
        } else {
            jump = commandPtr[1] + (commandPtr[2] << 8) + (commandPtr[3] << 16);
            cmdOffset += 4;
        }
        tuIndex += jump;
        if (temporalEnabled) {
            if (tuCoordsBlockAlignedRaster(&tuState, tuIndex, &x, &y) < 0) {
                return false;
            }
        } else {
            if (tuCoordsSurfaceRaster(&tuState, tuIndex, &x, &y) < 0) {
                return false;
            }
        }

        if (temporalEnabled && command == CBCClear) {
            /* For temporal surfaces, run clear blocks on the U8 'dst' surface */
            const uint16_t clearHeight = minU16(BSTemporal, (uint16_t)(dst->height - y));
            const uint16_t clearWidth = minU16(BSTemporal, (uint16_t)(dst->width - x));
            const size_t clearBytes = clearWidth * sizeof(uint8_t);

            uint8_t* pixels = dst->data + (y * dst->stride) + x;

            for (int32_t row = 0; row < clearHeight; ++row) {
                memset(pixels, 0, clearBytes);
                pixels += dst->stride;
            }
        }

        /* Copy residuals from the previously applied 'src' surface to the 'dst' with conversion to U8 */
        if (command != CBCClear) {
            if (transformType == TransformDD) {
                convertDD_S87_S8(&args, (int32_t)x, (int32_t)y);
            } else {
                convertDDS_S87_S8(&args, (int32_t)x, (int32_t)y);
            }
        }
    }
    return true;
}

/*------------------------------------------------------------------------------*/