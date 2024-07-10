/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "surface/upscale_neon.h"

#if VN_CORE_FEATURE(NEON)

#include "common/dither.h"
#include "common/simd.h"
#include "context.h"
#include "surface/upscale.h"
#include "surface/upscale_common.h"
#include "surface/upscale_scalar.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

enum UpscaleConstantsNEON
{
    UCHoriStepping = 8,
    UCHoriLoadAlignment = 16,     /* Horizontal requires 16-values loaded. */
    UCHoriLoadAlignmentNV12 = 32, /* Horizontal NV12 requires 32-values loaded. */
    UCHoriLoadAlignmentRGB = 48,  /* Horizontal RGB requires 48-values loaded. */
    UCHoriLoadAlignmentRGBA = 64, /* Horizontal RGBA requires 64-values loaded. */
    UCMaxKernelSize = 6,
    UCInverseShift = 14,
};

/*------------------------------------------------------------------------------*/

/*!
 * Loads a single channel of pixels in the high-half of a register.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 *
 * \return 8-pixels loaded for 1 channel.
 */
static inline uint8x16_t horizontalGetPelsU8(const uint8_t* in, int32_t offset)
{
    return vcombine_u8(vdup_n_u8(0), vld1_u8(&in[offset]));
}

/*!
 * Loads a single channel of pixels in the high register.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 *
 * \return 8-pixels loaded for 1 channel.
 */
static inline void horizontalGetPelsN16(const uint8_t* in, int32_t offset, int16x8x2_t* pels)
{
    const int16_t* in16 = (const int16_t*)in;
    pels->val[1] = vld1q_s16(&in16[offset]);
}

/*!
 * Loads 2 channels of pixels in the high-half of 2 registers.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 *
 * \return 8-pixels loaded for 2 channels.
 */
static inline uint8x16x2_t horizontalGetPelsU8NV12(const uint8_t* in, int32_t offset)
{
    const uint8x8x2_t loaded = vld2_u8(&in[offset << 1]);
    const uint8x16x2_t res = {
        {vcombine_u8(vdup_n_u8(0), loaded.val[0]), vcombine_u8(vdup_n_u8(0), loaded.val[1])}};

    return res;
}

/*!
 * Loads 3 channels of pixels in the high-half of 3 registers.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 *
 * \return 8-pixels loaded for 3 channels.
 */
static inline uint8x16x3_t horizontalGetPelsU8RGB(const uint8_t* in, int32_t offset)
{
    const uint8x8x3_t loaded = vld3_u8(&in[offset * 3]);
    const uint8x16x3_t res = {{vcombine_u8(vdup_n_u8(0), loaded.val[0]),
                               vcombine_u8(vdup_n_u8(0), loaded.val[1]),
                               vcombine_u8(vdup_n_u8(0), loaded.val[2])}};

    return res;
}

/*!
 * Loads 4 channels of pixels in the high-half of 4 registers.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 *
 * \return 8-pixels loaded for 4 channels.
 */
static inline uint8x16x4_t horizontalGetPelsU8RGBA(const uint8_t* in, int32_t offset)
{
    const uint8x8x4_t loaded = vld4_u8(&in[offset << 2]);
    const uint8x16x4_t res = {
        {vcombine_u8(vdup_n_u8(0), loaded.val[0]), vcombine_u8(vdup_n_u8(0), loaded.val[1]),
         vcombine_u8(vdup_n_u8(0), loaded.val[2]), vcombine_u8(vdup_n_u8(0), loaded.val[3])}};

    return res;
}

/*!
 * Loads the next pixels for a single channel into the high-half of a register whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 *
 * \return Additional 8-pixels loaded for 1 channel.
 */
static inline void horizontalGetNextPelsU8(const uint8_t* in, int32_t offset, uint8x16_t* pels)
{
    /* Load subsequent 8 pels into high half of uint8x16_t, and move current high half down to low. */
    const uint8x8_t next = vld1_u8(&in[offset]);
    *pels = vcombine_u8(vget_high_u8(*pels), next);
}

/*!
 * Loads the next pixels for a single channel into the high-half of a register whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 *
 * \return Additional 8-pixels loaded for 1 channel.
 */
static inline void horizontalGetNextPelsN16(const uint8_t* in, int32_t offset, int16x8x2_t* pels)
{
    const int16_t* in16 = (const int16_t*)in;

    pels->val[0] = pels->val[1];
    pels->val[1] = vld1q_s16(&in16[offset]);
}

/*!
 * Loads the next pixels for 2 channels into the high-half of 2 registers whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 */
static inline void horizontalGetNextPelsU8NV12(const uint8_t* in, int32_t offset, uint8x16x2_t* pels)
{
    const uint8x8x2_t next = vld2_u8(&in[offset << 1]);
    pels->val[0] = vcombine_u8(vget_high_u8(pels->val[0]), next.val[0]);
    pels->val[1] = vcombine_u8(vget_high_u8(pels->val[1]), next.val[1]);
}

/*!
 * Loads the next pixels for 3 channels into the high-half of 3 registers whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 *
 * \return Additional 8-pixels loaded for 3 channels.
 */
static inline void horizontalGetNextPelsU8_RGB(const uint8_t* in, int32_t offset, uint8x16x3_t* pels)
{
    const uint8x8x3_t next = vld3_u8(&in[offset * 3]);
    pels->val[0] = vcombine_u8(vget_high_u8(pels->val[0]), next.val[0]);
    pels->val[1] = vcombine_u8(vget_high_u8(pels->val[1]), next.val[1]);
    pels->val[2] = vcombine_u8(vget_high_u8(pels->val[2]), next.val[2]);
}

/*!
 * Loads the next pixels for 4 channels into the high-half of 4 registers whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 *
 * \return Additional 8-pixels loaded for 4 channels.
 */
