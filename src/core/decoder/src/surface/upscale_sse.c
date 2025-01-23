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

#include "surface/upscale_sse.h"

#include "common/types.h"
#include "lcevc_config.h"
#include "surface/upscale_common.h"

#include <stddef.h>

#if VN_CORE_FEATURE(SSE)

#include "common/dither.h"
#include "common/platform.h"
#include "common/sse.h"
#include "context.h"
#include "surface/upscale.h"
#include "surface/upscale_scalar.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/*
@TODO(bob): Look at shuffles in PA, there may be "better" ways to do it, if not
            then at least move the control values to a global constant.

@TODO(bob): Since each function here is working on a per-line basis there's a lot
            of setup that can be shared between invocations, look into improving this
            all (or change the calling model entirely).
*/

/*------------------------------------------------------------------------------*/

enum UpscaleConstantsSSE
{
    UCHoriStepping = 8,
    UCHoriLoadAlignment = 16,     /* Horizontal requires 16-values loaded. */
    UCHoriLoadAlignmentNV12 = 32, /* Horizontal NV12 requires 32-values loaded. */
    UCMaxKernelSize = 6,
    UCInterleavedStore = UCMaxKernelSize >> 1, /* PELs and Kernel are pair-wise interleaved. */
    UCVertGroupSize = 4,                       /* U8 converted to S16 with 2 rows interleaved */
    UCInverseShift = 14,
    UCInverseShiftRounding = (1 << (UCInverseShift - 1))
};

VN_ALIGN(static const uint8_t kDeinterleaveControl[16], 16) = {
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F};

/*------------------------------------------------------------------------------*/

/*!
 * Load a single channel of 8-pixels into pels.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 *
 * \return The loaded pixels in the high half of a register.
 */
static inline __m128i horizontalGetPelsU8(const uint8_t* in, int32_t offset)
{
    /* Load initial 8 pels and shift up in preparation for next load.  */
    return _mm_slli_si128(_mm_loadl_epi64((const __m128i*)&in[offset]), 8);
}

/*!
 * Load and return a single channel of 8-pixels
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 *
 * \return The loaded pixels in the high half of a register.
 */
static inline __m128i horizontalGetPelsN16(const uint8_t* in, int32_t offset)
{
    const int16_t* in16 = (const int16_t*)in;
    return _mm_loadu_si128((const __m128i*)&in16[offset]);
}

/*!
 * Load 2 channels and deinterleave into pels.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.1
 * \param pels    The pels to load into.
 */
