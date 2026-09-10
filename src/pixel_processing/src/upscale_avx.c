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

#if VN_SDK_FEATURE(SIMD) && (VN_ARCH(X86) || VN_ARCH(X64))

#include <assert.h>
#include <immintrin.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include "fp_types.h"
#include "upscale_macros/common_avx.h"

// === 1D S16 to S16 ===

void avxPlanar1DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

// === 1D U8 to U8 ===

void avxPlanar1DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

// === 1D NV12 ===

void avxPlanar1DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/upscale_1d_nv12_avx.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/upscale_1d_nv12_avx.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 1D U16 to U16 ===

void avxPlanar1DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar1DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/upscale_1d_avx.h"
#undef VN_UPSCALE_LOOP
}

// === 2D S16 to S16 ===

void avxPlanar2DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

// === 2D U8 to U8 ===

void avxPlanar2DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

// === 2D NV12 ===

void avxPlanar2DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/upscale_2d_nv12_avx.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/upscale_2d_nv12_avx.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 2D U16 to U16 ===

void avxPlanar2DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

void avxPlanar2DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/upscale_2d_avx.h"
#undef VN_UPSCALE_LOOP
}

/*------------------------------------------------------------------------------*/

static const UpscaleFunction kPlanar1DS16ToS16FunctionTable[2][2] = {
    /* PA off */ {avxPlanar1DS16ToS16PAOffDitherOff, avxPlanar1DS16ToS16PAOffDitherOn},
    /* PA on  */ {avxPlanar1DS16ToS16PAOnDitherOff, avxPlanar1DS16ToS16PAOnDitherOn},
};

static const UpscaleFunction kPlanar1DU8ToU8FunctionTable[2][2] = {
    /* PA off */ {avxPlanar1DU8ToU8PAOffDitherOff, avxPlanar1DU8ToU8PAOffDitherOn},
    /* PA on  */ {avxPlanar1DU8ToU8PAOnDitherOff, avxPlanar1DU8ToU8PAOnDitherOn},
};

static const UpscaleFunction kPlanar1DU16ToU16FunctionTable[2][2] = {
    /* PA off */ {avxPlanar1DU16ToU16PAOffDitherOff, avxPlanar1DU16ToU16PAOffDitherOn},
    /* PA on  */ {avxPlanar1DU16ToU16PAOnDitherOff, avxPlanar1DU16ToU16PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DS16ToS16FunctionTable[2][2] = {
    /* PA off */ {avxPlanar2DS16ToS16PAOffDitherOff, avxPlanar2DS16ToS16PAOffDitherOn},
    /* PA on  */ {avxPlanar2DS16ToS16PAOnDitherOff, avxPlanar2DS16ToS16PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DU8ToU8FunctionTable[2][2] = {
    /* PA off */ {avxPlanar2DU8ToU8PAOffDitherOff, avxPlanar2DU8ToU8PAOffDitherOn},
    /* PA on  */ {avxPlanar2DU8ToU8PAOnDitherOff, avxPlanar2DU8ToU8PAOnDitherOn},
};

static const UpscaleFunction kPlanar2DU16ToU16FunctionTable[2][2] = {
    /* PA off */ {avxPlanar2DU16ToU16PAOffDitherOff, avxPlanar2DU16ToU16PAOffDitherOn},
    /* PA on  */ {avxPlanar2DU16ToU16PAOnDitherOff, avxPlanar2DU16ToU16PAOnDitherOn},
};

UpscaleFunction upscaleGetFunctionAVX(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
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
                return (UpscaleFunction)avxPlanar2DNV12PAOnDitherOff;
            }
            return (UpscaleFunction)avxPlanar2DNV12PAOffDitherOff;
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
                return (UpscaleFunction)avxPlanar1DNV12PAOnDitherOff;
            }
            return (UpscaleFunction)avxPlanar1DNV12PAOffDitherOff;
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

UpscaleFunction upscaleGetFunctionAVX(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
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
