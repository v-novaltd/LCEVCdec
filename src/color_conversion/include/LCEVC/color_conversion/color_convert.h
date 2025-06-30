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

#ifndef VN_LCEVC_COLOR_CONVERSION_COLOR_CONVERT_H
#define VN_LCEVC_COLOR_CONVERSION_COLOR_CONVERT_H

#include <LCEVC/legacy/PerseusDecoder.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * \brief Convert rgb to yuv, with optional colorspace conversion
 *
 * @param[out]      dstYuv                  Non-null pointer to yuv output. Must be distinct from
 *                                          src and already have memory allocated.
 * @param[in]       srcRgb                  Non-null pointer to rgb input.
 * @param[in]       chromaHorizontalShift   Log (base 2) of the horizontal subsampling of the output
 *                                          image's chroma plane.
 * @param[in]       chromaVerticalShift     Log (base 2) of the vertical subsampling of the output
 *                                          image's chroma plane.
 * @param[in]       startRow                First row to convert (e.g. 0)
 * @param[in]       endRow                  Row after the last row to convert (e.g. src's height)
 * @param[in]       rgbToYuvMatrix          4x4 Matrix to use to convert RGB to YUV
 * @param[in]       colorspaceConversion    Optional. 4x4 Matrix to use to convert colorspaces
 *                                          BEFORE converting to YUV. For example, BT709->BT2020.
 * @return          		                True on success, false otherwise. Can only fail due to
 *                                          invalid parameters.
 */
bool LCEVC_rgbToYuv(perseus_image* dst, const perseus_image* src, uint16_t startRow, uint16_t endRow,
                    const double rgbToYuvMatrix[16], const double colorspaceConversion[16]);

/*!
 * \brief Convert yuv to rgb, with optional colorspace conversion
 *
 * @param[out]      dstRgb                  Non-null pointer to rgb output. Must be distinct from
 *                                          src and already have memory allocated.
 * @param[in]       srcYuv                  Non-null pointer to yuv input.
 * @param[in]       chromaHorizontalShift   Log (base 2) of the horizontal subsampling of the input
 *                                          image's chroma plane.
 * @param[in]       chromaVerticalShift     Log (base 2) of the vertical subsampling of the input
 *                                          image's chroma plane.
 * @param[in]       startRow                First row to convert (e.g. 0)
 * @param[in]       endRow                  Row after the last row to convert (e.g. src's height)
 * @param[in]       yuvToRgbMatrix          Matrix to use to convert YUV to RGB
 * @param[in]       colorspaceConversion    Optional. Matrix to use to convert colorspaces AFTER
 *                                          converting to RGB. For example, BT709->BT2020.
 * @return          		                True on success, false otherwise. Can only fail due to
 *                                          invalid parameters.
 */
bool LCEVC_yuvToRgb(perseus_image* dstRgb, const perseus_image* srcYuv, uint16_t startRow, uint16_t endRow,
                    const double yuvToRgbMatrix[16], const double colorspaceConversion[16]);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COLOR_CONVERSION_COLOR_CONVERT_H
