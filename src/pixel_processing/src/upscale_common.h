/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H
#define VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H

#include <LCEVC/pipeline/types.h>

/*------------------------------------------------------------------------------*/

typedef struct LdppDitherSlice LdppDitherSlice;
typedef struct LdeKernel LdeKernel;

typedef void (*UpscaleHorizontalFunction)(LdppDitherSlice* dither, const uint8_t* in[2],
                                          uint8_t* out[2], const uint8_t* base[2], uint32_t width,
                                          uint32_t xStart, uint32_t xEnd, const LdeKernel* kernel,
                                          const LdpFixedPoint dstFP);

typedef void (*UpscaleVerticalFunction)(const uint8_t* in, uint32_t inStride, uint8_t* out,
                                        uint32_t outStride, uint32_t y, uint32_t rows,
                                        uint32_t height, const LdeKernel* kernel);

typedef enum Interleaving
{
    ILNone, /**< Surface is planar */
    ILYUYV, /**< Surface is YUV422 of YUYV */
    ILNV12, /**< Surface is YUV420 of UV */
    ILUYVY, /**< Surface is YUV422 of UYVY */
    ILRGB,  /**< Surface is interleaved RGB channels */
    ILRGBA, /**< Surface is interleaved RGBA channels */
    ILCount
} Interleaving;

/*------------------------------------------------------------------------------*/

/*! \brief This structure contains the horizontal coordinates for slicing an upscaling
 *         operation. This is necessary for SIMD processing where edge-case handling
 *         can be difficult with respects to loading of data. The slices are left
 *         edge, right edge and middle. Where the middle width is aligned to a
 *         desired alignment, and the left and right edges are scaled accordingly. */
typedef struct UpscaleHorizontalCoords
{
    uint32_t leftStart;
    uint32_t leftEnd;
    uint32_t rightStart;
    uint32_t rightEnd;
    uint32_t start;
    uint32_t end;
} UpscaleHorizontalCoords;

/*------------------------------------------------------------------------------*/

/*! \brief Helper function that calculates the left, middle and right processing
 *         slices for performing horizontal upscaling in SIMD.
 *  \param width            The overall surface width being upscaled from.
 *  \param xStart           The start x-coord.
 *  \param xEnd             The end x-coord.
 *  \param kernelSize       The number of taps for the upscaling kernel.
 *  \param middleAlignment  The required width alignment for the middle slice.
 *  \param coords           The output coordinates structure to populate. */
void upscaleHorizontalGetCoords(uint32_t width, uint32_t xStart, uint32_t xEnd, uint32_t kernelSize,
                                uint32_t middleAlignment, UpscaleHorizontalCoords* coords);

/*! \brief Determines if the left slice region is valid.
 *  \param coords The input coordinates to check.
 *  \return true if the left slice is valid otherwise false */
bool upscaleHorizontalCoordsIsLeftValid(const UpscaleHorizontalCoords* coords);

/*! \brief Determines if the right  slice region is valid.
 *  \param coords The input coordinates to check.
 *  \return true if the right slice is valid otherwise false */
bool upscaleHorizontalCoordsIsRightValid(const UpscaleHorizontalCoords* coords);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H