static inline void horizontalGetPelsU8NV12(const uint8_t* in, int32_t offset, __m128i pels[2])
{
    const __m128i loaded = _mm_loadu_si128((const __m128i*)&in[offset << 1]);
    const __m128i shuffled = _mm_shuffle_epi8(loaded, *(const __m128i*)kDeinterleaveControl);

    pels[0] = _mm_unpacklo_epi64(shuffled, shuffled);
    pels[1] = _mm_unpackhi_epi64(shuffled, shuffled);
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
static inline void horizontalGetNextPelsU8(const uint8_t* in, int32_t offset, __m128i* pels)
{
    /* Load in 8 pels, shift up load, shift down current and merge. This results
     * in the same behaviour as performing a 128-bit load. */
    const __m128i next = _mm_loadl_epi64((const __m128i*)&in[offset]);
    *pels = _mm_or_si128(_mm_srli_si128(*pels, 8), _mm_slli_si128(next, 8));
}

/*!
 * Loads the next pixels for 2 channels into the high-half of 2 registers whilst
 * shifting the high half into the low half.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in pixels to load from.
 * \param pels     The pixels to modify and load into.
 */
static inline void horizontalGetNextPelsU8NV12(const uint8_t* in, int32_t offset, __m128i pels[2])
{
    const __m128i loaded = _mm_loadu_si128((const __m128i*)&in[offset << 1]);
    const __m128i shuffled = _mm_shuffle_epi8(loaded, *(const __m128i*)kDeinterleaveControl);

    pels[0] = _mm_unpackhi_epi64(pels[0], _mm_slli_si128(shuffled, 8));
    pels[1] = _mm_unpackhi_epi64(pels[1], shuffled);
}

/*!
 * Load 2 channels and deinterleave into pels and then convert from u8 into s16.
 *
 * \param in       The input row to load from.
 * \param offset   The offset in "elements" to load from.
 * \param pels    The pels to load into.
 */
static inline void loadDeinterleavePelsU8AsI16(const uint8_t* in, int32_t offset, __m128i pels[2])
{
    const __m128i loaded = _mm_loadu_si128((const __m128i*)&in[offset << 1]);
    const __m128i shuffled = _mm_shuffle_epi8(loaded, *(const __m128i*)kDeinterleaveControl);

    pels[0] = _mm_cvtepu8_epi16(shuffled);
    pels[1] = _mm_cvtepu8_epi16(_mm_srli_si128(shuffled, 8));
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
 * \param kernelLength   The length of both kernel_fwd and kernel_rev.
 */
static inline void horizontalConvolveU8(__m128i pels, __m128i result[2],
                                        const __m128i kernelFwd[UCInterleavedStore],
                                        const __m128i kernelRev[UCInterleavedStore], uint32_t kernelLength)
{
    const uint32_t loopCount = kernelLength >> 1;

    __m128i tap;
    __m128i values[4] = {
        _mm_setzero_si128(),
        _mm_setzero_si128(),
        _mm_setzero_si128(),
        _mm_setzero_si128(),
    };

    assert(kernelLength <= 8);

    /* The convolution is off-pixel, therefore calculate initial reverse then
     * load in next pixels and proceed. */
    for (uint32_t i = 0; i < loopCount; i++) {
        /* Reverse (even pixels) */
        tap = _mm_madd_epi16(kernelRev[i], _mm_cvtepu8_epi16(pels));
        values[0] = _mm_add_epi32(values[0], tap);

        pels = _mm_srli_si128(pels, 1);

        /* Forward (even pixels) */
        tap = _mm_madd_epi16(kernelFwd[i], _mm_cvtepu8_epi16(pels));
        values[1] = _mm_add_epi32(values[1], tap);

        /* Reverse (odd pixels) */
        tap = _mm_madd_epi16(kernelRev[i], _mm_cvtepu8_epi16(pels));
        values[2] = _mm_add_epi32(values[2], tap);

        pels = _mm_srli_si128(pels, 1);

        /* Forward (odd pixels) */
        tap = _mm_madd_epi16(kernelFwd[i], _mm_cvtepu8_epi16(pels));
        values[3] = _mm_add_epi32(values[3], tap);
    }

    /* Scale back to 8 bits */
    tap = _mm_set1_epi32(UCInverseShiftRounding);

    for (uint32_t i = 0; i < 4; i++) {
        values[i] = _mm_srai_epi32(_mm_add_epi32(values[i], tap), UCInverseShift);
    }

    /* Combine fwd and rev  */
    values[0] = _mm_packs_epi32(values[0], values[2]); /* Reverse 0 2 4 6 1 3 5 7 */
    values[1] = _mm_packs_epi32(values[1], values[3]); /* Forward 0 2 4 6 1 3 5 7 */

    /* Interleave */
    values[2] = _mm_unpacklo_epi16(values[0], values[1]); /* 0 0 2 2 4 4 6 6 */
    values[3] = _mm_unpackhi_epi16(values[0], values[1]); /* 1 1 3 3 5 5 7 7 */

    result[0] = _mm_unpacklo_epi32(values[2], values[3]); /* 0 0 1 1 2 2 3 3 */
    result[1] = _mm_unpackhi_epi32(values[2], values[3]); /* 4 4 5 5 6 6 7 7 */
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
 * \param kernelLength   The length of both kernel_fwd and kernel_rev.
 */
static inline void horizontalConvolveN16(__m128i pels[2], __m128i result[2],
                                         const __m128i kernelFwd[UCInterleavedStore],
                                         const __m128i kernelRev[UCInterleavedStore], uint32_t kernelLength)
{
    const uint32_t loopCount = kernelLength >> 1;
    const uint32_t shiftLoop = 4 - loopCount;

    /* see saturateS15 for choice of min/max */
    const __m128i minV = _mm_set1_epi16(-16384);
    const __m128i maxV = _mm_set1_epi16(16383);

    __m128i tap;
    __m128i values[4] = {_mm_setzero_si128(), _mm_setzero_si128(), _mm_setzero_si128(),
                         _mm_setzero_si128()};

    assert(kernelLength <= 8);

    /* The convolution is off-pixel, therefore calculate initial reverse then
     * load in next pixels and proceed. */
    for (uint32_t i = 0; i < loopCount; i++) {
        /* Reverse (even pixels) */
        tap = _mm_madd_epi16(kernelRev[i], pels[0]);
        values[0] = _mm_add_epi32(values[0], tap);

        pels[0] = _mm_alignr_epi8(pels[1], pels[0], 2);
        pels[1] = _mm_srli_si128(pels[1], 2);

        /* Forward (even pixels) */
        tap = _mm_madd_epi16(kernelFwd[i], pels[0]);
        values[1] = _mm_add_epi32(values[1], tap);

        /* Reverse (odd pixels) */
        tap = _mm_madd_epi16(kernelRev[i], pels[0]);
        values[2] = _mm_add_epi32(values[2], tap);

        pels[0] = _mm_alignr_epi8(pels[1], pels[0], 2);
        pels[1] = _mm_srli_si128(pels[1], 2);

        /* Forward (odd pixels) */
        tap = _mm_madd_epi16(kernelFwd[i], pels[0]);
        values[3] = _mm_add_epi32(values[3], tap);
    }

    /* Must shuffle the remaining pixels in the high register down to the low register,
     * this is because pels is passed in by reference meaning the above shifts have an
     * impact on the next getPels call.
     * A copy could have been taken, but it was measured to be ~10% slower. */
    for (uint32_t i = 0; i < shiftLoop; ++i) {
        pels[0] = _mm_alignr_epi8(pels[1], pels[0], 4);
        pels[1] = _mm_srli_si128(pels[1], 4);
    }

    /* Shift back to 16 bits */
    tap = _mm_set1_epi32(UCInverseShiftRounding);
    for (uint32_t i = 0; i < 4; i++) {
        values[i] = _mm_srai_epi32(_mm_add_epi32(values[i], tap), UCInverseShift);
    }

    /* Combine fwd and rev  */
    values[0] = _mm_packs_epi32(values[0], values[2]); /* Reverse 0 2 4 6 1 3 5 7 */
    values[1] = _mm_packs_epi32(values[1], values[3]); /* Forward 0 2 4 6 1 3 5 7 */

    /* Interleave */
    values[2] = _mm_unpacklo_epi16(values[0], values[1]); /* 0 0 2 2 4 4 6 6 */
    values[3] = _mm_unpackhi_epi16(values[0], values[1]); /* 1 1 3 3 5 5 7 7 */

    result[0] = _mm_unpacklo_epi32(values[2], values[3]); /* 0 0 1 1 2 2 3 3 */
    result[1] = _mm_unpackhi_epi32(values[2], values[3]); /* 4 4 5 5 6 6 7 7 */

    /* Saturate to + / -2 ^ 14 */
    result[0] = _mm_max_epi16(_mm_min_epi16(result[0], maxV), minV);
    result[1] = _mm_max_epi16(_mm_min_epi16(result[1], maxV), minV);
}

/*!
 * Apply 1D predicted-average to values using base for a single row.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to.
 */
static inline void applyPA1D(__m128i base, __m128i values[2])
{
    /* shuffle avg such that we end up with 2 variables - [0 0 1 1 2 2 3 3], [4 4 5 5 6 6 7 7] */
    VN_ALIGN(static const uint8_t kAverageControl[2][16], 16) = {
        {0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07},
        {0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f}};

    /* avg = base - ((pel_even + pel_odd + 1) >> 1) */
    const __m128i sum = _mm_add_epi16(_mm_hadd_epi16(values[0], values[1]), _mm_set1_epi16(1));
    const __m128i avg = _mm_sub_epi16(base, _mm_srai_epi16(sum, 1));
    const __m128i avg0 = _mm_shuffle_epi8(avg, *(const __m128i*)kAverageControl[0]);
    const __m128i avg1 = _mm_shuffle_epi8(avg, *(const __m128i*)kAverageControl[1]);

    values[0] = _mm_add_epi16(values[0], avg0);
    values[1] = _mm_add_epi16(values[1], avg1);
}

/*!
 * Apply 1D predicted-average to values using base for a single row.
 *
 * See `horizontal_apply_pa_2d_precision` for more detail on this specialization.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to.
 */
static inline void applyPA1DPrecision(__m128i base, __m128i values[2])
{
    /* shuffle avg such that we end up with 2 variables - [0 0 1 1 2 2 3 3], [4 4 5 5 6 6 7 7] */
    VN_ALIGN(static const uint8_t kAverageControl[2][16], 16) = {
        {0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07},
        {0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f}};

    __m128i tmp[2];

    /* Calculate horizontal average. */
    tmp[0] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[0]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[0], 8)));
    tmp[1] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[1]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[1], 8)));

    tmp[0] = _mm_srai_epi32(_mm_add_epi32(tmp[0], _mm_set1_epi32(1)), 1);
    tmp[1] = _mm_srai_epi32(_mm_add_epi32(tmp[1], _mm_set1_epi32(1)), 1);

    /* Pack back down and calculate avg. */
    tmp[0] = _mm_packs_epi32(tmp[0], tmp[1]);
    tmp[0] = _mm_sub_epi16(base, tmp[0]);

    const __m128i avg0 = _mm_shuffle_epi8(tmp[0], *(const __m128i*)kAverageControl[0]);
    const __m128i avg1 = _mm_shuffle_epi8(tmp[0], *(const __m128i*)kAverageControl[1]);

    values[0] = _mm_adds_epi16(values[0], avg0);
    values[1] = _mm_adds_epi16(values[1], avg1);
}

/*!
 * Apply 2D predicted-average to values using base, this requires 2 upscaled rows.
 *
 * \param base     The base pixels for the PA calculation.
 * \param values   The upscaled pixels to apply PA to for 2 rows.
 */
