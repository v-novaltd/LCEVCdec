/* Copyright (c) V-Nova International Limited 2022-2026. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_SHARPEN_COMMON_H
#define VN_LCEVC_PIXEL_PROCESSING_SHARPEN_COMMON_H

#include <assert.h>
#include <LCEVC/pipeline/types.h>

typedef struct LdppDitherSlice LdppDitherSlice;

typedef void (*SharpenFunction)(uint8_t* src, uint32_t stride, uint32_t width, uint32_t height,
                                float strength, LdppDitherSlice* dither);

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
SharpenFunction sharpenGetFunctionScalar(LdpFixedPoint fixedPoint);
#endif
SharpenFunction sharpenGetFunctionSSE(LdpFixedPoint fixedPoint);
SharpenFunction sharpenGetFunctionNEON(LdpFixedPoint fixedPoint);

static inline uint8_t* getRowU8(uint8_t* firstSample, size_t rowStride, int32_t y)
{
    return (uint8_t*)(firstSample + (rowStride * y));
}

static inline uint16_t f32ToU16(float val)
{
    assert(val >= 0.0f && val <= 1.0f);
    return (uint16_t)(val * ((1 << 16) - 1));
}

#endif // VN_LCEVC_PIXEL_PROCESSING_SHARPEN_COMMON_H
