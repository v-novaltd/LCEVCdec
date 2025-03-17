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

#include "LCEVC/color_convert.h"

#include "buffer_read_write.h"

#include <LCEVC/api_utility/linear_math.h>

bool LCEVC_rgbToYuv(perseus_image* dstYuv, const perseus_image* srcRgb, uint16_t startRow, uint16_t endRow,
                    const double rgbToYuvMatrix[16], const double colorspaceConversion[16])
{
    if (dstYuv == NULL || srcRgb == NULL) {
        return false;
    }
    if (!perseus_is_rgb(srcRgb->ilv) || perseus_is_rgb(dstYuv->ilv)) {
        return false;
    }
    // Validate later left-bitshifts
    const uint8_t inBitDepth = perseus_get_bitdepth(srcRgb->depth);
    const uint8_t outBitDepth = perseus_get_bitdepth(dstYuv->depth);
    const uint8_t chromaHorizontalShift = (dstYuv->ilv == PSS_ILV_NV12 ? 1 : 0);
    const uint8_t chromaVerticalShift = (dstYuv->ilv == PSS_ILV_NV12 ? 1 : 0);
    if (chromaHorizontalShift >= sizeof(uint8_t) || chromaVerticalShift >= sizeof(uint8_t) ||
        inBitDepth >= sizeof(uint16_t) || outBitDepth >= sizeof(uint16_t)) {
        return false;
    }

    // Gather several variables that don't change per-iteration in the loop
    const uint8_t inByteDepth = (inBitDepth + 7) / 8; // bytes per pixel
    const uint8_t outByteDepth = (outBitDepth + 7) / 8;
    const uint32_t inByteStrides[VN_IMAGE_NUM_PLANES] = {srcRgb->stride[0] * inByteDepth,
                                                         srcRgb->stride[1] * inByteDepth,
                                                         srcRgb->stride[2] * inByteDepth};
    const uint32_t outByteStrides[VN_IMAGE_NUM_PLANES] = {srcRgb->stride[0] * outByteDepth,
                                                          srcRgb->stride[1] * outByteDepth,
                                                          srcRgb->stride[2] * outByteDepth};
    const int16_t inMax = (int16_t)(1 << inBitDepth) - 1;
    const int16_t outMax = (int16_t)(1 << outBitDepth) - 1;
    const uint8_t subsampleHorizontal = (uint8_t)(1 << chromaHorizontalShift);
    const uint8_t subsampleVertical = (uint8_t)(1 << chromaVerticalShift);
    const uint8_t inComponentsInPlane0 = getNumComponentsInPlane0(srcRgb->ilv);

    // Use doubles for rgbs, to preserve precision until converting back to yuv
    DVec4 rgbaPreConvert = {0.0, 0.0, 0.0, (double)inMax};
    DVec4 rgbaPostConvert = {0.0, 0.0, 0.0, (double)inMax};
    DVec4* rgbaToMakeYuv = (colorspaceConversion == NULL ? &rgbaPreConvert : &rgbaPostConvert);
    I16Vec4 yuva = {0, 0, 0, 0};
    for (uint32_t y = startRow; y < endRow; y++) {
        const uint8_t* srcRow = planeBufferRowConst(srcRgb->plane, inByteStrides, 0, 0, y);
        uint8_t* dstRow0 = planeBufferRow(dstYuv->plane, outByteStrides, 0, 0, y);
        uint8_t* dstRowU = planeBufferRow(dstYuv->plane, outByteStrides, chromaVerticalShift, 1, y);
        uint8_t* dstRowV = planeBufferRow(dstYuv->plane, outByteStrides, chromaVerticalShift, 2, y);

        for (uint32_t x = 0; x < srcRgb->stride[0]; x += inComponentsInPlane0) {
            // collect rgba
            rgbaPreConvert[0] = (double)(srcRow[x * inByteDepth * inComponentsInPlane0]);
            rgbaPreConvert[1] = (double)(srcRow[x * inByteDepth * inComponentsInPlane0 + 1]);
            rgbaPreConvert[2] = (double)(srcRow[x * inByteDepth * inComponentsInPlane0 + 2]);
            if (inComponentsInPlane0 == 4) {
                rgbaPreConvert[3] = (double)(srcRow[x * inByteDepth * inComponentsInPlane0 + 3]);
            }

            // colorspace conversion
            if (colorspaceConversion != NULL) {
                mat4x4_mul_DVec4_to_DVec4(rgbaPostConvert, colorspaceConversion, rgbaPreConvert);
            }

            // rgb->yuv
            mat4x4_mul_DVec4_to_I16Vec4(yuva, rgbToYuvMatrix, *rgbaToMakeYuv);

            // Clamp and copy into destination arrays. (
            for (uint8_t plane = 0; plane < 4; plane++) {
                yuva[plane] = (uint16_t)clamp(yuva[plane], 0.0, outMax);
            }

            writeU16ToBuffer(dstRow0, yuva[0], dstYuv->depth, x);
            if ((y % subsampleVertical) == (subsampleVertical - 1U)) {
                const uint32_t subsampleX = x / subsampleHorizontal;
                writeU16ToBuffer(dstRowU, yuva[1], dstYuv->depth, subsampleX);
                writeU16ToBuffer(dstRowV, yuva[2], dstYuv->depth, subsampleX);
            }
        }
    }

    return true;
}