static inline void applyPA2DSpeed(__m128i base, __m128i values[2][2])
{
    VN_ALIGN(static const uint8_t kAverageControl[2][16], 16) = {
        {0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07},
        {0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f}};

    /* avg = base - ((row0_pel_even + row0_pel_odd + row1_pel_even + row1_pel_odd + 2) >> 2) */
    const __m128i sum = _mm_add_epi16(_mm_add_epi16(_mm_hadd_epi16(values[0][0], values[0][1]),
                                                    _mm_hadd_epi16(values[1][0], values[1][1])),
                                      _mm_set1_epi16(2));
    const __m128i avg = _mm_sub_epi16(base, _mm_srai_epi16(sum, 2));
    const __m128i avg0 = _mm_shuffle_epi8(avg, *(const __m128i*)kAverageControl[0]);
    const __m128i avg1 = _mm_shuffle_epi8(avg, *(const __m128i*)kAverageControl[1]);

    values[0][0] = _mm_add_epi16(values[0][0], avg0);
    values[0][1] = _mm_add_epi16(values[0][1], avg1);
    values[1][0] = _mm_add_epi16(values[1][0], avg0);
    values[1][1] = _mm_add_epi16(values[1][1], avg1);
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
static inline void applyPA2DPrecision(__m128i base, __m128i values[2][2])
{
    VN_ALIGN(static const uint8_t kAverageControl[2][16], 16) = {
        {0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07},
        {0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f}};

    __m128i tmp[4];
    __m128i avg0;
    __m128i avg1;

    /* For S16 there are not enough headroom bits for the average part of the PA
     * calculation so the math must be promoted to 32-bit. */
    tmp[0] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[0][0]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[0][0], 8)));
    tmp[1] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[0][1]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[0][1], 8)));
    tmp[2] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[1][0]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[1][0], 8)));
    tmp[3] = _mm_hadd_epi32(_mm_cvtepi16_epi32(values[1][1]),
                            _mm_cvtepi16_epi32(_mm_srli_si128(values[1][1], 8)));

    tmp[0] = _mm_add_epi32(tmp[0], tmp[2]);
    tmp[1] = _mm_add_epi32(tmp[1], tmp[3]);

    tmp[0] = _mm_srai_epi32(_mm_add_epi32(tmp[0], _mm_set1_epi32(2)), 2);
    tmp[1] = _mm_srai_epi32(_mm_add_epi32(tmp[1], _mm_set1_epi32(2)), 2);

    /* The average result will never overflow 16-bit, so it is safe to pack back
     * from 32-bit now and perform the rest of the operations in 16-bit. */
    tmp[0] = _mm_packs_epi32(tmp[0], tmp[1]);
    tmp[0] = _mm_sub_epi16(base, tmp[0]);

    avg0 = _mm_shuffle_epi8(tmp[0], *(const __m128i*)kAverageControl[0]);
    avg1 = _mm_shuffle_epi8(tmp[0], *(const __m128i*)kAverageControl[1]);

    values[0][0] = _mm_adds_epi16(values[0][0], avg0);
    values[0][1] = _mm_adds_epi16(values[0][1], avg1);
    values[1][0] = _mm_adds_epi16(values[1][0], avg0);
    values[1][1] = _mm_adds_epi16(values[1][1], avg1);
}

/*!
 * Apply dithering to values using supplied host buffer pointer containing randomized
 * values.
 *
 * This function loads in values from the dither buffer, then moves the buffer pointer
 * forward for the next invocation of this function.
 *
 * \param values         The values to apply dithering to.
 * \param ditherBuffer   A double pointer to the dither buffer pointer.
 */
static inline void applyDither(__m128i values[2], const int8_t** ditherBuffer)
{
    __m128i dither = _mm_loadu_si128((const __m128i*)*ditherBuffer);
    *ditherBuffer += 16;

    values[0] = _mm_adds_epi16(values[0], _mm_cvtepi8_epi16(dither));
    values[1] = _mm_adds_epi16(values[1], _mm_cvtepi8_epi16(_mm_srli_si128(dither, 8)));
}

/*!
 * Apply dithering to values using supplied host buffer pointer containing randomized
 * values for precision.
 *
 * This function loads in values from the dither buffer, when adding the dither values,
 * it makes the bit shifting to match the input bit depth, then moves the buffer pointer
 * forward for the next invocation of this function.
 *
 * \param values         The values to apply dithering to.
 * \param ditherBuffer   A double pointer to the dither buffer pointer.
 * \param shift          Places to be bit shifted to the left.
 */
static inline void applyDitherS16(__m128i values[2], const int8_t** ditherBuffer, int8_t shift)
{
    __m128i dither = _mm_loadu_si128((const __m128i*)*ditherBuffer);
    *ditherBuffer += 16;

    values[0] = _mm_adds_epi16(values[0], _mm_slli_epi16(_mm_cvtepi8_epi16(dither), shift));
    values[1] =
        _mm_adds_epi16(values[1], _mm_slli_epi16(_mm_cvtepi8_epi16(_mm_srli_si128(dither, 8)), shift));
}

