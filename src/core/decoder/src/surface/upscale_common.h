/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_UPSCALE_COMMON_H_
#define VN_DEC_CORE_UPSCALE_COMMON_H_

#include "common/types.h"

/*------------------------------------------------------------------------------*/

typedef struct Kernel Kernel_t;

typedef void (*UpscaleHorizontal_t)(Context_t* ctx, const uint8_t* in[2], uint8_t* out[2],
                                    const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                    uint32_t xEnd, const Kernel_t* kernel, bool dither);

typedef void (*UpscaleVertical_t)(Context_t* ctx, const uint8_t* in, uint32_t inStride,
                                  uint8_t* out, uint32_t outStride, uint32_t y, uint32_t rows,
                                  uint32_t height, const Kernel_t* kernel);

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
} UpscaleHorizontalCoords_t;

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
                                uint32_t middleAlignment, UpscaleHorizontalCoords_t* coords);

/*! \brief Determines if the left slice region is valid.
 *  \param coords The input coordinates to check.
 *  \return true if the left slice is valid otherwise false */
bool upscaleHorizontalCoordsIsLeftValid(const UpscaleHorizontalCoords_t* coords);

/*! \brief Determines if the right  slice region is valid.
 *  \param coords The input coordinates to check.
 *  \return true if the right slice is valid otherwise false */
bool upscaleHorizontalCoordsIsRightValid(const UpscaleHorizontalCoords_t* coords);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_UPSCALE_COMMON_H_ */