bool LCEVC_yuvToRgb(perseus_image* dstRgb, const perseus_image* srcYuv, uint16_t startRow, uint16_t endRow,
                    const double yuvToRgbMatrix[16], const double colorspaceConversion[16])
{
    if (dstRgb == NULL || srcYuv == NULL) {
        return false;
    }
    if (!perseus_is_rgb(dstRgb->ilv) || perseus_is_rgb(srcYuv->ilv)) {
        return false;
    }
    // Validate later left-bitshifts
    const uint8_t inBitDepth = perseus_get_bitdepth(srcYuv->depth);
    const uint8_t outBitDepth = perseus_get_bitdepth(dstRgb->depth);
    const uint8_t chromaHorizontalShift = (srcYuv->ilv == PSS_ILV_NV12 ? 1 : 0);
    const uint8_t chromaVerticalShift = (srcYuv->ilv == PSS_ILV_NV12 ? 1 : 0);
    if (chromaHorizontalShift >= sizeof(uint8_t) || chromaVerticalShift >= sizeof(uint8_t) ||
        inBitDepth >= sizeof(uint16_t) || outBitDepth >= sizeof(uint16_t)) {
        return false;
    }

    // Gather several variables that don't change per-iteration in the loop
    const uint8_t inByteDepth = (inBitDepth + 7) / 8; // bytes per pixel
    const uint8_t outByteDepth = (outBitDepth + 7) / 8;
    const uint32_t inByteStrides[VN_IMAGE_NUM_PLANES] = {srcYuv->stride[0] * inByteDepth,
                                                         srcYuv->stride[1] * inByteDepth,
                                                         srcYuv->stride[2] * inByteDepth};
    const uint32_t outByteStrides[VN_IMAGE_NUM_PLANES] = {dstRgb->stride[0] * outByteDepth,
                                                          dstRgb->stride[1] * outByteDepth,
                                                          dstRgb->stride[2] * outByteDepth};
    const int16_t inMax = (int16_t)(1 << inBitDepth) - 1;
    const int16_t outMax = (int16_t)(1 << outBitDepth) - 1;
    const uint8_t subsampleHorizontal = (uint8_t)(1 << chromaHorizontalShift);
    const uint8_t outComponentsInPlane0 = getNumComponentsInPlane0(dstRgb->ilv);

    // Use doubles for rgbs, to preserve precision until converting back to yuv
    I16Vec4 yuva = {0, 0, 0, inMax};
    DVec4 rgbaPreConvert = {0.0, 0.0, 0.0, (double)inMax};
    DVec4 rgbaPostConvert = {0.0, 0.0, 0.0, (double)inMax};
    DVec4* rgbaToOutput = (colorspaceConversion == NULL ? &rgbaPreConvert : &rgbaPostConvert);
    for (uint32_t y = startRow; y < endRow; y++) {
        uint8_t* dstRow = planeBufferRow(dstRgb->plane, outByteStrides, 0, 0, y);
        const uint8_t* srcRowLuma = planeBufferRowConst(srcYuv->plane, inByteStrides, 0, 0, y);
        const uint8_t* srcRowU =
            planeBufferRowConst(srcYuv->plane, inByteStrides, chromaVerticalShift, 1, y);
        const uint8_t* srcRowV =
            planeBufferRowConst(srcYuv->plane, inByteStrides, chromaVerticalShift, 2, y);

        for (uint32_t x = 0; x < srcYuv->stride[0]; x++) {
            const uint32_t subsampleX = x / subsampleHorizontal;
            memcpy(&yuva[0], &srcRowLuma[x * inByteDepth], inByteDepth);
            memcpy(&yuva[1], &srcRowU[subsampleX * inByteDepth], inByteDepth);
            memcpy(&yuva[2], &srcRowV[subsampleX * inByteDepth], inByteDepth);

            // Convert to rgb (the output is already in Full range)
            mat4x4_mul_I16Vec4_to_DVec4(rgbaPreConvert, yuvToRgbMatrix, yuva);

            // colorspace conversion
            if (colorspaceConversion != NULL) {
                mat4x4_mul_DVec4_to_DVec4(rgbaPostConvert, colorspaceConversion, rgbaPreConvert);
            }

            // Clamp and copy into destination arrays.
            for (uint8_t channel = 0; channel < outComponentsInPlane0; channel++) {
                (*rgbaToOutput)[channel] = clamp((*rgbaToOutput)[channel], 0.0, (double)outMax);
                writeU16ToBuffer(dstRow, (uint16_t)(*rgbaToOutput)[0], dstRgb->depth,
                                 outComponentsInPlane0 * x + channel);
            }
        }
    }

    return true;
}