/*! \brief U8 Planar horizontal upscaling of 2 rows. */
static void horizontalU8PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                  const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                  uint32_t xEnd, const Kernel_t* kernel)
{
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const uint32_t kernelLength = kernel->length;
    __m128i pels[2];
    __m128i values[2][2];
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;

    UpscaleHorizontalCoords_t coords = {0};

    /* This implementation assumes kernel is even in length. This is because the
     * implementation revolves around using _mm_madd_epi16 for the convolution as
     * 32-bits of storage are required for the calculation. */
    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse kernels as interleaved pairs respectively. */
    for (int32_t x = 0; x < (int32_t)(kernelLength >> 1); ++x) {
        const int32_t fwdIdx = x * 2;
        const int32_t revIdx = (int32_t)kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[x] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[x] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

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

        horizontalConvolveU8(pels[0], values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveU8(pels[1], values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            /* @note: The base pels are already loaded, they are src - they are however
             *        offset by -kernelLength / 2. The SSE shift intrinsics require the
             *        shift amount to be a constant compile time expression. */
            const __m128i basePels0 = _mm_loadl_epi64((const __m128i*)&base[0][x]);
            const __m128i basePels1 = _mm_loadl_epi64((const __m128i*)&base[1][x]);

            applyPA1D(_mm_cvtepu8_epi16(basePels0), values[0]);
            applyPA1D(_mm_cvtepu8_epi16(basePels1), values[1]);
        } else if (paEnabled) {
            const __m128i basePels = _mm_loadl_epi64((const __m128i*)&base[0][x]);
            applyPA2DSpeed(_mm_cvtepu8_epi16(basePels), values);
        }

        if (ditherBuffer) {
            applyDither(values[0], &ditherBuffer);
            applyDither(values[1], &ditherBuffer);
        }

        /* Unsigned saturated pack back to 16 uint8_t and write them out. */
        values[0][0] = _mm_packus_epi16(values[0][0], values[0][1]);
        values[1][0] = _mm_packus_epi16(values[1][0], values[1][1]);

        _mm_storeu_si128((__m128i*)&out[0][storeOffset], values[0][0]);
        _mm_storeu_si128((__m128i*)&out[1][storeOffset], values[1][0]);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 1);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8Planar(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief S16 Planar horizontal upscaling of 2 rows. */
static void horizontalS16PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                   const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                   uint32_t xEnd, const Kernel_t* kernel)
{
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const uint32_t kernelLength = kernel->length;
    __m128i pels[2][2];
    __m128i values[2][2];
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    int8_t shift = 0;
    int16_t* out16[2] = {(int16_t*)out[0], (int16_t*)out[1]};
    const int16_t* base16[2] = {(const int16_t*)base[0], (const int16_t*)base[1]};
    UpscaleHorizontalCoords_t coords = {0};

    /* This implementation assumes kernel is even in length. This is because the
     * implementation revolves around using _mm_madd_epi16 for the convolution as
     * 32-bits of storage are required for the calculation. */
    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse kernels as interleaved pairs respectively. */
    for (int32_t x = 0; x < (int32_t)(kernelLength >> 1); ++x) {
        const int32_t fwdIdx = x * 2;
        const int32_t revIdx = (int32_t)kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[x] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[x] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignment, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalS16Planar(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0][0] = horizontalGetPelsN16(in[0], loadOffset);
    pels[1][0] = horizontalGetPelsN16(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 1);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
        shift = ditherGetShiftS16(dither);
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        pels[0][1] = horizontalGetPelsN16(in[0], loadOffset);
        pels[1][1] = horizontalGetPelsN16(in[1], loadOffset);

        horizontalConvolveN16(pels[0], values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveN16(pels[1], values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            /* @note: The base pels are already loaded, they are src - they are however
             *        offset by -kernelLength / 2. The SSE shift intrinsics require the
             *        shift amount to be a constant compile time expression. */
            const __m128i basePels0 = _mm_loadu_si128((const __m128i*)&base16[0][x]);
            const __m128i basePels1 = _mm_loadu_si128((const __m128i*)&base16[1][x]);

            applyPA1DPrecision(basePels0, values[0]);
            applyPA1DPrecision(basePels1, values[1]);
        } else if (paEnabled) {
            const __m128i basePels = _mm_loadu_si128((const __m128i*)&base16[0][x]);
            applyPA2DPrecision(basePels, values);
        }

        if (ditherBuffer) {
            applyDitherS16(values[0], &ditherBuffer, shift);
            applyDitherS16(values[1], &ditherBuffer, shift);
        }

        /* Write out (note that dither and PA used saturating add, so we're safely within S16). */
        _mm_storeu_si128((__m128i*)&out16[0][storeOffset], values[0][0]);
        _mm_storeu_si128((__m128i*)&out16[0][storeOffset + 8], values[0][1]);
        _mm_storeu_si128((__m128i*)&out16[1][storeOffset], values[1][0]);
        _mm_storeu_si128((__m128i*)&out16[1][storeOffset + 8], values[1][1]);

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 1);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalS16Planar(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*! \brief U16 Planar horizontal upscaling of 2 rows. */
static inline void horizontalU16PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                          const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                          uint32_t xEnd, const Kernel_t* kernel, int16_t maxValue,
                                          bool is14Bit)
{
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const uint32_t kernelLength = kernel->length;
    __m128i pels[2][2];
    __m128i values[2][2];
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const __m128i minV = _mm_set1_epi16(0);
    const __m128i maxV = _mm_set1_epi16(maxValue);
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint16_t* out16[2] = {(uint16_t*)out[0], (uint16_t*)out[1]};
    const uint16_t* base16[2] = {(const uint16_t*)base[0], (const uint16_t*)base[1]};

    UpscaleHorizontalCoords_t coords = {0};

    /* This implementation assumes kernel is even in length. This is because the
     * implementation revolves around using _mm_madd_epi16 for the convolution as
     * 32-bits of storage are required for the calculation. */
    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse kernels as interleaved pairs respectively. */
    for (int32_t x = 0; x < (int32_t)(kernelLength >> 1); ++x) {
        const int32_t fwdIdx = x * 2;
        const int32_t revIdx = (int32_t)kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[x] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[x] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignment, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalUNPlanar(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel, maxValue);
    }

    /* Prime I/O */
    int32_t loadOffset = (int32_t)(coords.start - (kernelLength >> 1));
    pels[0][0] = horizontalGetPelsN16(in[0], loadOffset);
    pels[1][0] = horizontalGetPelsN16(in[1], loadOffset);
    loadOffset += UCHoriStepping;
    int32_t storeOffset = (int32_t)(coords.start << 1);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(4 * (xEnd - xStart), 16));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        pels[0][1] = horizontalGetPelsN16(in[0], loadOffset);
        pels[1][1] = horizontalGetPelsN16(in[1], loadOffset);

        horizontalConvolveN16(pels[0], values[0], kernelFwd, kernelRev, kernelLength);
        horizontalConvolveN16(pels[1], values[1], kernelFwd, kernelRev, kernelLength);

        if (paEnabled1D) {
            /* @note: The base pels are already loaded, they are src - they are however
             *        offset by -kernelLength / 2. The SSE shift intrinsics require the
             *        shift amount to be a constant compile time expression. */
            const __m128i basePels0 = _mm_loadu_si128((const __m128i*)&base16[0][x]);
            const __m128i basePels1 = _mm_loadu_si128((const __m128i*)&base16[1][x]);

            applyPA1D(basePels0, values[0]);
            applyPA1D(basePels1, values[1]);
        } else if (paEnabled) {
            const __m128i basePels = _mm_loadu_si128((const __m128i*)&base16[0][x]);

            if (is14Bit) {
                applyPA2DPrecision(basePels, values);
            } else {
                applyPA2DSpeed(basePels, values);
            }
        }

        if (ditherBuffer) {
            applyDither(values[0], &ditherBuffer);
            applyDither(values[1], &ditherBuffer);
        }

        /* Saturate to unsigned N-bit and write out. */
        _mm_storeu_si128((__m128i*)&out16[0][storeOffset],
                         _mm_min_epu16(_mm_max_epi16(values[0][0], minV), maxV));
        _mm_storeu_si128((__m128i*)&out16[0][storeOffset + 8],
                         _mm_min_epu16(_mm_max_epi16(values[0][1], minV), maxV));

        _mm_storeu_si128((__m128i*)&out16[1][storeOffset],
                         _mm_min_epu16(_mm_max_epi16(values[1][0], minV), maxV));
        _mm_storeu_si128((__m128i*)&out16[1][storeOffset + 8],
                         _mm_min_epu16(_mm_max_epi16(values[1][1], minV), maxV));

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
static void horizontalU10PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                   const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                   uint32_t xEnd, const Kernel_t* kernel)
{
    horizontalU16PlanarSSE(dither, in, out, base, width, xStart, xEnd, kernel, 1023, false);
}

/*! \brief U12 Planar horizontal upscaling of 2 rows. */
static void horizontalU12PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                   const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                   uint32_t xEnd, const Kernel_t* kernel)
{
    horizontalU16PlanarSSE(dither, in, out, base, width, xStart, xEnd, kernel, 4095, false);
}

/*! \brief U14 Planar horizontal upscaling of 2 rows. */
static void horizontalU14PlanarSSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                   const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                   uint32_t xEnd, const Kernel_t* kernel)
{
    horizontalU16PlanarSSE(dither, in, out, base, width, xStart, xEnd, kernel, 16383, true);
}

/*! \brief NV12 horizontal upscaling of 2 rows. */
static void horizontalU8NV12SSE(Dither_t dither, const uint8_t* in[2], uint8_t* out[2],
                                const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                uint32_t xEnd, const Kernel_t* kernel)
{
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const uint32_t kernelLength = kernel->length;
    int32_t loadOffset = 0;
    int32_t storeOffset = 0;
    __m128i pels[2][2]; /* Indexed by [row][channel] */
    __m128i result[2][2];
    __m128i values[2][2];
    __m128i basePels[2][2];
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int8_t* ditherBuffer = NULL;
    uint32_t channelIdx = 0;

    UpscaleHorizontalCoords_t coords = {0};

    /* This implementation assumes kernel is even in length. This is because the
     * implementation revolves around using _mm_madd_epi16 for the convolution as
     * 32-bits of storage are required for the calculation. */
    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse kernels as interleaved pairs respectively. */
    for (int32_t x = 0; x < (int32_t)(kernelLength >> 1); ++x) {
        const int32_t fwdIdx = x * 2;
        const int32_t revIdx = (int32_t)kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[x] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[x] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Determine edge-cases that should be run in non-SIMD codepath. */
    upscaleHorizontalGetCoords(width, xStart, xEnd, kernelLength, UCHoriLoadAlignmentNV12, &coords);

    /* Run left edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsLeftValid(&coords)) {
        horizontalU8NV12(dither, in, out, base, width, coords.leftStart, coords.leftEnd, kernel);
    }

    /* Prime I/O */
    loadOffset = (int32_t)(coords.start - (kernelLength >> 1));

    horizontalGetPelsU8NV12(in[0], loadOffset, pels[0]);
    horizontalGetPelsU8NV12(in[1], loadOffset, pels[1]);

    loadOffset += UCHoriStepping;
    storeOffset = (int32_t)(coords.start << 2);

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows. */
    if (dither != NULL) {
        ditherBuffer = ditherGetBuffer(dither, alignU32(8 * (xEnd - xStart), 32));
    }

    /* Run middle SIMD loop */
    for (uint32_t x = coords.start; x < coords.end; x += UCHoriStepping) {
        horizontalGetNextPelsU8NV12(in[0], loadOffset, pels[0]);
        horizontalGetNextPelsU8NV12(in[1], loadOffset, pels[1]);

        if (paEnabled1D) {
            loadDeinterleavePelsU8AsI16(base[0], (int32_t)x, basePels[0]);
            loadDeinterleavePelsU8AsI16(base[1], (int32_t)x, basePels[1]);
        } else if (paEnabled) {
            loadDeinterleavePelsU8AsI16(base[0], (int32_t)x, basePels[0]);
        }

        for (channelIdx = 0; channelIdx < 2; ++channelIdx) {
            horizontalConvolveU8(pels[0][channelIdx], values[0], kernelFwd, kernelRev, kernelLength);
            horizontalConvolveU8(pels[1][channelIdx], values[1], kernelFwd, kernelRev, kernelLength);

            if (paEnabled1D) {
                applyPA1D(basePels[0][channelIdx], values[0]);
                applyPA1D(basePels[1][channelIdx], values[1]);
            } else if (paEnabled) {
                applyPA2DSpeed(basePels[0][channelIdx], values);
            }

            if (ditherBuffer) {
                applyDither(values[0], &ditherBuffer);
                applyDither(values[1], &ditherBuffer);
            }

            /* Unsigned saturated pack back to 16 uint8_t and write them out. */
            result[0][channelIdx] = _mm_packus_epi16(values[0][0], values[0][1]);
            result[1][channelIdx] = _mm_packus_epi16(values[1][0], values[1][1]);
        }

        /* Interleave results and write out. */
        _mm_storeu_si128((__m128i*)&out[0][storeOffset], _mm_unpacklo_epi8(result[0][0], result[0][1]));
        _mm_storeu_si128((__m128i*)&out[0][storeOffset + 16],
                         _mm_unpackhi_epi8(result[0][0], result[0][1]));
        _mm_storeu_si128((__m128i*)&out[1][storeOffset], _mm_unpacklo_epi8(result[1][0], result[1][1]));
        _mm_storeu_si128((__m128i*)&out[1][storeOffset + 16],
                         _mm_unpackhi_epi8(result[1][0], result[1][1]));

        loadOffset += UCHoriStepping;
        storeOffset += (UCHoriStepping << 2);
    }

    /* Run right edge non-SIMD loop */
    if (upscaleHorizontalCoordsIsRightValid(&coords)) {
        horizontalU8NV12(dither, in, out, base, width, coords.rightStart, coords.rightEnd, kernel);
    }
}

/*------------------------------------------------------------------------------*/

/*!
 * Loads kernel-length rows of initial upscale input data ensuring that edge extension
 * is performed. This function interleaves loaded row pairs so that the madd intrinsic
 * can be used for high precision upscaling.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row offset to start loading from.
 * \param  count    The number of rows to load in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetPelsU8(const uint8_t* in, uint32_t height, uint32_t stride, int32_t offset,
                                     int32_t count, __m128i pels[UCInterleavedStore][UCVertGroupSize])
{
    __m128i load0;
    __m128i load1;

    /* Convert to 16-bit and interleave row pairs. */
    for (int32_t i = 0; i < (count >> 1); i++) {
        const size_t row0 = clampS32(offset + (i * 2), 0, (int32_t)height - 1);
        const size_t row1 = clampS32(offset + (i * 2) + 1, 0, (int32_t)height - 1);

        load0 = _mm_loadu_si128((const __m128i*)&in[row0 * stride]);
        load1 = _mm_loadu_si128((const __m128i*)&in[row1 * stride]);

        pels[i][0] = _mm_unpacklo_epi16(_mm_cvtepu8_epi16(load0), _mm_cvtepu8_epi16(load1));
        pels[i][1] = _mm_unpackhi_epi16(_mm_cvtepu8_epi16(load0), _mm_cvtepu8_epi16(load1));

        load0 = _mm_srli_si128(load0, 8);
        load1 = _mm_srli_si128(load1, 8);

        pels[i][2] = _mm_unpacklo_epi16(_mm_cvtepu8_epi16(load0), _mm_cvtepu8_epi16(load1));
        pels[i][3] = _mm_unpackhi_epi16(_mm_cvtepu8_epi16(load0), _mm_cvtepu8_epi16(load1));
    }
}

static inline void verticalGetPelsN16(const uint8_t* in, uint32_t height, uint32_t stride, int32_t offset,
                                      int32_t count, __m128i pels[UCInterleavedStore][UCVertGroupSize])
{
    const int16_t* in16 = (const int16_t*)in;
    __m128i load0;
    __m128i load1;

    /* Load as 16-bit and interleave row pairs. */
    for (int32_t i = 0; i < (count >> 1); i++) {
        const size_t row0 = (size_t)clampS32(offset + (i * 2), 0, (int32_t)height - 1);
        const size_t row1 = (size_t)clampS32(offset + (i * 2) + 1, 0, (int32_t)height - 1);

        /* First 8 elements */
        load0 = _mm_loadu_si128((const __m128i*)&in16[row0 * stride]);
        load1 = _mm_loadu_si128((const __m128i*)&in16[row1 * stride]);

        pels[i][0] = _mm_unpacklo_epi16(load0, load1);
        pels[i][1] = _mm_unpackhi_epi16(load0, load1);

        /* Next 8 elements */
        load0 = _mm_loadu_si128((const __m128i*)&in16[(row0 * stride) + 8]);
        load1 = _mm_loadu_si128((const __m128i*)&in16[(row1 * stride) + 8]);

        pels[i][2] = _mm_unpacklo_epi16(load0, load1);
        pels[i][3] = _mm_unpackhi_epi16(load0, load1);
    }
}

/*!
 * Loads the next row of upscale input data by shuffling the pels down 1, and loading
 * next row into the last entry. This function ensures that edge extension is performed.
 *
 * This function respects the interleaving configuration of the loaded pels outlined
 * in vertical_get_pels_u8.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row to load from.
 * \param  count    The number of rows loaded in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetNextPelsU8(const uint8_t* in, uint32_t height, uint32_t stride,
                                         int32_t offset, int32_t count,
                                         __m128i pels[UCInterleavedStore][UCVertGroupSize])
{
    const int32_t loopCount = (count >> 1) - 1;
    const int32_t index = offset + count - 1;
    const size_t row = (size_t)minS32(index, (int32_t)height - 1);
    __m128i load;

    assert(index > 0);

    /* Load up the next row */
    load = _mm_loadu_si128((const __m128i*)&in[row * stride]);

    /* Shuffle rows out to make space for this new row. */
    int32_t loopIndex = 0;
    for (loopIndex = 0; loopIndex < loopCount; loopIndex++) {
        for (int32_t j = 0; j < UCVertGroupSize; ++j) {
            /* Shift curr up 2 bytes: [A0,B0,A1,B1] -> [B0,A1,B1,A2] */
            const __m128i current = _mm_srli_si128(pels[loopIndex][j], 2);

            /* Shift next down 2 bytes: [C0,D0,C1,D1] -> [XX,C0,D0,C1] */
            const __m128i next = _mm_slli_si128(pels[loopIndex + 1][j], 2);

            /* Combine shifted curr & next to form [B0,C0,B1,C1] */
            pels[loopIndex][j] = _mm_blend_epi16(current, next, 0xAA);
        }
    }

    /* Shuffle last row up to odd lanes to make space for "load" in even
     * lanes. */
    for (int32_t j = 0; j < UCVertGroupSize; ++j) {
        pels[loopIndex][j] = _mm_srli_si128(pels[loopIndex][j], 2);
    }

    /* Interleave low half of load. */
    pels[loopIndex][0] = _mm_blend_epi16(
        pels[loopIndex][0], _mm_unpacklo_epi16(_mm_setzero_si128(), _mm_cvtepu8_epi16(load)), 0xAA);
    pels[loopIndex][1] = _mm_blend_epi16(
        pels[loopIndex][1], _mm_unpackhi_epi16(_mm_setzero_si128(), _mm_cvtepu8_epi16(load)), 0xAA);

    /* Interleave high half of load. */
    load = _mm_srli_si128(load, 8);

    pels[loopIndex][2] = _mm_blend_epi16(
        pels[loopIndex][2], _mm_unpacklo_epi16(_mm_setzero_si128(), _mm_cvtepu8_epi16(load)), 0xAA);
    pels[loopIndex][3] = _mm_blend_epi16(
        pels[loopIndex][3], _mm_unpackhi_epi16(_mm_setzero_si128(), _mm_cvtepu8_epi16(load)), 0xAA);
}