static inline void horizontalGetNextPelsU8_RGBA(const uint8_t* in, int32_t offset, uint8x16x4_t* pels)
{
    const uint8x8x4_t next = vld4_u8(&in[offset << 2]);
    pels->val[0] = vcombine_u8(vget_high_u8(pels->val[0]), next.val[0]);
    pels->val[1] = vcombine_u8(vget_high_u8(pels->val[1]), next.val[1]);
    pels->val[2] = vcombine_u8(vget_high_u8(pels->val[2]), next.val[2]);
    pels->val[3] = vcombine_u8(vget_high_u8(pels->val[3]), next.val[3]);
}

/*!
 * Performs horizontal convolution of input pels into result applying the forward
 * and reverse kernels accordingly, whereby the first result pixel will have the
 * reverse kernel applied, due to upscaling being off-pixel.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param result          Place to store the resultant 16-pixels.
 * \param kernel_fwd      The forward kernel.
 * \param kernel_rev      The reverse kernel.
 * \param kernel_length   The length of both kernel_fwd and kernel_rev.
 */
static inline void horizontalConvolveU8(uint8x16_t pels, int16x8x2_t* result, const int16_t* kernelFwd,
                                        const int16_t* kernelRev, int32_t kernelLength)
{
    int32x4_t values[4];
    int16x4x2_t combine[2];

    int16x8_t tap = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pels)));

    /* Reverse */
    values[0] = vmull_n_s16(vget_low_s16(tap), kernelRev[0]);
    values[2] = vmull_n_s16(vget_high_s16(tap), kernelRev[0]);

    /* Shift down */
    pels = vextq_u8(pels, pels, 1);
    tap = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pels)));

    /* Forward */
    values[1] = vmull_n_s16(vget_low_s16(tap), kernelFwd[0]);
    values[3] = vmull_n_s16(vget_high_s16(tap), kernelFwd[0]);

    for (int32_t i = 1; i < kernelLength; i++) {
        /* Reverse */
        values[0] = vmlal_n_s16(values[0], vget_low_s16(tap), kernelRev[i]);
        values[2] = vmlal_n_s16(values[2], vget_high_s16(tap), kernelRev[i]);

        /* Shift */
        pels = vextq_u8(pels, pels, 1);
        tap = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pels)));

        /* Forward */
        values[1] = vmlal_n_s16(values[1], vget_low_s16(tap), kernelFwd[i]);
        values[3] = vmlal_n_s16(values[3], vget_high_s16(tap), kernelFwd[i]);
    }

    /* Scale back to 8 bits */
    combine[0].val[0] = vqrshrn_n_s32(values[0], UCInverseShift);
    combine[0].val[1] = vqrshrn_n_s32(values[1], UCInverseShift);
    combine[1].val[0] = vqrshrn_n_s32(values[2], UCInverseShift);
    combine[1].val[1] = vqrshrn_n_s32(values[3], UCInverseShift);

    /* Interleave */
    combine[0] = vzip_s16(combine[0].val[0], combine[0].val[1]);
    combine[1] = vzip_s16(combine[1].val[0], combine[1].val[1]);

    /* Output */
    result->val[0] = vcombine_s16(combine[0].val[0], combine[0].val[1]);
    result->val[1] = vcombine_s16(combine[1].val[0], combine[1].val[1]);
}

/*!
 * Performs horizontal convolution of input pels into result applying the forward
 * and reverse kernels accordingly, whereby the first result pixel will have the
 * reverse kernel applied, due to upscaling being off-pixel.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param result          Place to store the resultant 16-pixels.
 * \param kernel_fwd      The forward kernel.
 * \param kernel_rev      The reverse kernel.
 * \param kernel_length   The length of both kernel_fwd and kernel_rev.
 */
static inline void horizontalConvolveN16(int16x8x2_t pels, int16x8x2_t* result, const int16_t* kernelFwd,
                                         const int16_t* kernelRev, int32_t kernelLength)
{
    int32x4_t values[4];
    int16x4x2_t combine[2];

    /* Reverse */
    values[0] = vmull_n_s16(vget_low_s16(pels.val[0]), kernelRev[0]);
    values[2] = vmull_n_s16(vget_high_s16(pels.val[0]), kernelRev[0]);

    /* Shift down */
    pels.val[0] = vextq_s16(pels.val[0], pels.val[1], 1);
    pels.val[1] = vextq_s16(pels.val[1], pels.val[1], 1);

    /* Forward */
    values[1] = vmull_n_s16(vget_low_s16(pels.val[0]), kernelFwd[0]);
    values[3] = vmull_n_s16(vget_high_s16(pels.val[0]), kernelFwd[0]);

    for (int32_t i = 1; i < kernelLength; i++) {
        /* Reverse */
        values[0] = vmlal_n_s16(values[0], vget_low_s16(pels.val[0]), kernelRev[i]);
        values[2] = vmlal_n_s16(values[2], vget_high_s16(pels.val[0]), kernelRev[i]);

        /* Shift */
        pels.val[0] = vextq_s16(pels.val[0], pels.val[1], 1);
        pels.val[1] = vextq_s16(pels.val[1], pels.val[1], 1);

        /* Forward */
        values[1] = vmlal_n_s16(values[1], vget_low_s16(pels.val[0]), kernelFwd[i]);
        values[3] = vmlal_n_s16(values[3], vget_high_s16(pels.val[0]), kernelFwd[i]);
    }

    /* Scale back to 16 bits */
    combine[0].val[0] = vqrshrn_n_s32(values[0], UCInverseShift);
    combine[0].val[1] = vqrshrn_n_s32(values[1], UCInverseShift);
    combine[1].val[0] = vqrshrn_n_s32(values[2], UCInverseShift);
    combine[1].val[1] = vqrshrn_n_s32(values[3], UCInverseShift);

    /* Interleave */
    combine[0] = vzip_s16(combine[0].val[0], combine[0].val[1]);
    combine[1] = vzip_s16(combine[1].val[0], combine[1].val[1]);

    /* Output */
    result->val[0] = vcombine_s16(combine[0].val[0], combine[0].val[1]);
    result->val[1] = vcombine_s16(combine[1].val[0], combine[1].val[1]);
}

