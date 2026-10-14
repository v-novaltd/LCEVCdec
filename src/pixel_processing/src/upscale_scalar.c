/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include "fp_types.h"
#include "upscale_common.h"

#include <LCEVC/common/limit.h>
#include <LCEVC/pixel_processing/dither.h>
//

/*------------------------------------------------------------------------------*/

static inline int16_t scalarU8ToS16(uint8_t value)
{
    return (int16_t)(((int32_t)value << 7) - 0x4000);
}

static inline int16_t scalarU16ToS16(int16_t value, int16_t shift)
{
    return (int16_t)(((int32_t)value << shift) - 0x4000);
}

static inline int16_t scalarShiftRoundS15(int64_t value)
{
    const int64_t rounded = (value + 0x2000) >> 14;
    return (int16_t)((rounded < -0x4000) ? -0x4000 : ((rounded > 0x3fff) ? 0x3fff : rounded));
}

static inline uint8_t scalarS16ToU8(int16_t value)
{
    return saturateU8((((int32_t)value + 64) >> 7) + 128);
}

static inline int16_t scalarS16ToU16(int16_t value, int16_t offset, int16_t shift, int16_t midpoint)
{
    const int32_t converted = (((int32_t)value + offset) >> shift) + midpoint;
    return (int16_t)clampInt(converted, 0, 2 * midpoint - 1);
}

static inline void scalarNV12LoadDeinterleaved8(const uint8_t* srcRow, int32_t srcWidth,
                                                int32_t blockSrcX, int16_t u[8], int16_t v[8])
{
    const int32_t pairCount = srcWidth >> 1;
    const int32_t pairStart = (blockSrcX >> 1) - 2;

    for (int32_t i = 0; i < 8; ++i) {
        const int32_t pairX = clampInt(pairStart + i, 0, pairCount - 1);
        const int32_t srcX = pairX << 1;
        u[i] = scalarU8ToS16(srcRow[srcX + 0]);
        v[i] = scalarU8ToS16(srcRow[srcX + 1]);
    }
}

static inline void scalarConvolveVerticalPair8(const int16_t src0[8], const int16_t src1[8],
                                               const int16_t src2[8], const int16_t src3[8],
                                               const int16_t src4[8], const int32_t kernel[4],
                                               int16_t v0[8], int16_t v1[8])
{
    for (int32_t i = 0; i < 8; ++i) {
        const int64_t acc0 = ((int64_t)src0[i] * kernel[3]) + ((int64_t)src1[i] * kernel[2]) +
                             ((int64_t)src2[i] * kernel[1]) + (src3[i] * kernel[0]);
        const int64_t acc1 = ((int64_t)src1[i] * kernel[0]) + ((int64_t)src2[i] * kernel[1]) +
                             ((int64_t)src3[i] * kernel[2]) + (src4[i] * kernel[3]);

        v0[i] = scalarShiftRoundS15(acc0);
        v1[i] = scalarShiftRoundS15(acc1);
    }
}

static inline void scalarConvolveHorizontal8(const int16_t srcPel[8], const int32_t kernel[4],
                                             int16_t row[8])
{
    for (int32_t i = 0; i < 4; ++i) {
        const int64_t rowOdd = ((int64_t)srcPel[i + 0] * kernel[3]) +
                               ((int64_t)srcPel[i + 1] * kernel[2]) +
                               ((int64_t)srcPel[i + 2] * kernel[1]) + (srcPel[i + 3] * kernel[0]);
        const int64_t rowEven = ((int64_t)srcPel[i + 1] * kernel[0]) +
                                ((int64_t)srcPel[i + 2] * kernel[1]) +
                                ((int64_t)srcPel[i + 3] * kernel[2]) + (srcPel[i + 4] * kernel[3]);

        row[(i << 1) + 0] = scalarShiftRoundS15(rowOdd);
        row[(i << 1) + 1] = scalarShiftRoundS15(rowEven);
    }
}

/*------------------------------------------------------------------------------*/

// === 1D S16 to S16 ===

static void planar1DS16ToS16(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_S16, FP_S16)
#include "upscale_macros/scalar_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D U8 to U8 ===

static void planar1DU8ToU8(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_U8, FP_U8)
#include "upscale_macros/scalar_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D U16 to U16 ===

static void planar1DU16ToU16(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_U16, FP_U16)
#include "upscale_macros/scalar_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D NV12 ===

static void planar1DNV12ToNV12(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_NV12, FP_NV12)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/scalar_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

/*------------------------------------------------------------------------------*/

// === 2D S16 to S16 ===

static void planar2DS16ToS16(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_S16, FP_S16)
#include "upscale_macros/scalar_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D U8 to U8 ===

static void planar2DU8ToU8(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_U8, FP_U8)
#include "upscale_macros/scalar_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D U16 to U16 ===

static void planar2DU16ToU16(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_U16, FP_U16)
#include "upscale_macros/scalar_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D NV12 ===

static void planar2DNV12ToNV12(VN_UPSCALE_ARGS){
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG_SCALAR(FP_NV12, FP_NV12)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/scalar_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

/*------------------------------------------------------------------------------*/

UpscaleFunction upscaleGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                         Interleaving interleaving, bool is2d, bool pa, bool dithering)
{
    (void)pa;
    (void)dithering;

    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        return NULL;
    }

    if (srcFP != dstFP) {
        return NULL;
    }

    if (interleaving == ILNV12 && dstFP == LdpFPU8) {
        return is2d ? planar2DNV12ToNV12 : planar1DNV12ToNV12;
    }

    if (fixedPointIsSigned(srcFP)) {
        return is2d ? planar2DS16ToS16 : planar1DS16ToS16;
    }

    if (srcFP == LdpFPU8) {
        return is2d ? planar2DU8ToU8 : planar1DU8ToU8;
    }

    if (srcFP == LdpFPU10 || srcFP == LdpFPU12 || srcFP == LdpFPU14) {
        return is2d ? planar2DU16ToU16 : planar1DU16ToU16;
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/