/*!
 * Loads the next row of upscale input data by shuffling the pels down 1, and loading
 * next row into the last entry. This function ensures that edge extension is performed.
 *
 * This function respects the interleaving configuration of the loaded pels outlined
 * in vertical_get_pels_u8.
 *
 * \param  in       The input source surface to load from.
 * \param  height   The height of the input surface being loaded.
 * \param  stride   The stride of the input surface being loaded.
 * \param  offset   The row to load from.
 * \param  count    The number of rows loaded in.
 * \param pels     The destination to load the pixels into.
 */
static inline void verticalGetNextPelsN16(const uint8_t* in, uint32_t height, uint32_t stride,
                                          int32_t offset, int32_t count,
                                          __m128i pels[UCInterleavedStore][UCVertGroupSize])
{
    const int16_t* in16 = (const int16_t*)in;
    const int32_t loopCount = (count >> 1) - 1;
    const int32_t index = offset + count - 1;
    const size_t row = (size_t)minS32(index, (int32_t)height - 1);
    __m128i load;

    assert(index > 0);

    /* Load up first 8 elements */
    load = _mm_loadu_si128((const __m128i*)&in16[row * stride]);

    int32_t loopIndex = 0;

    /* Shuffle rows out to make space for this new row. */
    for (; loopIndex < loopCount; loopIndex++) {
        for (int32_t j = 0; j < UCVertGroupSize; ++j) {
            /* Shift curr up 2 bytes: [A0,B0,A1,B1] -> [B0,A1,B1,A2] */
            const __m128i current = _mm_srli_si128(pels[loopIndex][j], 2);

            /* Shift next down 2 bytes: [C0,D0,C1,D1] -> [XX,C0,D0,C1] */
            const __m128i next = _mm_slli_si128(pels[loopIndex + 1][j], 2);

            /* Combine shifted curr & next to form [B0,C0,B1,C1] */
            pels[loopIndex][j] = _mm_blend_epi16(current, next, 0xAA);
        }
    }

    /* Shuffle last row up to odd lanes to make space for "load" in even
     * lanes. */
    for (int32_t j = 0; j < UCVertGroupSize; ++j) {
        pels[loopIndex][j] = _mm_srli_si128(pels[loopIndex][j], 2);
    }

    /* Interleave first 8 elements. */
    pels[loopIndex][0] =
        _mm_blend_epi16(pels[loopIndex][0], _mm_unpacklo_epi16(_mm_setzero_si128(), load), 0xAA);
    pels[loopIndex][1] =
        _mm_blend_epi16(pels[loopIndex][1], _mm_unpackhi_epi16(_mm_setzero_si128(), load), 0xAA);

    /* Interleave next 8 elements. */
    load = _mm_loadu_si128((const __m128i*)&in16[(row * stride) + 8]);

    pels[loopIndex][2] =
        _mm_blend_epi16(pels[loopIndex][2], _mm_unpacklo_epi16(_mm_setzero_si128(), load), 0xAA);
    pels[loopIndex][3] =
        _mm_blend_epi16(pels[loopIndex][3], _mm_unpackhi_epi16(_mm_setzero_si128(), load), 0xAA);
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result as signed 16-bit saturated values.
 *
 * This generates 16-pixels worth of output.
 *
 * \param  pels            The pixels to upscale from.
 * \param  kernel          The kernel to upscale with
 * \param  kernelLength   The length of kernel.
 * \param result          Place to store the resultant 16-pixels.
 */
static inline void verticalConvolveS16(__m128i pels[UCInterleavedStore][UCVertGroupSize],
                                       const __m128i kernel[UCInterleavedStore],
                                       int32_t kernelLength, __m128i result[2])
{
    const int32_t loopCount = kernelLength >> 1;

    __m128i tap;
    __m128i values[4] = {_mm_setzero_si128(), _mm_setzero_si128(), _mm_setzero_si128(),
                         _mm_setzero_si128()};

    /* see saturateS15 for choice of min/max */
    const __m128i minV = _mm_set1_epi32(-16384);
    const __m128i maxV = _mm_set1_epi32(16383);

    for (int32_t i = 0; i < loopCount; i++) {
        for (int32_t j = 0; j < UCVertGroupSize; j++) {
            tap = _mm_madd_epi16(pels[i][j], kernel[i]);
            values[j] = _mm_add_epi32(values[j], tap);
        }
    }

    /* Shift back to 16 bits, and clamp to +/-2^14 */
    tap = _mm_set1_epi32(UCInverseShiftRounding);
    for (int32_t j = 0; j < UCVertGroupSize; j++) {
        values[j] = _mm_srai_epi32(_mm_add_epi32(values[j], tap), UCInverseShift);
        values[j] = _mm_min_epi32(_mm_max_epi32(values[j], minV), maxV);
    }

    /* Pack back down to saturated int16_t */
    result[0] = _mm_packs_epi32(values[0], values[1]);
    result[1] = _mm_packs_epi32(values[2], values[3]);
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result as unsigned 8-bit saturated values.
 *
 * This generates 16-pixels worth of output.
 *
 * \param pels            The pixels to upscale from.
 * \param kernel          The kernel to upscale with
 * \param kernelLength   The length of kernel.
 *
 * \return The result of the convolution saturated and packed back to U8.
 */
static inline __m128i verticalConvolveU8(__m128i pels[UCInterleavedStore][UCVertGroupSize],
                                         const __m128i kernel[UCInterleavedStore], int32_t kernelLength)
{
    __m128i result[2];

    /* Run the s16 convolution, then saturate and pack the result to U8. */
    verticalConvolveS16(pels, kernel, kernelLength, result);

    /* Pack back down to saturated u8 values */
    return _mm_packus_epi16(result[0], result[1]);
}

/*!
 * Performs vertical convolution of input pels applying the kernel and returns the
 * result as unsigned 16-bit saturated values.
 *
 * This generates 16-pixels worth of output.
 *
 * \param  pels            The pixels to upscale from.
 * \param  kernel          The kernel to upscale with
 * \param  kernelLength   The length of kernel.
 * \param result          Place to store the resultant 16-pixels.
 */
static inline void verticalConvolveU16(const __m128i pels[UCInterleavedStore][UCVertGroupSize],
                                       const __m128i kernel[UCInterleavedStore],
                                       int32_t kernelLength, __m128i result[2])
{
    const int32_t loopCount = kernelLength >> 1;
    __m128i tap;
    __m128i values[4] = {_mm_setzero_si128(), _mm_setzero_si128(), _mm_setzero_si128(),
                         _mm_setzero_si128()};

    for (int32_t i = 0; i < loopCount; i++) {
        for (int32_t j = 0; j < UCVertGroupSize; j++) {
            tap = _mm_madd_epi16(pels[i][j], kernel[i]);
            values[j] = _mm_add_epi32(values[j], tap);
        }
    }

    /* Scale back */
    tap = _mm_set1_epi32(UCInverseShiftRounding);

    for (int32_t j = 0; j < UCVertGroupSize; j++) {
        values[j] = _mm_srai_epi32(_mm_add_epi32(values[j], tap), UCInverseShift);
    }

    /* Pack back down to saturated uint16_t */
    result[0] = _mm_packus_epi32(values[0], values[1]);
    result[1] = _mm_packus_epi32(values[2], values[3]);
}

/*! \brief Vertical upscaling of 16 columns. */
void verticalU8SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                   uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const int32_t kernelLength = (int32_t)kernel->length;
    const uint32_t outSkip = 2 * outStride;
    uint8_t* out0 = out + ((size_t)y * outSkip);
    uint8_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    __m128i pels[UCInterleavedStore][UCVertGroupSize];

    /* This implementation assumes kernel is even in length. This is because the
     * implementation revolves around using _mm_madd_epi16 for the convolution as
     * 32-bits of storage are required for the calculation. */
    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* The bit-depth of the upscaling kernel requires the multiplication and accumulation
     * be performed in 32-bit. The following steps are performed to support 32-bit
     * calculations whilst minimizing loads and keeping register pressure low.
     *
     *     1. Load kernel into 2 groups of registers (kernel_fwd & kernel_rev), the
     *        coefficients are interleaved in pairs within their group. i.e. For
     *        a 4-tap kernel:
     *           kernel_fwd[0] = {k0, k1, k0, k1, k0, k1, k0, k1};
     *           kernel_fwd[1] = {k2, k3, k2, k3, k2, k3, k2, k3};
     *           kernel_rev[0] = {k3, k2, k3, k2, k3, k2, k3, k2};
     *           kernel_rev[1] = {k1, k0, k1, k0, k1, k0, k1, k0};
     *
     *     2. Load 16 uint8_t pels per source row. i.e For a 4-tap kernel this would be
     *        4 rows of data loaded (handling edge extension)
     *
     *     3. Convert the loaded per-row data into 16-bit and interleave row pairs.
     *        This produces row "groups" of 32 int16_t values. The interleaving is
     *        the same as the kernel interleaving. i.e
     *           pels[0][0] = {r0[0], r1[0], r0[1], r1[1], r0[2], r1[2], r0[3], r1[3]};
     *           pels[1][0] = {r2[0], r3[0], r2[1], r3[1], r2[2], r3[2], r2[3], r4[3]};
     *
     *           pels[X][1,2,3] contains the remaining 12 values for each row.
     *
     *     4. Convolve by performing _mm_madd_epi16(pels[N], kernel_X[N}) for each
     *        loaded row/kernel pair. This instruction multiplies pels by kernel and
     *        performs a horizontal pair-wise addition of the 2 products in 32-bit.
     *        There are no other 32-bit multiplication operations. The result is
     *        accumulated for each row resulting in 4 registers of 4 int32_t values
     *        one for each column which is saturate packed back down to u8 on a
     *        single register.
     *
     *     5. Load next row by shuffling each register up to remove the first row
     *        and insert the new row at the end.
     *        This is performed by interleaving the even lanes from pels[0][N] with
     *        the odd lanes in pels[1][N]. Resulting in:
     *           pels[0][0] = {r1[0], r2[0], r1[1], r2[1], r1[2], r2[2], r1[3], r2[3]};
     *
     *        - For a 2-tap kernel this step is skipped.
     *        - For a 6-tap kernel the the even lanes from pels[1][N] are interleaved
     *          with the odd lanes from pels[2][N].
     *
     *        The last row "group" of pels will have its even lanes interleaved with
     *        the newly loaded values. The valid row index is dependent on the number
     *        of kernel taps. The index into the group is defined for each sized
     *        kernel as such:
     *
     *           2-tap = 0
     *           4-tap = 1
     *           6-tap = 2
     */

    /* Load up forward and reverse interleaved kernels. */
    for (int32_t i = 0; i < (kernelLength >> 1); ++i) {
        const int32_t fwdIdx = i * 2;
        const int32_t revIdx = kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[i] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[i] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Prime interleaved rows. */
    verticalGetPelsU8(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        _mm_storeu_si128((__m128i*)out0, verticalConvolveU8(pels, kernelRev, kernelLength));

        /* Next input due to being off-pixel */
        verticalGetNextPelsU8(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        _mm_storeu_si128((__m128i*)out1, verticalConvolveU8(pels, kernelFwd, kernelLength));

        out0 += outSkip;
        out1 += outSkip;
    }
}

void verticalS16SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                    uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const int32_t kernelLength = (int32_t)kernel->length;
    const uint32_t outSkip = 2 * outStride;
    int16_t* out16 = (int16_t*)out;
    int16_t* out0 = out16 + ((size_t)y * outSkip);
    int16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    __m128i pels[UCInterleavedStore][UCVertGroupSize];
    __m128i result[2];

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse interleaved kernels. */
    for (int32_t i = 0; i < (kernelLength >> 1); ++i) {
        const int32_t fwdIdx = i * 2;
        const int32_t revIdx = kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[i] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[i] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Prime interleaved rows. */
    verticalGetPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        verticalConvolveS16(pels, kernelRev, kernelLength, result);
        _mm_storeu_si128((__m128i*)out0, result[0]);
        _mm_storeu_si128((__m128i*)(out0 + 8), result[1]);

        /* Next input due to being off-pixel */
        verticalGetNextPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        verticalConvolveS16(pels, kernelFwd, kernelLength, result);
        _mm_storeu_si128((__m128i*)out1, result[0]);
        _mm_storeu_si128((__m128i*)(out1 + 8), result[1]);

        out0 += outSkip;
        out1 += outSkip;
    }
}

static void verticalU16SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                           uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel,
                           uint16_t maxValue)
{
    __m128i kernelFwd[UCInterleavedStore];
    __m128i kernelRev[UCInterleavedStore];
    const int16_t* kernelCoeffs = kernel->coeffs[0];
    const int32_t kernelLength = (int32_t)kernel->length;
    const uint32_t outSkip = 2 * outStride;
    uint16_t* out16 = (uint16_t*)out;
    uint16_t* out0 = out16 + ((size_t)y * outSkip);
    uint16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);
    __m128i pels[UCInterleavedStore][UCVertGroupSize];
    __m128i result[2];
    __m128i maxV = _mm_set1_epi16((int16_t)maxValue);

    assert(kernelLength % 2 == 0);
    assert(kernelLength <= UCMaxKernelSize);

    /* Load up forward and reverse interleaved kernels. */
    for (int32_t i = 0; i < (kernelLength >> 1); ++i) {
        const int32_t fwdIdx = i * 2;
        const int32_t revIdx = kernelLength - fwdIdx - 1;

        const int16_t fwd0 = kernelCoeffs[fwdIdx];
        const int16_t fwd1 = kernelCoeffs[fwdIdx + 1];

        const int16_t rev0 = kernelCoeffs[revIdx];
        const int16_t rev1 = kernelCoeffs[revIdx - 1];

        kernelFwd[i] = _mm_set_epi16(fwd1, fwd0, fwd1, fwd0, fwd1, fwd0, fwd1, fwd0);
        kernelRev[i] = _mm_set_epi16(rev1, rev0, rev1, rev0, rev1, rev0, rev1, rev0);
    }

    /* Prime interleaved rows. */
    verticalGetPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
    loadOffset += 1;

    /* Only need to clamp max as the convolve function performs unsigned 16-bit
     * saturation already. */
    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        /* Reverse filter */
        verticalConvolveU16(pels, kernelRev, kernelLength, result);

        _mm_storeu_si128((__m128i*)out0, _mm_min_epu16(result[0], maxV));
        _mm_storeu_si128((__m128i*)(out0 + 8), _mm_min_epu16(result[1], maxV));

        /* Next input due to being off-pixel */
        verticalGetNextPelsN16(in, height, inStride, loadOffset, kernelLength, pels);
        loadOffset += 1;

        /* Forward filter */
        verticalConvolveU16(pels, kernelFwd, kernelLength, result);
        _mm_storeu_si128((__m128i*)out1, _mm_min_epu16(result[0], maxV));
        _mm_storeu_si128((__m128i*)(out1 + 8), _mm_min_epu16(result[1], maxV));

        out0 += outSkip;
        out1 += outSkip;
    }
}