/*!
 * Apply 1D predicted-average to values using base for a single row.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to.
 */
static inline void applyPA1D(int16x8_t base, int16x8x2_t* values)
{
    /* avg = base - ((pel_even + pel_odd + 1) >> 1) */
    const int16x8_t sum =
        vcombine_s16(vpadd_s16(vget_low_s16(values->val[0]), vget_high_s16(values->val[0])),
                     vpadd_s16(vget_low_s16(values->val[1]), vget_high_s16(values->val[1])));
    const int16x8_t avg = vsubq_s16(base, vrshrq_n_s16(sum, 1));

    /* Repeat each avg to apply to values. */
    const int16x8x2_t broadcast = vzipq_s16(avg, avg);
    values->val[0] = vaddq_s16(values->val[0], broadcast.val[0]);
    values->val[1] = vaddq_s16(values->val[1], broadcast.val[1]);
}

/*!
 * Apply 1D predicted-average to values using base for a single row.
 *
 * See `horizontal_apply_pa_2d_precision` for more detail on this specialisation.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to.
 */
static inline void applyPA1DPrecision(int16x8_t base, int16x8x2_t* values)
{
    /* avg = base - ((pel_even + pel_odd + 1) >> 1) */
    const int32x4_t tmp0 = vpaddlq_s16(values->val[0]);
    const int32x4_t tmp1 = vpaddlq_s16(values->val[1]);
    const int16x8_t sum = vcombine_s16(vrshrn_n_s32(tmp0, 1), vrshrn_n_s32(tmp1, 1));
    const int16x8_t avg = vsubq_s16(base, sum);

    /* Repeat each avg to apply to values. */
    const int16x8x2_t broadcast = vzipq_s16(avg, avg);
    values->val[0] = vqaddq_s16(values->val[0], broadcast.val[0]);
    values->val[1] = vqaddq_s16(values->val[1], broadcast.val[1]);
}

/*!
 * Apply 2D predicted-average to values using base, this requires 2 upscaled rows.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to for 2 rows.
 */
static inline void applyPA2DSpeed(int16x8_t base, int16x8x2_t values[2])
{
    /* avg = base - ((row0_pel_even + row0_pel_odd + row1_pel_even + row1_pel_odd + 2) >> 2) */
    const int16x8_t sum0 =
        vcombine_s16(vpadd_s16(vget_low_s16(values[0].val[0]), vget_high_s16(values[0].val[0])),
                     vpadd_s16(vget_low_s16(values[0].val[1]), vget_high_s16(values[0].val[1])));

    const int16x8_t sum1 =
        vcombine_s16(vpadd_s16(vget_low_s16(values[1].val[0]), vget_high_s16(values[1].val[0])),
                     vpadd_s16(vget_low_s16(values[1].val[1]), vget_high_s16(values[1].val[1])));

    const int16x8_t avg = vsubq_s16(base, vrshrq_n_s16(vaddq_s16(sum0, sum1), 2));

    /* Repeat each avg to apply to values. */
    const int16x8x2_t broadcast = vzipq_s16(avg, avg);
    values[0].val[0] = vaddq_s16(values[0].val[0], broadcast.val[0]);
    values[0].val[1] = vaddq_s16(values[0].val[1], broadcast.val[1]);
    values[1].val[0] = vaddq_s16(values[1].val[0], broadcast.val[0]);
    values[1].val[1] = vaddq_s16(values[1].val[1], broadcast.val[1]);
}

/*!
 * Apply 2D predicted-average to values using base, this requires 2 upscaled rows.
 *
 * This is a specialised version of the function that promotes the math to 32-bit
 * as the average calculation for S16 & U14 can trivially overflow - the none-S16 variant
 * is intended to consume numbers between U8 and U12 which have enough headroom bits
 * to allow the average to be performed in 16-bit.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to for 2 rows.
 */
static inline void applyPA2DPrecision(int16x8_t base, int16x8x2_t values[2])
{
    /* avg = base - ((row0_pel_even + row0_pel_odd + row1_pel_even + row1_pel_odd + 2) >> 2) */
    const int32x4_t tmp0 = vpaddlq_s16(values[0].val[0]);
    const int32x4_t tmp1 = vpaddlq_s16(values[0].val[1]);
    const int32x4_t tmp2 = vpaddlq_s16(values[1].val[0]);
    const int32x4_t tmp3 = vpaddlq_s16(values[1].val[1]);
    const int32x4_t sum0 = vaddq_s32(tmp0, tmp2);
    const int32x4_t sum1 = vaddq_s32(tmp1, tmp3);
    const int16x8_t sum = vcombine_s16(vrshrn_n_s32(sum0, 2), vrshrn_n_s32(sum1, 2));
    const int16x8_t avg = vsubq_s16(base, sum);

    /* Repeat each avg to apply to values. */
    const int16x8x2_t broadcast = vzipq_s16(avg, avg);
    values[0].val[0] = vqaddq_s16(values[0].val[0], broadcast.val[0]);
    values[0].val[1] = vqaddq_s16(values[0].val[1], broadcast.val[1]);
    values[1].val[0] = vqaddq_s16(values[1].val[0], broadcast.val[0]);
    values[1].val[1] = vqaddq_s16(values[1].val[1], broadcast.val[1]);
}

