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

#include "blit_common.h"
#include "fp_types.h"

#include <LCEVC/common/limit.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pixel_processing/blit.h>
#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*
 * Add SN.M to UN
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP(loadOp, storeOp) dstValue = storeOp(loadOp(*dstPixel) + srcValue)

static void addU8(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint8_t, fpU8ToS8, fpS8ToU8)
}

static void addU10(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, fpU10ToS10, fpS10ToU10)
}

static void addU12(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, fpU12ToS12, fpS12ToU12)
}

static void addU14(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, fpU14ToS14, fpS14ToU14)
}

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------*
 * Add S16 to S16.
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP() dstValue = saturateS16(*dstPixel + srcValue)

static void addS16(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(int16_t, int16_t) }

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------
 * Copy UN to S16
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP(shift) dstValue = (srcValue << (shift)) - 16384;

static void copyU8ToS16(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint8_t, int16_t, 7) }

static void copyU10ToS16(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, int16_t, 5) }

static void copyU12ToS16(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, int16_t, 3) }

static void copyU14ToS16(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, int16_t, 1) }

static void copyU8ToS16Nv12(const LdppBlitArgs* args)
{
    const LdpPicturePlaneDesc* src = args->src;
    const LdpPicturePlaneDesc* dst = args->dst;
    const uint32_t srcStride = src->rowByteStride / sizeof(uint8_t);
    const uint32_t dstStride = dst->rowByteStride / sizeof(int16_t);
    const uint8_t* srcRow = VN_PLANE_GETLINE(src, args->offset);
    int16_t* dstRow = (int16_t*)VN_PLANE_GETLINE(dst, args->offset);
    int32_t dstValue = 0;
    for (uint32_t y = 0; y < args->count; ++y) {
        const uint8_t* srcPixel = srcRow;
        int16_t* dstPixel = dstRow;
        for (uint32_t x = 0; x < args->minWidth; ++x) {
            int32_t srcValue = (int32_t)*srcPixel;
            srcPixel += 2;
            VN_SURFACE_OP(7);
            *dstPixel++ = (int16_t)dstValue;
        }
        srcRow += srcStride;
        dstRow += dstStride;
    }
}

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------
 * Copy S16 to UN
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP(rounding, shift, signOffset, maxValue)        \
    srcValue = ((srcValue + (rounding)) >> (shift)) + (signOffset); \
    dstValue = (srcValue < 0 ? 0 : srcValue > (maxValue) ? (maxValue) : srcValue)

static void copyS16ToU8(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint8_t, 64, 7, 128, 255)
}

static void copyS16ToU10(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, 16, 5, 512, 1023)
}

static void copyS16ToU12(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, 4, 3, 2048, 4095)
}

static void copyS16ToU14(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(int16_t, uint16_t, 1, 1, 8192, 16383)
}

static void copyS16ToU8Nv12(const LdppBlitArgs* args)
{
    const LdpPicturePlaneDesc* src = args->src;
    const LdpPicturePlaneDesc* dst = args->dst;
    const uint32_t srcStride = src->rowByteStride / sizeof(int16_t);
    const uint32_t dstStride = dst->rowByteStride / sizeof(uint8_t);
    const int16_t* srcRow = (const int16_t*)VN_PLANE_GETLINE(src, args->offset);
    uint8_t* dstRow = VN_PLANE_GETLINE(dst, args->offset);
    int32_t dstValue = 0;
    for (uint32_t y = 0; y < args->count; ++y) {
        const int16_t* srcPixel = srcRow;
        uint8_t* dstPixel = dstRow;
        for (uint32_t x = 0; x < args->minWidth; ++x) {
            int32_t srcValue = (int32_t)*srcPixel++;
            VN_SURFACE_OP(64, 7, 128, 255);
            *dstPixel = (uint8_t)dstValue;
            dstPixel += 2;
        }
        srcRow += srcStride;
        dstRow += dstStride;
    }
}

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------
 * Copy UN to UM (promoting).
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP(shift) dstValue = srcValue << (shift);

static void copyU8ToU10(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint8_t, uint16_t, 2) }

static void copyU8ToU12(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint8_t, uint16_t, 4) }

static void copyU8ToU14(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint8_t, uint16_t, 6) }

static void copyU16ToU16SUp2(const LdppBlitArgs* args)
{
    VN_BLIT_PER_PIXEL_BODY(uint16_t, uint16_t, 2)
}

static void copyU10ToU14(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, uint16_t, 4) }

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------
 * Copy UN to UM (demoting).
 *------------------------------------------------------------------------------*/

#define VN_SURFACE_OP(shift) dstValue = srcValue >> (shift);

