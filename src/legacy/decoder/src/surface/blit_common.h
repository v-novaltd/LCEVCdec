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

#ifndef VN_LCEVC_LEGACY_BLIT_COMMON_H
#define VN_LCEVC_LEGACY_BLIT_COMMON_H

#include <stdint.h>

/*------------------------------------------------------------------------------*/

/*! \brief Helper macros for performing per-pixel loop boilerplate on Surface_t
 *         objects.
 *
 * They provide the ability to specify the src & dst pointer types and take variable
 * arguments that are forwarded onto VNSurfaceOp(). There are some requirements that
 * must be met before using these macros:
 *
 *    1. A macro `VNSurfaceOp` must be defined, this is the per-pixel operation.
 *    2. An args variable must be in scope that is a pointer to a struct that must
 *       contain the following parameters:
 *           a. `const Surface_t* src` - The surface to read from.
 *           b. `const Surface_t* dst` - The surface to write to.
 *           c. `uint32_t offset`      - The offset row to read from/write to.
 *           d. `uint32_t count`       - The number of rows to process.
 *
 * VNSurfaceOp is expected to read from `srcValue`, which has been cast to int32_t from
 * Src_t, and assign to `dstValue` (as int32_t), this is then cast to Dst_t after
 * the op is executed.
 *
 * This implementation is provided as a way to simulate templates and prevent
 * re-writing the same tedious boilerplate repeatedly. Furthermore it may afford
 * the compiler a better chance at optimizing the inner loop functionality with
 * fixed constants over generalized functions.
 */
#define VN_CALL_OP_GLUE(x, y) x y
#define VN_CALL_OP(...) VN_CALL_OP_GLUE(VN_SURFACE_OP, (__VA_ARGS__))

#define VN_BLIT_PER_PIXEL_BODY(Src_t, Dst_t, ...)                          \
    const Surface_t* src = args->src;                                      \
    const Surface_t* dst = args->dst;                                      \
    const uint32_t xWidth = minU32(src->width, dst->width);                \
    const Src_t* srcRow = (const Src_t*)surfaceGetLine(src, args->offset); \
    Dst_t* dstRow = (Dst_t*)surfaceGetLine(dst, args->offset);             \
    int32_t dstValue = 0;                                                  \
    for (uint32_t y = 0; y < args->count; ++y) {                           \
        const Src_t* srcPixel = srcRow;                                    \
        Dst_t* dstPixel = dstRow;                                          \
        for (uint32_t x = 0; x < xWidth; ++x) {                            \
            int32_t srcValue = (int32_t)*srcPixel++;                       \
            VN_CALL_OP(__VA_ARGS__);                                       \
            *dstPixel++ = (Dst_t)dstValue;                                 \
        }                                                                  \
        srcRow += src->stride;                                             \
        dstRow += dst->stride;                                             \
    }

/*! \brief Helper macro for setting up the boilerplate code used for each specialized
 *  implementation. It initializes several variables that each implementation
 *  will need
 *
 *  src        The source surface to read from.
 *  dst        The destination surface to write to.
 *  width      The overall width to copy.
 *  simdWidth  The number of pixels to operate on in the SIMD loop.
 *  srcRow     Pointer of const Src_t type pointing to first pixel of the
 *             row to copy from.
 *  dstRow     Pointer of Dst_t type pointing to the first pixel of the row
 *             to copy to.
 *
 * Note: This functions requires the declaration of a function with the following
 *       signature:
 *
 *           uint32_t simdAlignment(const uint32_t);
 *
 *       It is intended to take the width to process, and it will return the lower
 *       aligned width for the number of pixels to process in the SIMD loop.
 */
#define VN_BLIT_SIMD_BOILERPLATE(Src_t, Dst_t)                             \
    const Surface_t* src = args->src;                                      \
    const Surface_t* dst = args->dst;                                      \
    const uint32_t width = minU32(src->width, dst->width);                 \
    const uint32_t simdWidth = simdAlignment(width);                       \
    const Src_t* srcRow = (const Src_t*)surfaceGetLine(src, args->offset); \
    Dst_t* dstRow = (Dst_t*)surfaceGetLine(dst, args->offset);

/*------------------------------------------------------------------------------*/

typedef struct Surface Surface_t;

/*------------------------------------------------------------------------------*/

/*! \brief Arguments passed to the specialised blit function implementations. */
typedef struct BlitArgs
{
    const Surface_t* src; /**< Source surface to blit from. */
    const Surface_t* dst; /**< Destination surface to blit to. */
    uint32_t offset;      /**< Row offset to start processing from. */
    uint32_t count;       /**< Number of rows to process. */
} BlitArgs_t;

typedef void (*BlitFunction_t)(const BlitArgs_t* args);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_LEGACY_BLIT_COMMON_H