/*!
 * Apply dithering to values using supplied host buffer pointer containing pre-randomised
 * values.
 *
 * This function loads in values from the dither buffer, then moves the buffer pointer
 * forward for the next invocation of this function.
 *
 * \param values   The values to apply dithering to.
 * \param buffer   A double pointer to the dither buffer pointer.
 */
static inline void applyDither(int16x8x2_t* values, const int8_t** buffer)
{
    const int8x16_t ditherLoad = vld1q_s8(*buffer);
    *buffer += 16;

    values->val[0] = vqaddq_s16(values->val[0], vmovl_s8(vget_low_s8(ditherLoad)));
    values->val[1] = vqaddq_s16(values->val[1], vmovl_s8(vget_high_s8(ditherLoad)));
}

/*! \brief Planar horizontal upscaling of 2 rows. */
void horizontalU8PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                            const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                            const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    uint8x16_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignment, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalU8Planar(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0] = horizontalGetPelsU8(in[0], loadOffset);
    pels[1] = horizontalGetPelsU8(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 1);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsU8(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsU8(in[1], loadOffset, &pels[1]);

        horizontalConvolveU8(pels[0], &values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveU8(pels[1], &values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            const int16x8_t basePels0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(&base[0][x])));
            const int16x8_t basePels1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(&base[1][x])));

            applyPA1D(basePels0, &values[0]);
            applyPA1D(basePels1, &values[1]);
        } else if (paEnabled) {
            const int16x8_t basePels = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(&base[0][x])));
            applyPA2DSpeed(basePels, values);
        }

        if (ditherBuffer) {
            applyDither(&values[0], &ditherBuffer);
            applyDither(&values[1], &ditherBuffer);
        }

        vst1q_u8(&out[0][storeOffset], packS16ToU8NEON(values[0]));
        vst1q_u8(&out[1][storeOffset], packS16ToU8NEON(values[1]));

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 1);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8Planar(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief Planar horizontal upscaling of 2 rows. */
void horizontalS16PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                             const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                             const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int16x8x2_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    int16_t* out16[2] = {(int16_t*)out[0], (int16_t*)out[1]};
    const int16_t* base16[2] = {(const int16_t*)base[0], (const int16_t*)base[1]};

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignment, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalS16Planar(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    horizontalGetPelsN16(in[0], loadOffset, &pels[0]);
    horizontalGetPelsN16(in[1], loadOffset, &pels[1]);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 1);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsN16(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsN16(in[1], loadOffset, &pels[1]);

        horizontalConvolveN16(pels[0], &values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveN16(pels[1], &values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            const int16x8_t basePels0 = vld1q_s16(&base16[0][x]);
            const int16x8_t basePels1 = vld1q_s16(&base16[1][x]);

            applyPA1DPrecision(basePels0, &values[0]);
            applyPA1DPrecision(basePels1, &values[1]);
        } else if (paEnabled) {
            const int16x8_t basePels = vld1q_s16(&base16[0][x]);
            applyPA2DPrecision(basePels, values);
        }

        if (ditherBuffer) {
            applyDither(&values[0], &ditherBuffer);
            applyDither(&values[1], &ditherBuffer);
        }

        vst1q_s16(&out16[0][storeOffset], values[0].val[0]);
        vst1q_s16(&out16[0][storeOffset + 8], values[0].val[1]);

        vst1q_s16(&out16[1][storeOffset], values[1].val[0]);
        vst1q_s16(&out16[1][storeOffset + 8], values[1].val[1]);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 1);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalS16Planar(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief Planar horizontal upscaling of 2 rows. */
static inline void horizontalU16PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                           const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                           uint32_t xEnd, const Kernel_t* kernel, uint16_t maxValue,
                                           bool is14Bit)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int16x8x2_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint16_t* out16[2] = {(uint16_t*)out[0], (uint16_t*)out[1]};
    const int16_t* base16[2] = {(const int16_t*)base[0], (const int16_t*)base[1]};
    const int16x8_t minV = vdupq_n_s16(0);
    const int16x8_t maxV = vdupq_n_s16((int16_t)maxValue);

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignment, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalUNPlanar(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel, maxValue);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    horizontalGetPelsN16(in[0], loadOffset, &pels[0]);
    horizontalGetPelsN16(in[1], loadOffset, &pels[1]);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 1);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsN16(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsN16(in[1], loadOffset, &pels[1]);

        horizontalConvolveN16(pels[0], &values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveN16(pels[1], &values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            const int16x8_t basePels0 = vld1q_s16(&base16[0][x]);
            const int16x8_t basePels1 = vld1q_s16(&base16[1][x]);

            applyPA1D(basePels0, &values[0]);
            applyPA1D(basePels1, &values[1]);
        } else if (paEnabled) {
            const int16x8_t basePels = vld1q_s16(&base16[0][x]);

            if (is14Bit) {
                applyPA2DPrecision(basePels, values);
            } else {
                applyPA2DSpeed(basePels, values);
            }
        }

        if (ditherBuffer) {
            applyDither(&values[0], &ditherBuffer);
            applyDither(&values[1], &ditherBuffer);
        }

        vst1q_u16(&out16[0][storeOffset], clampS16ToU16(values[0].val[0], minV, maxV));
        vst1q_u16(&out16[0][storeOffset + 8], clampS16ToU16(values[0].val[1], minV, maxV));

        vst1q_u16(&out16[1][storeOffset], clampS16ToU16(values[1].val[0], minV, maxV));
        vst1q_u16(&out16[1][storeOffset + 8], clampS16ToU16(values[1].val[1], minV, maxV));

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 1);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalUNPlanar(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel,
                           maxValue);
    }
}

