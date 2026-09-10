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

#ifndef VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H
#define VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H

#include <LCEVC/pipeline/types.h>

/*------------------------------------------------------------------------------*/

typedef struct LdppDitherSlice LdppDitherSlice;
typedef struct LdeKernel LdeKernel;

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

typedef struct LdppUpscaleParams
{
    uint16_t kernel[4]; /** Signed S16 kernel bit patterns for SIMD, or magnitudes for wide scalar kernels **/
    bool kernelIsWide; /** Kernel contains magnitudes which cannot be represented as S16 **/
    bool applyPA;      /** Runtime PA flag for scalar upscale paths **/
    uint16_t shift;    /** Shift when converting from baseFP to S16 format and back **/
    uint16_t offset;   /** Offset when converting from S16 to baseFP format **/
    uint16_t midpoint; /** Midpoint when converting from S16 to baseFP format **/
} LdppUpscaleParams;

#define VN_UPSCALE_ARGS                                                                    \
    uint8_t *src, uint8_t *dst, uint32_t srcStride, uint32_t dstStride, uint32_t srcWidth, \
        uint32_t srcHeight, uint32_t yStart, uint32_t yEnd, LdppUpscaleParams params,      \
        LdppDitherSlice *dither

typedef void (*UpscaleFunction)(VN_UPSCALE_ARGS);

UpscaleFunction getUpscaleFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP, Interleaving interleaving,
                                   bool is2d, bool pa, bool ditheringEnabled, const uint16_t kernel[4]);

UpscaleFunction upscaleGetFunctionAVX(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                      Interleaving interleaving, bool is2d, bool pa, bool dithering);

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
UpscaleFunction upscaleGetFunctionSSE(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                      Interleaving interleaving, bool is2d, bool pa, bool dithering);
#endif

UpscaleFunction upscaleGetFunctionNEON(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                       Interleaving interleaving, bool is2d, bool pa, bool dithering);

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
UpscaleFunction upscaleGetFunctionScalar(LdpFixedPoint srcFP, LdpFixedPoint dstFP,
                                         Interleaving interleaving, bool is2d, bool pa, bool dithering);
#endif

/*! \brief Kernel coefficients are stored in the bitstream and the LdeKernel as uint16_t;
 *         however, the MPEG-5 Part 2 LCEVC standard specifies that the 0th and 3rd elements
 *         are negated when read. This helper should be used at read time to enforce this.
 */
static inline int32_t upscaleKernelGetCoefficient(const uint16_t kernel[4], uint32_t index)
{
    const int32_t magnitude = kernel[index];
    return (index == 0 || index == 3) ? -magnitude : magnitude;
}

static inline int32_t upscaleKernelGetPreparedCoefficient(const LdppUpscaleParams* params, uint32_t index)
{
    return params->kernelIsWide ? upscaleKernelGetCoefficient(params->kernel, index)
                                : (int16_t)params->kernel[index];
}

/*! \brief Most kernels (including all defaults from the encoder) fit into S16 types, this makes
 *         writing performant upscalers easier. For conformance, kernels can be larger than this,
 *         which is detected by this helper; slower but fully conformant scalar paths are then used.
 */
static inline bool upscaleKernelFitsS16(const uint16_t kernel[4])
{
    return kernel[0] <= INT16_MAX && kernel[1] <= INT16_MAX && kernel[2] <= INT16_MAX &&
           kernel[3] <= INT16_MAX;
}

/*------------------------------------------------------------------------------*/

// Dimension definitions for NV12
#define DIM_1D 1
#define DIM_2D 2

// Fixed point definitions
#define FP_U8 1
#define FP_NV12 2
#define FP_U16 3
#define FP_S16 4

/*! \brief define VN_UPSCALE_LOOP_CONFIG macro for use in parameterising upscale functions at compile time.
 *
 *  \param IN_FIXED_POINT Input fixed point format
 *  \param OUT_FIXED_POINT Output fixed point format
 *  \param APPLY_PA Apply predicted average
 *  \param DITHER Apply dithering
 *
 *  Further macros are used to pull the 4 parameters out as individual compile time macros for
 *  comparison within upscale loops.
 */
#define VN_UPSCALE_LOOP_CONFIG(IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER) \
    IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER
#define VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT(...) \
    VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT(...) \
    VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_APPLY_PA(...) VN_UPSCALE_LOOP_CONFIG_APPLY_PA_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_DITHER(...) VN_UPSCALE_LOOP_CONFIG_DITHER_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER) \
    IN_FIXED_POINT
#define VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER) \
    OUT_FIXED_POINT
#define VN_UPSCALE_LOOP_CONFIG_APPLY_PA_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER) \
    APPLY_PA
#define VN_UPSCALE_LOOP_CONFIG_DITHER_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT, APPLY_PA, DITHER) DITHER

/* Scalar path has runtime PA/dither control, so only fixed-point types are encoded. */
#define VN_UPSCALE_LOOP_CONFIG_SCALAR(IN_FIXED_POINT, OUT_FIXED_POINT) \
    IN_FIXED_POINT, OUT_FIXED_POINT
#define VN_UPSCALE_LOOP_CONFIG_SCALAR_IN_FIXED_POINT(...) \
    VN_UPSCALE_LOOP_CONFIG_SCALAR_IN_FIXED_POINT_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_SCALAR_OUT_FIXED_POINT(...) \
    VN_UPSCALE_LOOP_CONFIG_SCALAR_OUT_FIXED_POINT_IMPL(__VA_ARGS__)
#define VN_UPSCALE_LOOP_CONFIG_SCALAR_IN_FIXED_POINT_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT) \
    IN_FIXED_POINT
#define VN_UPSCALE_LOOP_CONFIG_SCALAR_OUT_FIXED_POINT_IMPL(IN_FIXED_POINT, OUT_FIXED_POINT) \
    OUT_FIXED_POINT

// Clamp value to range [lo, hi]
static inline int32_t clampInt(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

// Get pointer to row y of plane (no bounds checking)
static inline int16_t* getRowS16(uint8_t* firstSample, size_t rowStride, int32_t y)
{
    return (int16_t*)(firstSample + (rowStride * y));
}

static inline uint8_t* getRowU8(uint8_t* firstSample, size_t rowStride, int32_t y)
{
    return (uint8_t*)(firstSample + (rowStride * y));
}

#endif // VN_LCEVC_PIXEL_PROCESSING_UPSCALE_COMMON_H
