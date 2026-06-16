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

#if defined(VN_UPSCALE_LOOP)
#define IN_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_IN_FIXED_POINT(VN_UPSCALE_LOOP)
#define OUT_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_OUT_FIXED_POINT(VN_UPSCALE_LOOP)
#define APPLY_PA VN_UPSCALE_LOOP_CONFIG_APPLY_PA(VN_UPSCALE_LOOP)
#define DITHER VN_UPSCALE_LOOP_CONFIG_DITHER(VN_UPSCALE_LOOP)
#endif
#if !defined(IN_FIXED_POINT) || !defined(OUT_FIXED_POINT) || !defined(APPLY_PA) || !defined(DITHER)
#error "Define VN_UPSCALE_LOOP before including this header"
#endif

{
    VNUnused(srcHeight);
    const int16x8_t kMin16 = vdupq_n_s16(-0x4000); /* see saturateS15 for explanation */
    const int16x8_t kMax16 = vdupq_n_s16(0x4000 - 1);
#if IN_FIXED_POINT != FP_S16
    const int16x8_t kMidpointS16 = vdupq_n_s16(0x4000);
#endif

    // Load fixed point conversion constants to registers
#if OUT_FIXED_POINT == FP_U8
    const int16x8_t kOffset = vdupq_n_s16(64);
    const int16x8_t kMidpoint = vdupq_n_s16(128);
    const int16x8_t kS8Min = vdupq_n_s16(INT8_MIN);
    const int16x8_t kU8Max = vdupq_n_s16(UINT8_MAX);
#elif OUT_FIXED_POINT == FP_U16
    const int16x8_t kOffset = vdupq_n_s16(params.offset);
    const int16x8_t kMidpoint = vdupq_n_s16(params.midpoint);
    const int16x8_t kLeftShift = vdupq_n_s16(params.shift);
    const int16x8_t kRightShift = vdupq_n_s16(-params.shift);
#endif

    // Kernel coefficients broadcast to vectors
    const int16x4_t k0 = vdup_n_s16(params.kernel[0]);
    const int16x4_t k1 = vdup_n_s16(params.kernel[1]);
    const int16x4_t k2 = vdup_n_s16(params.kernel[2]);
    const int16x4_t k3 = vdup_n_s16(params.kernel[3]);

    // Body end for 4-wide kernel (used for edge handling)
    const int32_t bodyEnd = ((srcWidth - 2) & ~3);

    // Process each row
    for (int32_t row = yStart; row < yEnd; ++row) {
#if DITHER == 1
        const uint16_t* ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
        const int16x8_t ditherOffset = vdupq_n_s16((int16_t)dither->strength);
        const int16x8_t ditherShift = vdupq_n_s16(params.shift); // Shift when < 16bit
        const uint16_t ditherMul = (uint16_t)(dither->strength * 2u + 1u);
#else
        VNUnused(dither);
#endif

#if IN_FIXED_POINT == FP_U8
        const uint8_t* const srcRow = getRowU8(src, srcStride, row);
        uint8_t* const dstRow = getRowU8(dst, dstStride, row);
#else // N16
        const int16_t* const srcRow = getRowS16(src, srcStride, row);
        int16_t* const dstRow = getRowS16(dst, dstStride, row);
#endif

        // Start: handle left edge using 4-wide kernel (destX = 0, outputs 0-7)
#define KERNEL_VARIANT 1
#include "neon_1d_kernel.h"

        // Main body loop
        for (int32_t srcX = 4; srcX < bodyEnd; srcX += 4) {
#define KERNEL_VARIANT 0
#include "neon_1d_kernel.h"
        }

        // Final 4 input pixels on the left edge
#define KERNEL_VARIANT 2
#include "neon_1d_kernel.h"
    }
}

#undef DITHER
#undef APPLY_PA
#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