/*! \brief U10 Planar horizontal upscaling of 2 rows. */
void horizontalU10PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                             const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                             const Kernel_t* kernel)
{
    horizontalU16PlanarNEON(dither, in, out, base, width, xStart, xEnd, kernel, 1023, false);
}

/*! \brief U12 Planar horizontal upscaling of 2 rows. */
void horizontalU12PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                             const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                             const Kernel_t* kernel)
{
    horizontalU16PlanarNEON(dither, in, out, base, width, xStart, xEnd, kernel, 4095, false);
}

/*! \brief U14 Planar horizontal upscaling of 2 rows. */
void horizontalU14PlanarNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                             const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                             const Kernel_t* kernel)
{
    horizontalU16PlanarNEON(dither, in, out, base, width, xStart, xEnd, kernel, 16383, true);
}

/*! \brief NV12 horizontal upscaling of 2 rows. */
void horizontalU8NV12NEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                          uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    uint8x16x2_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint32_t channelIdx = 0;
    uint8x8x2_t basePels[2];
    int16x8x2_t result[2][2];
    uint8x16x2_t packed;

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignmentNV12, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalU8NV12(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0] = horizontalGetPelsU8NV12(in[0], loadOffset);
    pels[1] = horizontalGetPelsU8NV12(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 2);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsU8NV12(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsU8NV12(in[1], loadOffset, &pels[1]);

        if (paEnabled1D) {
            basePels[0] = vld2_u8(&base[0][x << 1]);
            basePels[1] = vld2_u8(&base[1][x << 1]);
        } else if (paEnabled) {
            basePels[0] = vld2_u8(&base[0][x << 1]);
        }

        for (channelIdx = 0; channelIdx < 2; ++channelIdx) {
            horizontalConvolveU8(pels[0].val[channelIdx], &values[0], kernelFwd, kernelRev, kernelLength);
            horizontalConvolveU8(pels[1].val[channelIdx], &values[1], kernelFwd, kernelRev, kernelLength);

            if (paEnabled1D) {
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), &values[0]);
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[1].val[channelIdx])), &values[1]);
            } else if (paEnabled) {
                applyPA2DSpeed(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), values);
            }

            if (ditherBuffer) {
                applyDither(&values[0], &ditherBuffer);
                applyDither(&values[1], &ditherBuffer);
            }

            /* Stash result */
            result[0][channelIdx] = values[0];
            result[1][channelIdx] = values[1];
        }

        packed.val[0] = packS16ToU8NEON(result[0][0]);
        packed.val[1] = packS16ToU8NEON(result[0][1]);
        vst2q_u8(&out[0][storeOffset], packed);

        packed.val[0] = packS16ToU8NEON(result[1][0]);
        packed.val[1] = packS16ToU8NEON(result[1][1]);
        vst2q_u8(&out[1][storeOffset], packed);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 2);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8NV12(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief RGB horizontal upscaling of 2 rows. */
void horizontalU8RGBNEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                         uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    uint8x16x3_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint32_t channelIdx = 0;
    uint8x8x3_t basePels[2];
    int16x8x2_t result[2][3];
    uint8x16x3_t packed;

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignmentRGB, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalU8RGB(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0] = horizontalGetPelsU8RGB(in[0], loadOffset);
    pels[1] = horizontalGetPelsU8RGB(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start * 6);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsU8_RGB(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsU8_RGB(in[1], loadOffset, &pels[1]);

        if (paEnabled1D) {
            basePels[0] = vld3_u8(&base[0][x * 3]);
            basePels[1] = vld3_u8(&base[1][x * 3]);
        } else if (paEnabled) {
            basePels[0] = vld3_u8(&base[0][x * 3]);
        }

        for (channelIdx = 0; channelIdx < 3; ++channelIdx) {
            horizontalConvolveU8(pels[0].val[channelIdx], &values[0], kernelFwd, kernelRev, kernelLength);
            horizontalConvolveU8(pels[1].val[channelIdx], &values[1], kernelFwd, kernelRev, kernelLength);

            if (paEnabled1D) {
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), &values[0]);
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[1].val[channelIdx])), &values[1]);
            } else if (paEnabled) {
                applyPA2DSpeed(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), values);
            }

            if (ditherBuffer) {
                applyDither(&values[0], &ditherBuffer);
                applyDither(&values[1], &ditherBuffer);
            }

            /* Stash result */
            result[0][channelIdx] = values[0];
            result[1][channelIdx] = values[1];
        }

        packed.val[0] = packS16ToU8NEON(result[0][0]);
        packed.val[1] = packS16ToU8NEON(result[0][1]);
        packed.val[2] = packS16ToU8NEON(result[0][2]);
        vst3q_u8(&out[0][storeOffset], packed);

        packed.val[0] = packS16ToU8NEON(result[1][0]);
        packed.val[1] = packS16ToU8NEON(result[1][1]);
        packed.val[2] = packS16ToU8NEON(result[1][2]);
        vst3q_u8(&out[1][storeOffset], packed);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping * 6);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8RGB(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief RGBA horizontal upscaling of 2 rows. */
void horizontalU8RGBANEON(Dither_t dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],
                          uint32_t width, uint32_t xStart, uint32_t xEnd, const Kernel_t* kernel)
{
    uint32_t x;
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    uint8x16x4_t pels[2];
    int16x8x2_t values[2];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint32_t channelIdx = 0;
    uint8x8x4_t basePels[2];
    int16x8x2_t result[2][4];
    uint8x16x4_t packed;

    UpscaleHorizontalCoords_t coords = {0};

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignmentRGBA, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalU8RGBA(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0] = horizontalGetPelsU8RGBA(in[0], loadOffset);
    pels[1] = horizontalGetPelsU8RGBA(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 3);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsU8_RGBA(in[0], loadOffset, &pels[0]);
        horizontalGetNextPelsU8_RGBA(in[1], loadOffset, &pels[1]);

        if (paEnabled1D) {
            basePels[0] = vld4_u8(&base[0][x << 2]);
            basePels[1] = vld4_u8(&base[1][x << 2]);
        } else if (paEnabled) {
            basePels[0] = vld4_u8(&base[0][x << 2]);
        }

        for (channelIdx = 0; channelIdx < 4; ++channelIdx) {
            horizontalConvolveU8(pels[0].val[channelIdx], &values[0], kernelFwd, kernelRev, kernelLength);
            horizontalConvolveU8(pels[1].val[channelIdx], &values[1], kernelFwd, kernelRev, kernelLength);

            if (paEnabled1D) {
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), &values[0]);
                applyPA1D(vreinterpretq_s16_u16(vmovl_u8(basePels[1].val[channelIdx])), &values[1]);
            } else if (paEnabled) {
                applyPA2DSpeed(vreinterpretq_s16_u16(vmovl_u8(basePels[0].val[channelIdx])), values);
            }

            if (ditherBuffer) {
                applyDither(&values[0], &ditherBuffer);
                applyDither(&values[1], &ditherBuffer);
            }

            /* Stash result */
            result[0][channelIdx] = values[0];
            result[1][channelIdx] = values[1];
        }

        packed.val[0] = packS16ToU8NEON(result[0][0]);
        packed.val[1] = packS16ToU8NEON(result[0][1]);
        packed.val[2] = packS16ToU8NEON(result[0][2]);
        packed.val[3] = packS16ToU8NEON(result[0][3]);
        vst4q_u8(&out[0][storeOffset], packed);

        packed.val[0] = packS16ToU8NEON(result[1][0]);
        packed.val[1] = packS16ToU8NEON(result[1][1]);
        packed.val[2] = packS16ToU8NEON(result[1][2]);
        packed.val[3] = packS16ToU8NEON(result[1][3]);
        vst4q_u8(&out[1][storeOffset], packed);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 3);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8RGBA(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*------------------------------------------------------------------------------*/

/*!
 * Loads kernel-length rows of initial upscale input data ensuring that edge extension
 * is performed.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row offset to start loading from.
 * \param  count    The number of rows to load in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetPelsU8(const uint8_t* in, uint32_t height, uint32_t stride,
                                     int32_t offset, int32_t count, uint8x16_t pels[UCMaxKernelSize])
{
    for (int32_t i = 0; i < count; ++i) {
        const int32_t rowIdx = clampS32(offset + i, 0, (int32_t)height - 1);
        pels[i] = vld1q_u8(&in[rowIdx * stride]);
    }
}

/*!
 * Loads kernel-length rows of initial upscale input data ensuring that edge extension
 * is performed.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row offset to start loading from.
 * \param  count    The number of rows to load in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetPelsN16(const uint8_t* in, uint32_t height, uint32_t stride,
                                      int32_t offset, int32_t count, int16x8x2_t pels[UCMaxKernelSize])
{
    const int16_t* in16 = (const int16_t*)in;

    for (int32_t i = 0; i < count; ++i) {
        const int32_t rowIdx = clampS32(offset + i, 0, (int32_t)height - 1);
        const size_t rowOffset = (size_t)rowIdx * stride;
        pels[i].val[0] = vld1q_s16(&in16[rowOffset]);
        pels[i].val[1] = vld1q_s16(&in16[rowOffset + 8]);
    }
}

/*!
 * Loads the next row of upscale input data by shuffling the pels down 1, and loading
 * next row into the last entry. This function ensures that edge extension is performed.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row to load from.
 * \param  count    The number of rows loaded in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetNextPelsU8(const uint8_t* in, uint32_t height, uint32_t stride,
                                         int32_t offset, int32_t count, uint8x16_t pels[UCMaxKernelSize])
{
    /* Shift pels */
    for (int32_t i = 1; i < count; ++i) {
        pels[i - 1] = pels[i];
    }

    const size_t rowIdx = (size_t)clampS32(offset + count - 1, 0, (int32_t)height - 1);
    pels[count - 1] = vld1q_u8(&in[rowIdx * stride]);
}

