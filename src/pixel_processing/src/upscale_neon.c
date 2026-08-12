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

#include <LCEVC/build_config.h>
#include <LCEVC/common/platform.h>
//
#include "upscale_common.h"

#include <stddef.h>

#if VN_SDK_FEATURE(NEON)

#include "fp_types.h"

#include <arm_neon.h>
#include <assert.h>
#include <LCEVC/pixel_processing/dither.h>

/*------------------------------------------------------------------------------*/

// int16x8_t & uint8x8_t (input ranges) conversion to int16x8_t (S16 range)
#define U8ToS16(v) \
    vreinterpretq_s16_u16(vsubq_u16(vshll_n_u8(v, 7), vreinterpretq_u16_s16(kMidpointS16)))
#define U16ToS16(v) vsubq_s16(vshlq_s16(v, kLeftShift), kMidpointS16)

// int16x4_t & uint8x8_t (input ranges) conversion to int16x4_t (S16 range), required for some edges
#define U8ToS16x4(v)                                              \
    vreinterpret_s16_u16(vsub_u16(vget_low_u16(vshll_n_u8(v, 7)), \
                                  vget_low_u16(vreinterpretq_u16_s16(kMidpointS16))))
#define U16ToS16x4(v) vsub_s16(vshl_s16(v, vget_low_s16(kLeftShift)), vget_low_s16(kMidpointS16))

// int16x8_t (S16 range) conversion to int16x8_t & uint8x8_t (output ranges)
#define S16ToU8(v)                          \
    vreinterpret_u8_s8(vmovn_s16(vminq_s16( \
        vqaddq_s16(vmaxq_s16(vshrq_n_s16(vqaddq_s16(v, kOffset), 7), kS8Min), kMidpoint), kU8Max)))
#define S16ToU16(v) vaddq_s16(vshlq_s16(vaddq_s16(v, kOffset), kRightShift), kMidpoint)

static inline void neonApplyPA(int16x8_t* row0, int16x8_t* row1, const int16x4_t base)
{
    const int32x4_t sum0 = vpaddlq_s16(*row0);
    const int32x4_t sum1 = vpaddlq_s16(*row1);
    const int32x4_t mean = vrshrq_n_s32(vaddq_s32(sum0, sum1), 2);
    const int32x4_t adjust = vsubq_s32(vmovl_s16(base), mean);
    const int32x4x2_t broadcast = vzipq_s32(adjust, adjust);

    const int32x4_t row0Lo = vaddq_s32(vmovl_s16(vget_low_s16(*row0)), broadcast.val[0]);
    const int32x4_t row0Hi = vaddq_s32(vmovl_s16(vget_high_s16(*row0)), broadcast.val[1]);
    const int32x4_t row1Lo = vaddq_s32(vmovl_s16(vget_low_s16(*row1)), broadcast.val[0]);
    const int32x4_t row1Hi = vaddq_s32(vmovl_s16(vget_high_s16(*row1)), broadcast.val[1]);

    *row0 = vcombine_s16(vqmovn_s32(row0Lo), vqmovn_s32(row0Hi));
    *row1 = vcombine_s16(vqmovn_s32(row1Lo), vqmovn_s32(row1Hi));
}

#if defined(__aarch64__)
#define VTBL1Q_U8_COMPAT(src, idx) vqtbl1q_u8((src), (idx))
#else
static inline uint8x16_t VTBL1Q_U8_COMPAT_IMPL(const uint8x16_t src, const uint8x16_t idx)
{
    const uint8x8x2_t table = {{vget_low_u8(src), vget_high_u8(src)}};
    const uint8x8_t lo = vtbl2_u8(table, vget_low_u8(idx));
    const uint8x8_t hi = vtbl2_u8(table, vget_high_u8(idx));
    return vcombine_u8(lo, hi);
}
#define VTBL1Q_U8_COMPAT(src, idx) VTBL1Q_U8_COMPAT_IMPL((src), (idx))
#endif

/*------------------------------------------------------------------------------*/

// === 1D S16 to S16 ===

void planar1DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D U8 to U8 ===

void planar1DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D NV12 ===

void planar1DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/neon_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void planar1DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/neon_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 1D U16 to U16 ===

void planar1DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/neon_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D S16 to S16 ===

void planar2DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D U8 to U8 ===

void planar2DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/neon_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void planar2DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/neon_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 2D U16 to U16 ===

