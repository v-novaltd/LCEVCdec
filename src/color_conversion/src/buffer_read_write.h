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

#ifndef VN_LCEVC_COLOR_CONVERSION_BUFFER_READ_WRITE_H
#define VN_LCEVC_COLOR_CONVERSION_BUFFER_READ_WRITE_H

#include <LCEVC/legacy/PerseusDecoder.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

static inline double clamp(double in, double min, double max)
{
    const double out = (in < min) ? min : in;
    return (out > max) ? max : out;
}

static inline void writeU16ToU8(uint8_t* destBuf, uint16_t src, uint32_t loc)
{
    destBuf[loc] = (src > UINT8_MAX ? UINT8_MAX : (uint8_t)src);
}

static inline void writeU16ToU10OrMore(uint8_t* destBuf, uint16_t src, uint32_t loc)
{
    memcpy(&destBuf[2 * loc], &src, sizeof(src));
}

static inline void writeU16ToBuffer(uint8_t* destBuf, uint16_t src, perseus_bitdepth bitdepth, uint32_t loc)
{
    if (perseus_get_bytedepth(bitdepth) == 1) {
        writeU16ToU8(destBuf, src, loc);
    } else {
        writeU16ToU10OrMore(destBuf, src, loc);
    }
}

static inline uint8_t* planeBufferRow(void* planes[VN_IMAGE_NUM_PLANES],
                                      const uint32_t bytesPerRow[VN_IMAGE_NUM_PLANES],
                                      uint16_t verticalShift, uint8_t planeIdx, uint32_t row)
{
    if (planes[planeIdx] == NULL) {
        return NULL;
    }

    return (uint8_t*)planes[planeIdx] + ((row >> verticalShift) * bytesPerRow[planeIdx]);
}

static inline const uint8_t* planeBufferRowConst(void* const planes[VN_IMAGE_NUM_PLANES],
                                                 const uint32_t bytesPerRow[VN_IMAGE_NUM_PLANES],
                                                 uint16_t verticalShift, uint8_t planeIdx, uint32_t row)
{
    if (planes[planeIdx] == NULL) {
        return NULL;
    }

    return (uint8_t*)planes[planeIdx] + ((row >> verticalShift) * bytesPerRow[planeIdx]);
}

static inline uint8_t getNumComponentsInPlane0(perseus_interleaving ilv)
{
    switch (ilv) {
        case PSS_ILV_YUYV:
        case PSS_ILV_UYVY:
        case PSS_ILV_RGB: return 3;
        case PSS_ILV_RGBA: return 4;
        case PSS_ILV_NV12:
        case PSS_ILV_NONE: break;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COLOR_CONVERSION_BUFFER_READ_WRITE_H