static inline void verticalGetNextPelsN16(const uint8_t* in, uint32_t height, uint32_t stride,
                                          int32_t offset, int32_t count, int16x8x2_t pels[UCMaxKernelSize])
{
    /* Shift pels */
    for (int32_t i = 1; i < count; ++i) {
        pels[i - 1] = pels[i];
    }

    const int32_t rowIndex = clampS32(offset + count - 1, 0, (int32_t)height - 1);
    const size_t rowOffset = (size_t)rowIndex * stride;
    const int16_t* in16 = (const int16_t*)in;
    pels[count - 1].val[0] = vld1q_s16(&in16[rowOffset]);
    pels[count - 1].val[1] = vld1q_s16(&in16[rowOffset + 8]);
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param kernel          The kernel to upscale with
 * \param kernel_length   The length of kernel.
 *
 * \return The result of the convolution saturated and packed back to U8.
 */
static inline uint8x16_t verticalConvolveU8(uint8x16_t pels[UCMaxKernelSize], const int16_t* kernel,
                                            int32_t kernelLength)
{
    int32x4_t values[4];
    int16x8_t tap;
    int16x8x2_t res;

    /* Prime with initial multiply */
    tap = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pels[0])));
    values[0] = vmull_n_s16(vget_low_s16(tap), kernel[0]);
    values[1] = vmull_n_s16(vget_high_s16(tap), kernel[0]);

    tap = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pels[0])));
    values[2] = vmull_n_s16(vget_low_s16(tap), kernel[0]);
    values[3] = vmull_n_s16(vget_high_s16(tap), kernel[0]);

    /* Multiply and accumulate the rest of the kernel */
    for (int32_t i = 1; i < kernelLength; ++i) {
        tap = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pels[i])));
        values[0] = vmlal_n_s16(values[0], vget_low_s16(tap), kernel[i]);
        values[1] = vmlal_n_s16(values[1], vget_high_s16(tap), kernel[i]);

        tap = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pels[i])));
        values[2] = vmlal_n_s16(values[2], vget_low_s16(tap), kernel[i]);
        values[3] = vmlal_n_s16(values[3], vget_high_s16(tap), kernel[i]);
    }

    /* Scale back & combine */
    res.val[0] = vcombine_s16(vqrshrn_n_s32(values[0], UCInverseShift),
                              vqrshrn_n_s32(values[1], UCInverseShift));
    res.val[1] = vcombine_s16(vqrshrn_n_s32(values[2], UCInverseShift),
                              vqrshrn_n_s32(values[3], UCInverseShift));

    return packS16ToU8NEON(res);
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param result          Place to store the resultant 16-pixels.
 * \param kernel          The kernel to upscale with
 * \param kernel_length   The length of kernel.
 * \param result          Place to store the resultant 16-pixels.
 */