static void verticalU10SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                           uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16SSE(in, inStride, out, outStride, y, rows, height, kernel, 1023);
}

static void verticalU12SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                           uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16SSE(in, inStride, out, outStride, y, rows, height, kernel, 4095);
}

static void verticalU14SSE(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                           uint32_t y, uint32_t rows, uint32_t height, const Kernel_t* kernel)
{
    verticalU16SSE(in, inStride, out, outStride, y, rows, height, kernel, 16383);
}

/*------------------------------------------------------------------------------*/

/* clang-format off */

/* kHorizontalFunctionTable[ilv][fp] */
static const UpscaleHorizontal_t kHorizontalFunctionTable[ILCount][FPCount] = {
    /* U8,                  U10,                    U12,                    U14,                    S8.7,                   S10.5,                  S12.3,                  S14.1 */
    {horizontalU8PlanarSSE, horizontalU10PlanarSSE, horizontalU12PlanarSSE, horizontalU14PlanarSSE, horizontalS16PlanarSSE, horizontalS16PlanarSSE, horizontalS16PlanarSSE, horizontalS16PlanarSSE}, /* None*/
    {NULL,                  NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL},                   /* YUYV */
    {horizontalU8NV12SSE,   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL},                   /* NV12 */
    {NULL,                  NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL},                   /* UYVY */
    {NULL,                  NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL},                   /* RGB */
    {NULL,                  NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL,                   NULL},                   /* RGBA */
};

