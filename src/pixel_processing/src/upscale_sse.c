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

#if VN_SDK_FEATURE(SSE)

#include "fp_types.h"

#include <assert.h>
#include <LCEVC/common/sse.h>
#include <LCEVC/pixel_processing/dither.h>

/*------------------------------------------------------------------------------*/

// Input range conversion to internal S16 range
#define U8ToS16(v) _mm_sub_epi16(_mm_slli_epi16(_mm_cvtepu8_epi16(v), 7), kMidpointS16)
#define U16ToS16(v) _mm_sub_epi16(_mm_sll_epi16((v), kLeftShift), kMidpointS16)

// Output range conversion from internal S16 range
#define S16ToU8(value, kOffset, kMidpoint) \
    _mm_add_epi16(_mm_srai_epi16(_mm_add_epi16((value), (kOffset)), 7), (kMidpoint))
#define S16ToU16(value, kOffset, kRightShift, kMidpoint) \
    _mm_add_epi16(_mm_sra_epi16(_mm_add_epi16((value), (kOffset)), (kRightShift)), (kMidpoint))

static inline void sseApplyPA(__m128i* row0, __m128i* row1, const __m128i base)
{
    const __m128i kOnes = _mm_set1_epi16(1);
    const __m128i sum0 = _mm_madd_epi16(*row0, kOnes);
    const __m128i sum1 = _mm_madd_epi16(*row1, kOnes);
    const __m128i mean = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum0, sum1), _mm_set1_epi32(2)), 2);
    const __m128i adjust = _mm_sub_epi32(_mm_cvtepi16_epi32(base), mean);
    const __m128i adjustLo = _mm_unpacklo_epi32(adjust, adjust);
    const __m128i adjustHi = _mm_unpackhi_epi32(adjust, adjust);

    const __m128i row0Lo = _mm_add_epi32(_mm_cvtepi16_epi32(*row0), adjustLo);
    const __m128i row0Hi = _mm_add_epi32(_mm_cvtepi16_epi32(_mm_srli_si128(*row0, 8)), adjustHi);
    const __m128i row1Lo = _mm_add_epi32(_mm_cvtepi16_epi32(*row1), adjustLo);
    const __m128i row1Hi = _mm_add_epi32(_mm_cvtepi16_epi32(_mm_srli_si128(*row1, 8)), adjustHi);

    *row0 = _mm_packs_epi32(row0Lo, row0Hi);
    *row1 = _mm_packs_epi32(row1Lo, row1Hi);
}

static inline __m128i sseApplyDither(__m128i values, const uint16_t** ditherBuffer,
                                     const __m128i offset, const __m128i shift, const __m128i mul)
{
    __m128i ditherValues = _mm_loadu_si128((const __m128i*)*ditherBuffer);
    *ditherBuffer += 8;

    ditherValues = _mm_mulhi_epu16(ditherValues, mul);
    ditherValues = _mm_sub_epi16(offset, ditherValues);

    return _mm_adds_epi16(values, _mm_sll_epi16(ditherValues, shift));
}

/*------------------------------------------------------------------------------*/

// === 1D S16 to S16 ===

void planar1DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D U8 to U8 ===

void planar1DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 1D NV12 ===

void planar1DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/sse_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void planar1DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_1D
#include "upscale_macros/sse_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 1D U16 to U16 ===

void planar1DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar1DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/sse_1d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D S16 to S16 ===

void planar2DS16ToS16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 1, 1)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DS16ToS16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_S16, FP_S16, 0, 1)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D U8 to U8 ===

void planar2DU8ToU8PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 1, 1)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU8ToU8PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U8, FP_U8, 0, 1)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

// === 2D NV12 ===

void planar2DNV12PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 0, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/sse_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

void planar2DNV12PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_NV12, FP_NV12, 1, 0)
#define DIMENSION DIMENSION_2D
#include "upscale_macros/sse_nv12_loop.h"
#undef DIMENSION
#undef VN_UPSCALE_LOOP
}

// === 2D U16 to U16 ===

void planar2DU16ToU16PAOnDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOnDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 1, 1)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOffDitherOff(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 0)
#include "upscale_macros/sse_2d_loop.h"
#undef VN_UPSCALE_LOOP
}

void planar2DU16ToU16PAOffDitherOn(VN_UPSCALE_ARGS)
{
#define VN_UPSCALE_LOOP VN_UPSCALE_LOOP_CONFIG(FP_U16, FP_U16, 0, 1)
#include "upscale_macros/sse_2d_loop.h"
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

UpscaleFunction upscaleGetFunctionSSE(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
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

UpscaleFunction upscaleGetFunctionSSE(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
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