static inline void verticalConvolveS16(int16x8x2_t pels[UCMaxKernelSize], const int16_t* kernel,
                                       int32_t kernelLength, int16x8x2_t* result)
{
    int32x4_t values[4];

    /* Prime with initial multiply */
    values[0] = vmull_n_s16(vget_low_s16(pels[0].val[0]), kernel[0]);
    values[1] = vmull_n_s16(vget_high_s16(pels[0].val[0]), kernel[0]);
    values[2] = vmull_n_s16(vget_low_s16(pels[0].val[1]), kernel[0]);
    values[3] = vmull_n_s16(vget_high_s16(pels[0].val[1]), kernel[0]);

    /* Multiply and accumulate the rest of the kernel */
    for (int32_t i = 1; i < kernelLength; ++i) {
        values[0] = vmlal_n_s16(values[0], vget_low_s16(pels[i].val[0]), kernel[i]);
        values[1] = vmlal_n_s16(values[1], vget_high_s16(pels[i].val[0]), kernel[i]);
        values[2] = vmlal_n_s16(values[2], vget_low_s16(pels[i].val[1]), kernel[i]);
        values[3] = vmlal_n_s16(values[3], vget_high_s16(pels[i].val[1]), kernel[i]);
    }

    /* Scale back & combine with a saturating shift from int32_t to unsigned N-bits. */
    result->val[0] = vcombine_s16(vqrshrn_n_s32(values[0], UCInverseShift),
                                  vqrshrn_n_s32(values[1], UCInverseShift));

    result->val[1] = vcombine_s16(vqrshrn_n_s32(values[2], UCInverseShift),
                                  vqrshrn_n_s32(values[3], UCInverseShift));
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param result          Place to store the resultant 16-pixels.
 * \param kernel          The kernel to upscale with
 * \param kernel_length   The length of kernel.
 * \param result          Place to store the resultant 16-pixels.
 */
static inline void verticalConvolveU16(int16x8x2_t pels[UCMaxKernelSize], const int16_t* kernel,
                                       int32_t kernelLength, uint16x8_t maxValue, uint16x8x2_t* result)
{
    int32x4_t values[4];

    /* Prime with initial multiply */
    values[0] = vmull_n_s16(vget_low_s16(pels[0].val[0]), kernel[0]);
    values[1] = vmull_n_s16(vget_high_s16(pels[0].val[0]), kernel[0]);
    values[2] = vmull_n_s16(vget_low_s16(pels[0].val[1]), kernel[0]);
    values[3] = vmull_n_s16(vget_high_s16(pels[0].val[1]), kernel[0]);

    /* Multiply and accumulate the rest of the kernel */
    for (int32_t i = 1; i < kernelLength; ++i) {
        values[0] = vmlal_n_s16(values[0], vget_low_s16(pels[i].val[0]), kernel[i]);
        values[1] = vmlal_n_s16(values[1], vget_high_s16(pels[i].val[0]), kernel[i]);
        values[2] = vmlal_n_s16(values[2], vget_low_s16(pels[i].val[1]), kernel[i]);
        values[3] = vmlal_n_s16(values[3], vget_high_s16(pels[i].val[1]), kernel[i]);
    }

    /* Scale back & combine with a saturating shift from int32_t to unsigned N-bits. */
    result->val[0] = vcombine_u16(vqrshrun_n_s32(values[0], UCInverseShift),
                                  vqrshrun_n_s32(values[1], UCInverseShift));
    result->val[0] = vminq_u16(result->val[0], maxValue);

    result->val[1] = vcombine_u16(vqrshrun_n_s32(values[2], UCInverseShift),
                                  vqrshrun_n_s32(values[3], UCInverseShift));
    result->val[1] = vminq_u16(result->val[1], maxValue);
}

/*! \brief Vertical upscaling of 16 columns. */
static void verticalU8NEON(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                           uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;

    uint32_t rowIndex = 0;
    const uint32_t outSkip = 2 * outStride;
    uint8_t* out0 = out + ((size_t)y * outSkip);
    uint8_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    uint8x16_t pels[UCMaxKernelSize];

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Prime rows. */
    verticalGetPelsU8(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    for (rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        vst1q_u8(out0, verticalConvolveU8(pels, kernelRev, kernelLength));

        /* Next input due to being off-pixel */
        verticalGetNextPelsU8(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        vst1q_u8(out1, verticalConvolveU8(pels, kernelFwd, kernelLength));

        out0 += outSkip;
        out1 += outSkip;
    }
}

/*! \brief S16 vertical upscaling of 16 columns. */
static void verticalS16NEON(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                            uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;

    uint32_t rowIndex = 0;
    const uint32_t outSkip = 2 * outStride;
    int16_t* out16 = (int16_t*)out;
    int16_t* out0 = out16 + ((size_t)y * outSkip);
    int16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    int16x8x2_t pels[UCMaxKernelSize];
    int16x8x2_t result;

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Prime rows. */
    verticalGetPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    for (rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        verticalConvolveS16(pels, kernelRev, kernelLength, &result);
        vst1q_s16(out0, result.val[0]);
        vst1q_s16(out0 + 8, result.val[1]);

        /* Next input due to being off-pixel */
        verticalGetNextPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        verticalConvolveS16(pels, kernelFwd, kernelLength, &result);
        vst1q_s16(out1, result.val[0]);
        vst1q_s16(out1 + 8, result.val[1]);

        out0 += outSkip;
        out1 += outSkip;
    }
}

/*! \brief U16 vertical upscaling of 16 columns. */
static inline void verticalU16NEON(const uint8_t* in, uint32_t inStride, uint8_t* out,
                                   uint32_t outStride, uint32_t y, uint32_t rows, uint32_t height,
                                   const Kernel_t* kernel, uint16_t maxValue)
{
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;

    uint32_t rowIndex = 0;
    const uint32_t outSkip = 2 * outStride;
    uint16_t* out16 = (uint16_t*)out;
    uint16_t* out0 = out16 + ((size_t)y * outSkip);
    uint16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    const uint16x8_t maxV = vdupq_n_u16(maxValue);
    int16x8x2_t pels[UCMaxKernelSize];
    uint16x8x2_t result;

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Prime rows. */
    verticalGetPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    for (rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        verticalConvolveU16(pels, kernelRev, kernelLength, maxV, &result);
        vst1q_u16(out0, result.val[0]);
        vst1q_u16(out0 + 8, result.val[1]);

        /* Next input due to being off-pixel */
        verticalGetNextPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        verticalConvolveU16(pels, kernelFwd, kernelLength, maxV, &result);
        vst1q_u16(out1, result.val[0]);
        vst1q_u16(out1 + 8, result.val[1]);

        out0 += outSkip;
        out1 += outSkip;
    }
}

/*! \brief U10 vertical upscaling of 16 columns. */
void verticalU10NEON(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16NEON(in, inStride, out, outStride, y, rows, height, kernel, 1023);
}

/*! \brief U12 vertical upscaling of 16 columns. */
void verticalU12NEON(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16NEON(in, inStride, out, outStride, y, rows, height, kernel, 4095);
}

/*! \brief U14 vertical upscaling of 16 columns. */
void verticalU14NEON(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16NEON(in, inStride, out, outStride, y, rows, height, kernel, 16383);
}

/*------------------------------------------------------------------------------*/

/* clang-format off */

static const UpscaleHorizontal_t kHorizontalFunctionTable[ILCount][FPCount] = {
	/* U8,                   U10,                     U12,                     U14,                     S8.7,                    S10.5,                   S12.3,                   S14.1 */
	{horizontalU8PlanarNEON, horizontalU10PlanarNEON, horizontalU12PlanarNEON, horizontalU14PlanarNEON, horizontalS16PlanarNEON, horizontalS16PlanarNEON, horizontalS16PlanarNEON, horizontalS16PlanarNEON}, /* None*/
	{NULL,                   NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL},                    /* YUYV */
	{horizontalU8NV12NEON,   NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL},                    /* NV12 */
	{NULL,                   NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL},                    /* UYVY */
	{horizontalU8RGBNEON,    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL},                    /* RGB */
	{horizontalU8RGBANEON,   NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL,                    NULL},                    /* RGBA */
};

/* kVerticalFunctionTable[fp] */
static const UpscaleVertical_t kVerticalFunctionTable[FPCount] = {
	verticalU8NEON,  /* U8 */
	verticalU10NEON, /* U10 */
	verticalU12NEON, /* U12 */
	verticalU14NEON, /* U14 */
	verticalS16NEON, /* S8.7 */
	verticalS16NEON, /* S10.5 */
	verticalS16NEON, /* S12.3 */
	verticalS16NEON, /* S14.1 */
};

/* clang-format on */

/*------------------------------------------------------------------------------*/

UpscaleHorizontal_t upscaleGetHorizontalFunctionNEON(Interleaving_t ilv, FixedPoint_t srcFP,
                                                     FixedPoint_t dstFP, FixedPoint_t baseFP)
{
    /* Conversion is not currently supported in SIMD. */
    if ((srcFP != dstFP) || ((baseFP != dstFP) && fixedPointIsValid(baseFP))) {
        return NULL;
    }

    return kHorizontalFunctionTable[ilv][srcFP];
}

UpscaleVertical_t upscaleGetVerticalFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP)
{
    /* Conversion is not currently supported in SIMD. */
    if (srcFP != dstFP) {
        return NULL;
    }

    return kVerticalFunctionTable[srcFP];
}

/*------------------------------------------------------------------------------*/

#else

UpscaleHorizontal_t upscaleGetHorizontalFunctionNEON(Interleaving_t ilv, FixedPoint_t srcFP,
                                                     FixedPoint_t dstFP, FixedPoint_t baseFP)
{
    VN_UNUSED(ilv);
    VN_UNUSED(srcFP);
    VN_UNUSED(dstFP);
    VN_UNUSED(baseFP);
    return NULL;
}

UpscaleVertical_t upscaleGetVerticalFunctionNEON(FixedPoint_t srcFP, FixedPoint_t dstFP)
{
    VN_UNUSED(srcFP);
    VN_UNUSED(dstFP);
    return NULL;
}

#endif