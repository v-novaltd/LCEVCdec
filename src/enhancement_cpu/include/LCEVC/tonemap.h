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

#ifndef VN_DEC_ENHANCEMENT_CPU_TONEMAP_H_
#define VN_DEC_ENHANCEMENT_CPU_TONEMAP_H_

#include <LCEVC/PerseusDecoder.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * \brief Apply colour conversions and tonemapping. The tonemap and colorspace conversions are
 *        applied in RGB. All conversions steps are optional, but src and dst must have formats
 *        compatible with the parameters provided (for example, if you leave yuvToRgb null, then
 *        src is expected to be rgb already).
 *
 * @param[out]      dst                       Non-null pointer to a destination image.
 * @param[in]       src                       Non-null pointer to a source image. If dst and src
 *                                            are the same format, then src can be the same image
 *                                            as dst, allowing you to avoid allocating an
 *                                            additional image. Must be large enough to hold
 *                                            output.
 * @param[in]       dstChromaHorizontalShift, The logarithm (base 2) of the chroma subsampling, for
 *                  dstChromaVerticalShift,   src and dst pictures, horizontally and vertically.
 *                  srcChromaHorizontalShift,
 *                  srcChromaVerticalShift,
 * @param[in]       startRow                  First row to tonemap (e.g. 0)
 * @param[in]       endRow                    Row after the last row to tonemap (e.g. img->height)
 * @param[in]       inYuvToRgb709             Required if input is YUV. 4x4 Matrix to use to
 *                                            convert YUV input to RGB.
 * @param[in]       rgb2020ToOutYuv           Required if output is YUV. 4x4 Matrix to use to
 *                                            convert RGB to YUV output.
 * @param[in]       BT709toBT2020             Optional. 4x4 Matrix to use to convert BT709 to
 *                                            BT2020. If null, img should be BT2020 already.
 * @param[in]       tonemappingLutArr         Optional. Non-empty lookup-table for SDR->HDR
 *                                            tonemapping. If null, no tonemapping is performed.
 * @param[in]       tonemappingLutArrLen      Must be >0 if tonemappingLutArr is provided. The
 *                                            length of the tonemapping lookup table.
 * @return          		                  True on success, false otherwise. Can only fail due to
 *                                            invalid parameters.
 */
bool LCEVC_tonemap(perseus_image* dst, uint8_t dstChromaHorizontalShift,
                   uint8_t dstChromaVerticalShift, const perseus_image* src,
                   uint8_t srcChromaHorizontalShift, uint8_t srcChromaVerticalShift,
                   uint32_t startRow, uint32_t endRow, const double inYuvToRgb709[16],
                   const double rgb2020ToOutYuv[16], const double BT709toBT2020[16],
                   const float* tonemappingLutArr, size_t tonemappingLutArrLen);

#ifdef __cplusplus
}
#endif

#endif // VN_DEC_ENHANCEMENT_CPU_TONEMAP_H_
