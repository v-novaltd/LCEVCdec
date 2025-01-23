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

#include "LCEVC/tonemap.h"

#include "buffer_read_write.h"

#include <LCEVC/utility/linear_math.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void tonemapLut(DVec4* rgbaInOut, const double lutScale, const float* tonemappingLutArr, const double maxD)
{
    for (uint8_t channel = 0; channel < 4; channel++) {
        double idx = (*rgbaInOut)[channel] * lutScale / maxD;
        int32_t floorIndex = (int32_t)floor(idx);
        int32_t ceilIndex = floorIndex + 1;
        int32_t floorIndexClamped = (int32_t)clamp(floorIndex, 0, (int32_t)lutScale);
        int32_t ceilIndexClamped = (int32_t)clamp(ceilIndex, 0, (int32_t)lutScale);
        double lutVal = tonemappingLutArr[floorIndexClamped] * ((double)ceilIndex - idx) +
                        tonemappingLutArr[ceilIndexClamped] * (idx - (double)floorIndex);
        (*rgbaInOut)[channel] = lutVal * maxD;
    }
}

bool LCEVC_tonemap(perseus_image* dst, const uint8_t dstChromaHorizontalShift,
                   const uint8_t dstChromaVerticalShift, const perseus_image* src,
                   const uint8_t srcChromaHorizontalShift, const uint8_t srcChromaVerticalShift,
                   uint32_t startRow, uint32_t endRow, const double inYuvToRgb709[16],
                   const double rgb2020ToOutYuv[16], const double BT709toBT2020[16],
                   const float* tonemappingLutArr, size_t tonemappingLutArrLen)
{
    if (dst == NULL || src == NULL) {
        return false;
    }
    // if src is not rgb, we need the input conversion
    if (!perseus_is_rgb(src->ilv) && (inYuvToRgb709 == NULL)) {
        return false;
    }
    // if dst is not rgb, we need the output conversion
    if (!perseus_is_rgb(src->ilv) && (rgb2020ToOutYuv == NULL)) {
        return false;
    }
    // if we have a tonemapping lookup-table array, it must have size >0
    if ((tonemappingLutArr != NULL) && (tonemappingLutArrLen == 0)) {
        return false;
    }
    // Validate later left-bitshifts
    const uint8_t inBitDepth = perseus_get_bitdepth(src->depth);
    if (inBitDepth >= 8 * sizeof(uint16_t) || dstChromaHorizontalShift >= 8 * sizeof(uint8_t) ||
        dstChromaVerticalShift >= 8 * sizeof(uint8_t) || srcChromaHorizontalShift >= 8 * sizeof(uint8_t) ||
        srcChromaVerticalShift >= 8 * sizeof(uint8_t)) {
        return false;
    }

    // Gather several variables that don't change per-iteration in the loop
    const double lutScale = (double)(tonemappingLutArrLen)-1.0;
    const int16_t inMax = (int16_t)(1 << inBitDepth) - 1;
    // bytes per pixel:
    const uint8_t inByteDepth = (inBitDepth + 7) / 8;
    const uint8_t outByteDepth = (perseus_get_bitdepth(dst->depth) + 7) / 8;
    // bytes per row:
    const uint32_t inByteStrides[VN_IMAGE_NUM_PLANES] = {
        src->stride[0] * inByteDepth,
        src->stride[1] * inByteDepth,
        src->stride[2] * inByteDepth,
    };
    const uint32_t outByteStrides[VN_IMAGE_NUM_PLANES] = {
        dst->stride[0] * outByteDepth,
        dst->stride[1] * outByteDepth,
        dst->stride[2] * outByteDepth,
    };

    const uint8_t inSubsampleHorizontal = (uint8_t)(1 << srcChromaHorizontalShift);
    const uint8_t outSubsampleHorizontal = (uint8_t)(1 << dstChromaHorizontalShift);
    const uint8_t outSubsampleVertical = (uint8_t)(1 << dstChromaVerticalShift);
    const uint8_t inComponentsInPlane0 = getNumComponentsInPlane0(src->ilv);
    const uint8_t outComponentsInPlane0 = getNumComponentsInPlane0(dst->ilv);
    const bool inIsRgb = perseus_is_rgb(src->ilv);
    const bool outIsRgb = perseus_is_rgb(dst->ilv);

    // Temporary vectors to hold each row. This allows us to tonemap in-place without accidentally
    // eating our own outputs from one row to the next (for example, row 1's chroma values should
    // be the same as row 0's, but they won't be if they're already tonemapped).
    uint8_t* dstHolderLuma = malloc(outByteStrides[0]);
    uint8_t* dstHolderU = malloc(outByteStrides[1]);
    uint8_t* dstHolderV = malloc(outByteStrides[2]);

    // Use doubles for rgbs, to preserve precision until converting back to yuv
    I16Vec4 yuvaInOut = {0, 0, 0, inMax};
    DVec4 rgba709 = {0.0, 0.0, 0.0, (double)inMax};
    DVec4 rgba2020 = {0.0, 0.0, 0.0, (double)inMax};
    DVec4* rgbaToTonemap = (BT709toBT2020 == NULL ? &rgba709 : &rgba2020);
    for (uint32_t y = startRow; y < endRow; y++) {
        uint8_t* dstRow0 = planeBufferRow(dst->plane, outByteStrides, 0, 0, y);
        uint8_t* dstRowU = planeBufferRow(dst->plane, outByteStrides, dstChromaHorizontalShift, 1, y);
        uint8_t* dstRowV = planeBufferRow(dst->plane, outByteStrides, dstChromaHorizontalShift, 2, y);
        const uint8_t* srcRow0 = planeBufferRowConst(src->plane, inByteStrides, 0, 0, y);
        const uint8_t* srcRowU =
            planeBufferRowConst(src->plane, inByteStrides, srcChromaVerticalShift, 1, y);
        const uint8_t* srcRowV =
            planeBufferRowConst(src->plane, inByteStrides, srcChromaVerticalShift, 2, y);

        for (uint32_t x = 0; x < src->stride[0]; x++) {
            // Gather input
            if (inIsRgb) {
                rgba709[0] = (double)(srcRow0[x * inByteDepth * inComponentsInPlane0]);
                rgba709[1] = (double)(srcRow0[x * inByteDepth * inComponentsInPlane0 + 1]);
                rgba709[2] = (double)(srcRow0[x * inByteDepth * inComponentsInPlane0 + 2]);
                if (inComponentsInPlane0 == 4) {
                    rgba709[3] = (double)(srcRow0[x * inByteDepth * inComponentsInPlane0 + 3]);
                }
            } else {
                const uint32_t subsampleX = x / inSubsampleHorizontal;
                // Collect by memcpying (this allows 10bit bases).
                memcpy(&yuvaInOut[0], &srcRow0[x * inByteDepth], inByteDepth);
                memcpy(&yuvaInOut[1], &srcRowU[subsampleX * inByteDepth], inByteDepth);
                memcpy(&yuvaInOut[2], &srcRowV[subsampleX * inByteDepth], inByteDepth);

                // yuv->rgb
                mat4x4_mul_I16Vec4_to_DVec4(rgba709, inYuvToRgb709, yuvaInOut);
            }

            // rgb709 -> rgb2020
            if (BT709toBT2020 != NULL) {
                mat4x4_mul_DVec4_to_DVec4(rgba2020, BT709toBT2020, rgba709);
            }

            // tonemap (with linear interpolation).
            if (tonemappingLutArr != NULL) {
                tonemapLut(rgbaToTonemap, lutScale, tonemappingLutArr, (double)inMax);
            }

            // Clamp and copy into destination arrays
            if (outIsRgb) {
                for (uint8_t channel = 0; channel < outComponentsInPlane0; channel++) {
                    (*rgbaToTonemap)[channel] = clamp((*rgbaToTonemap)[channel], 0.0, (double)inMax);
                    writeU16ToBuffer(dstHolderLuma, (uint16_t)(*rgbaToTonemap)[0], dst->depth,
                                     outComponentsInPlane0 * x + channel);
                }
            } else {
                const uint32_t subsampleX = x / outSubsampleHorizontal;
                // Convert rgb->yuv
                mat4x4_mul_DVec4_to_I16Vec4(yuvaInOut, rgb2020ToOutYuv, (*rgbaToTonemap));

                for (uint8_t plane = 0; plane < 4; plane++) {
                    // Note that we still clamp to inMax: we're not in the output bit range until
                    // AFTER we call writeU16ToBuffer
                    yuvaInOut[plane] = (uint16_t)clamp(yuvaInOut[plane], 0.0, (double)inMax);
                }
                writeU16ToBuffer(dstHolderLuma, yuvaInOut[0], dst->depth, x);
                writeU16ToBuffer(dstHolderU, yuvaInOut[1], dst->depth, subsampleX);
                writeU16ToBuffer(dstHolderV, yuvaInOut[2], dst->depth, subsampleX);
            }
        }

        // Copy back from dest holders into original image.
        memcpy(dstRow0, dstHolderLuma, outByteStrides[0]);
        if (!outIsRgb && ((y % outSubsampleVertical) == (outSubsampleVertical - 1U))) {
            memcpy(dstRowU, dstHolderU, outByteStrides[1]);
            memcpy(dstRowV, dstHolderV, outByteStrides[2]);
        }
    }

    free(dstHolderLuma);
    free(dstHolderU);
    free(dstHolderV);

    return true;
}