static void copyU10ToU8(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, uint8_t, 2) }

static void copyU12ToU8(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, uint8_t, 4) }

static void copyU14ToU8(const LdppBlitArgs* args) { VN_BLIT_PER_PIXEL_BODY(uint16_t, uint8_t, 6) }

#undef VN_SURFACE_OP

/*------------------------------------------------------------------------------
 * Copy identity - these are memory copies from one surface to another, consider at
 * the call-site to use the src surface in this situation where possible.
 *------------------------------------------------------------------------------*/

static void copyIdentity(const LdppBlitArgs* args)
{
    const LdpPicturePlaneDesc* src = args->src;
    const LdpPicturePlaneDesc* dst = args->dst;

    const uint32_t srcByteStride = src->rowByteStride;
    const uint32_t dstByteStride = dst->rowByteStride;

    const uint8_t* srcRow = VN_PLANE_GETLINE(src, args->offset);
    uint8_t* dstRow = VN_PLANE_GETLINE(dst, args->offset);

    if (srcByteStride == dstByteStride) {
        const size_t copySize = (size_t)srcByteStride * args->count;
        memcpy(dstRow, srcRow, copySize);
    } else {
        const size_t copySize = (size_t)minU32(srcByteStride, dstByteStride);
        for (uint32_t y = 0; y < args->count; ++y) {
            memcpy(dstRow, srcRow, copySize);
            srcRow += srcByteStride;
            dstRow += dstByteStride;
        }
    }
}

/*------------------------------------------------------------------------------
 * Tables
 *------------------------------------------------------------------------------*/

/* clang-format off */

static const PlaneBlitFunction kAddTable[LdpFPCount] = {
	&addU8,  /* FP_U8 */
	&addU10, /* FP_U10 */
	&addU12, /* FP_U12 */
	&addU14, /* FP_U14 */
	&addS16, /* FP_S8_7 */
	&addS16, /* FP_S10_5 */
	&addS16, /* FP_S12_3 */
	&addS16, /* FP_S14_1 */
};

static const PlaneBlitFunction kCopyTable[LdpFPCount][LdpFPCount] = {
	/* src/dst   U8            U10            U12                U14                S8.7          S10.5          S12.3          S14.1*/
	/* U8    */ {NULL,         &copyU8ToU10,  &copyU8ToU12,      &copyU8ToU14,      &copyU8ToS16, &copyU8ToS16,  &copyU8ToS16,  &copyU8ToS16},
	/* U10   */ {&copyU10ToU8, NULL,          &copyU16ToU16SUp2, &copyU10ToU14,     NULL,         &copyU10ToS16, &copyU10ToS16, &copyU10ToS16},
	/* U12   */ {&copyU12ToU8, NULL,          NULL,              &copyU16ToU16SUp2, NULL,         NULL,          &copyU12ToS16, &copyU12ToS16},
	/* U14   */ {&copyU14ToU8, NULL,          NULL,              NULL,              NULL,         NULL,          NULL,          &copyU14ToS16},
	/* S8.7  */ {&copyS16ToU8, &copyS16ToU10, &copyS16ToU12,     &copyS16ToU14,     NULL,         NULL,          NULL,          NULL},
	/* S10.5 */ {NULL,         &copyS16ToU10, &copyS16ToU12,     &copyS16ToU14,     NULL,         NULL,          NULL,          NULL},
	/* S12.3 */ {NULL,         NULL,          &copyS16ToU12,     &copyS16ToU14,     NULL,         NULL,          NULL,          NULL},
	/* S14.1 */ {NULL,         NULL,          NULL,              &copyS16ToU14,     NULL,         NULL,          NULL,          NULL}
};

/* clang-format on */

/*------------------------------------------------------------------------------*/

PlaneBlitFunction planeBlitGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                             LdppBlendingMode blending, uint32_t planeIndex, bool isNV12)
{
    if (blending == BMAdd) {
        /* Additive blending is expecting srcFP to be residuals int16_t */
        if (srcFP != fixedPointHighPrecision(dstFP)) {
            return NULL;
        }

        return kAddTable[dstFP];
    }

    if (blending == BMCopy) {
        if ((srcFP == dstFP) || (fixedPointIsSigned(srcFP) && fixedPointIsSigned(dstFP))) {
            return &copyIdentity;
        }
        if (isNV12 && planeIndex > 0) {
            if (srcFP == LdpFPU8 && dstFP == LdpFPS8) {
                return &copyU8ToS16Nv12;
            } else if (srcFP == LdpFPS8 && dstFP == LdpFPU8) {
                return &copyS16ToU8Nv12;
            }
            return NULL;
        }

        return kCopyTable[srcFP][dstFP];
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/