/* kVerticalFunctionTable[fp] */
static const UpscaleVertical_t kVerticalFunctionTable[FPCount] = {
    verticalU8SSE,    /* U8 */
    verticalU10SSE,   /* U10 */
    verticalU12SSE,   /* U12 */
    verticalU14SSE,   /* U14 */
    verticalS16SSE, /* S8.7 */
    verticalS16SSE, /* S10.5 */
    verticalS16SSE, /* S12.3 */
    verticalS16SSE, /* S14.1 */
};

/* clang-format on */

/*------------------------------------------------------------------------------*/

UpscaleHorizontal_t upscaleGetHorizontalFunctionSSE(Interleaving_t ilv, FixedPoint_t srcFP,
                                                    FixedPoint_t dstFP, FixedPoint_t baseFP)
{
    /* Conversion is not currently supported in SIMD. */
    if ((srcFP != dstFP) || ((baseFP != dstFP) && fixedPointIsValid(baseFP))) {
        return NULL;
    }

    return kHorizontalFunctionTable[ilv][srcFP];
}

UpscaleVertical_t upscaleGetVerticalFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP)
{
    /* Conversion is not currently supported in SIMD. */
    if (srcFP != dstFP) {
        return NULL;
    }

    return kVerticalFunctionTable[srcFP];
}

/*------------------------------------------------------------------------------*/

#else

UpscaleHorizontal_t upscaleGetHorizontalFunctionSSE(Interleaving_t ilv, FixedPoint_t srcFP,
                                                    FixedPoint_t dstFP, FixedPoint_t baseFP)
{
    VN_UNUSED(ilv);
    VN_UNUSED(srcFP);
    VN_UNUSED(dstFP);
    VN_UNUSED(baseFP);
    return NULL;
}

UpscaleVertical_t upscaleGetVerticalFunctionSSE(FixedPoint_t srcFP, FixedPoint_t dstFP)
{
    VN_UNUSED(srcFP);
    VN_UNUSED(dstFP);
    return NULL;
}

#endif