void planar2DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/neon_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

/*------------------------------------------------------------------------------*/

static const UpscaleFunction kPlanar1DS16ToS16FunctionTable[2][2] = {
    /* PA off */ {planar1DS16ToS16PAOffDitherOff, planar1DS16ToS16PAOffDitherOn},
    /* PA on  */ {planar1DS16ToS16PAOnDitherOff, planar1DS16ToS16PAOnDitherOn},
};

static const UpscaleFunction kPlanar1DU8ToU8FunctionTable[2][2] = {
    /* PA off */ {planar1DU8ToU8PAOffDitherOff, planar1DU8ToU8PAOffDitherOn},
    /* PA on  */ {planar1DU8ToU8PAOnDitherOff, planar1DU8ToU8PAOnDitherOn},
};

static const UpscaleFunction kPlanar1DU16ToU16FunctionTable[2][2] = {
    /* PA off */ {planar1DU16ToU16PAOffDitherOff, planar1DU16ToU16PAOffDitherOn},
    /* PA on  */ {planar1DU16ToU16PAOnDitherOff, planar1DU16ToU16PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DS16ToS16FunctionTable[2][2] = {
    /* PA off */ {planar2DS16ToS16PAOffDitherOff, planar2DS16ToS16PAOffDitherOn},
    /* PA on  */ {planar2DS16ToS16PAOnDitherOff, planar2DS16ToS16PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DU8ToU8FunctionTable[2][2] = {
    /* PA off */ {planar2DU8ToU8PAOffDitherOff, planar2DU8ToU8PAOffDitherOn},
    /* PA on  */ {planar2DU8ToU8PAOnDitherOff, planar2DU8ToU8PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DU16ToU16FunctionTable[2][2] = {
    /* PA off */ {planar2DU16ToU16PAOffDitherOff, planar2DU16ToU16PAOffDitherOn},
    /* PA on  */ {planar2DU16ToU16PAOnDitherOff, planar2DU16ToU16PAOnDitherOn},
};

UpscaleFunction upscaleGetFunctionNEON(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                       Interleaving interleaving, bool is2d, bool pa, bool dithering)
{
    if (!fixedPointIsValid(srcFP) || !fixedPointIsValid(dstFP)) {
        return NULL;
    }

    if (srcFP != dstFP) {
        return NULL;
    }

    const uint8_t paMode = pa ? 1 : 0;
    const uint8_t ditherMode = dithering ? 1 : 0;

    if (is2d) {
        if (interleaving == ILNV12 && dstFP == LdpFPU8) {
            if (pa) {
                return (UpscaleFunction)planar2DNV12PAOnDitherOff;
            }
            return (UpscaleFunction)planar2DNV12PAOffDitherOff;
        }
        if (fixedPointIsSigned(srcFP)) {
            return kPlanar2DS16ToS16FunctionTable[paMode][ditherMode];
        }
        if (srcFP == LdpFPU8) {
            return kPlanar2DU8ToU8FunctionTable[paMode][ditherMode];
        }
        if (srcFP == LdpFPU10 || srcFP == LdpFPU12 || srcFP == LdpFPU14) {
            return kPlanar2DU16ToU16FunctionTable[paMode][ditherMode];
        }
    } else { // 1D
        if (interleaving == ILNV12 && dstFP == LdpFPU8) {
            if (pa) {
                return (UpscaleFunction)planar1DNV12PAOnDitherOff;
            }
            return (UpscaleFunction)planar1DNV12PAOffDitherOff;
        }
        if (fixedPointIsSigned(srcFP)) {
            return kPlanar1DS16ToS16FunctionTable[paMode][ditherMode];
        }
        if (srcFP == LdpFPU8) {
            return kPlanar1DU8ToU8FunctionTable[paMode][ditherMode];
        }
        if (srcFP == LdpFPU10 || srcFP == LdpFPU12 || srcFP == LdpFPU14) {
            return kPlanar1DU16ToU16FunctionTable[paMode][ditherMode];
        }
    }

    return NULL;
}

#else

UpscaleFunction upscaleGetFunctionNEON(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                       Interleaving interleaving, bool is2d, bool pa, bool dithering)
{
    VNUnused(srcFP);
    VNUnused(dstFP);
    VNUnused(interleaving);
    VNUnused(is2d);
    VNUnused(pa);
    VNUnused(dithering);

    return NULL;
}

#endif
