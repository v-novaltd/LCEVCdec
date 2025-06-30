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

#include "upscale_scalar.h"
//
#include <LCEVC/common/limit.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include "fp_types.h"
//
#include <assert.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

/* \brief constants for upscaling. */
enum UpscaleConstants
{
    UCShift = 14,
    UCCeilRounding = 1 << (UCShift - 1),
};

/* Performs the inversion shift for the calculated convolution result.
 *
 * The result is unsaturated, as it is expected to be saturated by the caller.  */
static inline int32_t shiftResultUnsaturated(int32_t value)
{
    const int32_t result = (value + UCCeilRounding) >> UCShift;
    return result;
}

/* Perform the inversion shift for the calculated convolution result, and saturate to 15-bit.
 *
 * 15 bits is the saturation performed in the middle of upscale operations, to ensure that residuals
 * (which have, at most, 16 bits) can correct all upscaling differences.
 *
 */
static inline int16_t shiftResultSaturated(int32_t value)
{
    value = shiftResultUnsaturated(value);
    return saturateS15(value);
}

/*------------------------------------------------------------------------------*/

static inline int32_t getPelsOffset(int32_t offset, uint32_t length, int32_t pelsLength)
{
    return clampS32(offset + pelsLength - 1, 0, (int32_t)length - 1);
}

static inline void getPelsU8(const uint8_t* in, uint32_t inSize, uint32_t stride, int32_t offset,
                             uint8_t* pels, int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        const int32_t rowIndex = clampS32(offset + i, 0, (int32_t)inSize - 1);

        pels[i] = in[(size_t)rowIndex * stride];
    }
}

static inline void getNextPelsU8(const uint8_t* in, uint32_t inSize, uint32_t stride,
                                 int32_t offset, uint8_t* pels, int32_t pelsLength)
{
    /* Shuffle pels */
    for (int32_t i = 1; i < pelsLength; i++) {
        pels[i - 1] = pels[i];
    }

    /* Load the new one */
    const size_t rowIndex = (size_t)getPelsOffset(offset, inSize, pelsLength);
    pels[pelsLength - 1] = in[rowIndex * stride];
}

static inline void getPelsU16(const uint16_t* in, uint32_t inSize, uint32_t stride, int32_t offset,
                              uint16_t* pels, int32_t pelsLength)
{
    for (int32_t i = 0; i < pelsLength; i++) {
        const int32_t rowIndex = clampS32(offset + i, 0, (int32_t)inSize - 1);
        pels[i] = in[(size_t)rowIndex * stride];
    }
}

static inline void getNextPelsU16(const uint16_t* in, uint32_t inSize, uint32_t stride,
                                  int32_t offset, uint16_t* pels, int32_t pelsLength)
{
    /* Shuffle pels */
    for (int32_t i = 1; i < pelsLength; i++) {
        pels[i - 1] = pels[i];
    }

    /* Load the new one */
    const int32_t rowIndex = getPelsOffset(offset, inSize, pelsLength);
    pels[pelsLength - 1] = in[(size_t)rowIndex * stride];
}

static inline void getPelsS16(const int16_t* in, uint32_t inSize, uint32_t stride, int32_t offset,
                              int16_t* pels, int32_t pelsLength)
{
    for (int32_t i = 0; i < pelsLength; i++) {
        const int32_t rowIndex = clampS32(offset + i, 0, (int32_t)inSize - 1);
        pels[i] = in[(size_t)rowIndex * stride];
    }
}

static inline void getNextPelsS16(const int16_t* in, uint32_t inSize, uint32_t stride,
                                  int32_t offset, int16_t* pels, int32_t pelsLength)
{
    /* Shuffle pels */
    for (int32_t i = 1; i < pelsLength; i++) {
        pels[i - 1] = pels[i];
    }

    /* Load the new one */
    const int32_t rowIndex = getPelsOffset(offset, inSize, pelsLength);
    pels[pelsLength - 1] = in[(size_t)rowIndex * stride];
}

static inline void getPelsUN(const uint8_t* in, uint32_t inSize, uint32_t stride, int32_t offset,
                             int16_t* pels, int32_t pelsLength, uint32_t pelSize, uint32_t shift)
{
    for (int32_t i = 0; i < pelsLength; i++) {
        const size_t rowIndex = (size_t)clampS32(offset + i, 0, (int32_t)inSize - 1);

        if (pelSize == 1) {
            pels[i] = (int16_t)(in[rowIndex * stride] << shift);
        } else {
            assert(pelSize == 2);
            pels[i] = (int16_t)(((const int16_t*)in)[rowIndex * stride] << shift);
        }
    }
}

static inline void getNextPelsUN(const uint8_t* in, uint32_t inSize, uint32_t stride,
                                 int32_t offset, int16_t* pels, int32_t pelsLength,
                                 const uint32_t pelSize, const uint32_t shift)
{
    /* Shuffle pels */
    for (int32_t i = 1; i < pelsLength; i++) {
        pels[i - 1] = pels[i];
    }

    /* Load the new one */
    const size_t rowIndex = (size_t)getPelsOffset(offset, inSize, pelsLength);
    if (pelSize == 1) {
        pels[pelsLength - 1] = (int16_t)(in[rowIndex * stride] << shift);
    } else {
        assert(pelSize == 2);
        pels[pelsLength - 1] = (int16_t)(((const int16_t*)in)[rowIndex * stride] << shift);
    }
}

/*------------------------------------------------------------------------------*/

/*!
 * Perform horizontal upscaling of 2 lines at a time for an interleaved unsigned
 * 8-bit surface of up to 4 channels.
 *
 * This function supports multiple features:
 *
 *    - Off-pixel convolution upscaling
 *    - 1D predicted average
 *    - 2D predicted average for when horizontal is the second upscale pass (hence
 *      requiring 2 lines at a time).
 *    - dithering application
 *    - simultaneous upscaling of interleaved data up to 4 channels at a time with
 *      support for YUYV/UYVY interleaving.
 *
 * \param dither        Dither data (NULL for no dithering)
 * \param in            Byte pointers to the 2 input rows to upscale from.
 * \param out           Byte pointers to the 2 output rows to upscale to.
 * \param base          Byte pointers to 2 rows to use for PA, if the second pointer
 *                      is NULL then it is assumed that 2D PA is expected.
 * \param width         The pixel width of the input rows.
 * \param xStart        The x-coordinate to start upscaling from.
 * \param xEnd          The x-coordinate to end upscaling from.
 * \param k             The kernel to perform convolution upscaling with.
 * \param channelCount  The number of channels to upscale, up to a maximum of 4, this
 *                      is essentially the number of interleave planes to upscale.
 * \param channelSkip   An array that specifies how many pixels each channel should
 *                      skip over for each load/store
 * \param channelMap    An array that specifies a remapping of one channel to another
 *                      that has a direct impact on loads/stores and is used solely
 *                      for YUYV/UYVY where the second luma channel is really just the
 *                      next iteration of the first luma channel.
 */
static inline void horizontalU8(LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2],
                                const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                uint32_t xEnd, const LdeKernel* kernel, uint32_t channelCount,
                                const uint32_t channelSkip[4], const uint32_t channelMap[4])
{
    uint8_t pels[4][2][8]; /* Maximum kernel size is 6, use 8 for alignment. Maximum channel count of 4. */
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const uint8_t* channelIn[4][2] = {{0}};
    uint32_t channelWidth[4] = {0};
    const size_t initialBaseOffset = (size_t)xStart * channelCount;
    const uint8_t* base0 = (base[0] != NULL) ? &base[0][initialBaseOffset] : NULL;
    const uint8_t* base1 = (base[1] != NULL) ? &base[1][initialBaseOffset] : NULL;

    int32_t channelLoadOffset[4] = {
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1),
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1)};

    const int32_t initialStoreOffset = (int32_t)((xStart << 1) * channelCount);
    int32_t channelStoreOffset[4] = {initialStoreOffset, initialStoreOffset + 1,
                                     initialStoreOffset + 2, initialStoreOffset + 3};

    const uint16_t* ditherBuffer = NULL;

    assert((channelCount > 0) && (channelCount <= 4));

    /* Prime pels with initial values. */
    for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
        const uint32_t channel = channelMap[channelIdx];

        /* For channels where they map load/store to another channels PELs then
         * skip the initial load (i.e. 2nd luma channel for YUYV/UYVY). */
        if (channel == channelIdx) {
            const int32_t loadOffset = channelLoadOffset[channel];
            const uint32_t skip = channelSkip[channel];

            /* channel_width is modulated based upon skip frequency against channel
             * count. Essentially a skip < channel_count is processing more samples
             * per x iteration (i.e. 422 luma/chroma interleaved YUYV would have a
             * luma channel width twice that of the chroma channel) - this is relying
             * on how the functions are called/used. */
            const uint32_t localWidth = width * (channelCount / skip);
            channelWidth[channel] = localWidth;

            assert(skip > 0);
            channelIn[channel][0] = in[0] + channelIdx;
            channelIn[channel][1] = in[1] + channelIdx;

            getPelsU8(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0], kernelLength);
            getPelsU8(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1], kernelLength);

            channelLoadOffset[channel] += 1;
        }
    }

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows
     * for each channel */
    if (dither != NULL) {
        ditherBuffer = ldppDitherGetBuffer(dither, (xEnd - xStart) * (size_t)channelCount * 4);
    }

    for (uint32_t x = xStart; x < xEnd; ++x) {
        for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
            const uint32_t channel = channelMap[channelIdx];
            const int32_t loadOffset = channelLoadOffset[channel];
            const int32_t storeOffset = channelStoreOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = channelWidth[channel];

            memset(values, 0, sizeof(int32_t) * 4);

            /* Reverse filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[0] += (kernelRev[i] * pels[channel][0][i]);
                values[2] += (kernelRev[i] * pels[channel][1][i]);
            }

            /* Next input after reverse-phase as we are off pixel */
            getNextPelsU8(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0],
                          kernelLength);
            getNextPelsU8(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1],
                          kernelLength);

            /* Forward filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[1] += (kernelFwd[i] * pels[channel][0][i]);
                values[3] += (kernelFwd[i] * pels[channel][1][i]);
            }

            values[0] = shiftResultSaturated(values[0]);
            values[1] = shiftResultSaturated(values[1]);
            values[2] = shiftResultSaturated(values[2]);
            values[3] = shiftResultSaturated(values[3]);

            /* Apply predicted average */
            if (paEnabled1D) {
                const int32_t avg0 = *base0++ - ((values[0] + values[1] + 1) >> 1);
                const int32_t avg1 = *base1++ - ((values[2] + values[3] + 1) >> 1);

                values[0] += avg0;
                values[1] += avg0;
                values[2] += avg1;
                values[3] += avg1;
            } else if (paEnabled) {
                const int32_t avg = *base0++ - ((values[0] + values[1] + values[2] + values[3] + 2) >> 2);

                values[0] += avg;
                values[1] += avg;
                values[2] += avg;
                values[3] += avg;
            }

            /* Apply dithering */
            if (ditherBuffer) {
                ldppDitherApply(&values[0], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[1], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[2], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[3], &ditherBuffer, 0, dither->strength);
            }

            out[0][storeOffset] = saturateU8(values[0]);
            out[0][storeOffset + skip] = saturateU8(values[1]);
            out[1][storeOffset] = saturateU8(values[2]);
            out[1][storeOffset + skip] = saturateU8(values[3]);

            channelStoreOffset[channel] += (int32_t)(skip * 2);
            channelLoadOffset[channel] += 1;
        }
    }
}

/*!
 * Perform horizontal upscaling of 2 lines at a time for an interleaved signed
 * 16-bit surface of up to 4 channels.
 *
 * This function supports multiple features:
 *
 *    - Off-pixel convolution upscaling
 *    - 1D predicted average
 *    - 2D predicted average for when horizontal is the second upscale pass (hence
 *      requiring 2 lines at a time).
 *    - dithering application
 *    - simultaneous upscaling of interleaved data up to 4 channels at a time with
 *      support for YUYV/UYVY interleaving.
 *
 * \param dither        Dithering info. NULL indicates that dithering should not be performed.
 * \param in            Byte pointers to the 2 input rows to upscale from.
 * \param out           Byte pointers to the 2 output rows to upscale to.
 * \param base          Byte pointers to 2 rows to use for PA, if the second pointer
 *                      is NULL then it is assumed that 2D PA is expected.
 * \param width         The pixel width of the input rows.
 * \param xStart        The x-coordinate to start upscaling from.
 * \param xEnd          The x-coordinate to end upscaling from.
 * \param k             The kernel to perform convolution upscaling with.
 * \param channelCount  The number of channels to upscale, up to a maximum of 4.
 * \param channelSkip   An array that specifies how many pixels each channel should
 *                      skip over for each load/store
 * \param channelMap    An array that specifies a remapping of one channel to another
 *                      that has a direct impact on loads/stores and is used solely
 *                      for YUYV/UYVY where the second luma channel is really just the
 *                      next iteration of the first luma channel.
 *
 * \note See `upscale_horizontal_u8_impl` implementation for more details comments.
 */
static void horizontalS16(LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2],
                          const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                          const LdeKernel* kernel, uint32_t channelCount, const uint32_t channelSkip[4],
                          const uint32_t channelMap[4], const LdpFixedPoint dstFP)
{
    int16_t pels[4][2][8];
    int16_t* outI16[2] = {(int16_t*)out[0], (int16_t*)out[1]};
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4] = {0};
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const int16_t* channelIn[4][2] = {{0}};
    uint32_t channelWidth[4] = {0};
    const size_t initialBaseOffset = (size_t)xStart * channelCount;
    const int16_t* base0 = (base[0] != NULL) ? &((const int16_t*)base[0])[initialBaseOffset] : NULL;
    const int16_t* base1 = (base[1] != NULL) ? &((const int16_t*)base[1])[initialBaseOffset] : NULL;

    int32_t channelLoadOffset[4] = {
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1),
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1)};

    const int32_t initialStoreOffset = (int32_t)((xStart << 1) * channelCount);
    int32_t channelStoreOffset[4] = {initialStoreOffset, initialStoreOffset + 1,
                                     initialStoreOffset + 2, initialStoreOffset + 3};

    const uint16_t* ditherBuffer = NULL;
    int8_t shift = 0;

    assert((channelCount > 0) && (channelCount <= 4));

    /* Prime pels with initial values. */
    for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
        const uint32_t channel = channelMap[channelIdx];

        if (channel == channelIdx) {
            const int32_t loadOffset = channelLoadOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = width * (channelCount / skip);
            channelWidth[channel] = localWidth;

            assert(skip > 0);
            channelIn[channel][0] = (const int16_t*)in[0] + channelIdx;
            channelIn[channel][1] = (const int16_t*)in[1] + channelIdx;

            getPelsS16(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0], kernelLength);
            getPelsS16(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1], kernelLength);

            channelLoadOffset[channel] += 1;
        }
    }

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows
     * for each channel */
    if (dither != NULL) {
        ditherBuffer = ldppDitherGetBuffer(dither, (xEnd - xStart) * (size_t)channelCount * 4);
        shift = ldppDitherGetShiftS16(dstFP);
    }

    for (uint32_t x = xStart; x < xEnd; ++x) {
        for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
            const uint32_t channel = channelMap[channelIdx];
            const int32_t loadOffset = channelLoadOffset[channel];
            const int32_t storeOffset = channelStoreOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = channelWidth[channel];

            memset(values, 0, sizeof(int32_t) * 4);

            /* Reverse filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[0] += (kernelRev[i] * pels[channel][0][i]);
                values[2] += (kernelRev[i] * pels[channel][1][i]);
            }

            /* Next input after reverse-phase as we are off pixel */
            getNextPelsS16(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0],
                           kernelLength);
            getNextPelsS16(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1],
                           kernelLength);

            /* Forward filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[1] += (kernelFwd[i] * pels[channel][0][i]);
                values[3] += (kernelFwd[i] * pels[channel][1][i]);
            }

            values[0] = shiftResultSaturated(values[0]);
            values[1] = shiftResultSaturated(values[1]);
            values[2] = shiftResultSaturated(values[2]);
            values[3] = shiftResultSaturated(values[3]);

            /* Apply predicted average */
            if (paEnabled1D) {
                const int32_t avg0 = *base0++ - ((values[0] + values[1] + 1) >> 1);
                const int32_t avg1 = *base1++ - ((values[2] + values[3] + 1) >> 1);

                values[0] += avg0;
                values[1] += avg0;
                values[2] += avg1;
                values[3] += avg1;
            } else if (paEnabled) {
                const int32_t avg = *base0++ - ((values[0] + values[1] + values[2] + values[3] + 2) >> 2);

                values[0] += avg;
                values[1] += avg;
                values[2] += avg;
                values[3] += avg;
            }

            /* Apply dithering */
            if (ditherBuffer) {
                ldppDitherApply(&values[0], &ditherBuffer, shift, dither->strength);
                ldppDitherApply(&values[1], &ditherBuffer, shift, dither->strength);
                ldppDitherApply(&values[2], &ditherBuffer, shift, dither->strength);
                ldppDitherApply(&values[3], &ditherBuffer, shift, dither->strength);
            }

            outI16[0][storeOffset] = saturateS16(values[0]);
            outI16[0][storeOffset + skip] = saturateS16(values[1]);
            outI16[1][storeOffset] = saturateS16(values[2]);
            outI16[1][storeOffset + skip] = saturateS16(values[3]);

            channelStoreOffset[channel] += (int32_t)(skip * 2);
            channelLoadOffset[channel] += 1;
        }
    }
}

/*!
 * Perform horizontal upscaling of 2 lines at a time for an interleaved unsigned
 * 16-bit surface of up to 4 channels.
 *
 * This function supports multiple features:
 *
 *    - Off-pixel convolution upscaling
 *    - 1D predicted average
 *    - 2D predicted average for when horizontal is the second upscale pass (hence
 *      requiring 2 lines at a time).
 *    - dithering application
 *    - simultaneous upscaling of interleaved data up to 4 channels at a time with
 *      support for YUYV/UYVY interleaving.
 *
 * \param dither        Dithering data (set to null if not desired)
 * \param in            Byte pointers to the 2 input rows to upscale from.
 * \param out           Byte pointers to the 2 output rows to upscale to.
 * \param base          Byte pointers to 2 rows to use for PA, if the second pointer
 *                      is NULL then it is assumed that 2D PA is expected.
 * \param width         The pixel width of the input rows.
 * \param xStart        The x-coordinate to start upscaling from.
 * \param xEnd          The x-coordinate to end upscaling from.
 * \param k             The kernel to perform convolution upscaling with.
 * \param channelCount  The number of channels to upscale, up to a maximum of 4.
 * \param channelSkip   An array that specifies how many pixels each channel should
 *                      skip over for each load/store
 * \param channelMap    An array that specifies a remapping of one channel to another
 *                      that has a direct impact on loads/stores and is used solely
 *                      for YUYV/UYVY where the second luma channel is really just the
 *                      next iteration of the first luma channel.
 * \param maxValue      The maximum value that can be stored.
 *
 * \note See `upscale_horizontal_u8_impl` implementation for more details comments.
 */
static void horizontalU16(LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2],
                          const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                          const LdeKernel* kernel, uint32_t channelCount,
                          const uint32_t channelSkip[4], const uint32_t channelMap[4], uint16_t maxValue)
{
    uint16_t pels[4][2][8];
    uint16_t* outU16[2] = {(uint16_t*)out[0], (uint16_t*)out[1]};
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const uint16_t* channelIn[4][2] = {{0}};
    uint32_t channelWidth[4] = {0};
    const size_t initialBaseOffset = (size_t)xStart * channelCount;
    const uint16_t* base0 = (base[0] != NULL) ? &((const uint16_t*)base[0])[initialBaseOffset] : NULL;
    const uint16_t* base1 = (base[1] != NULL) ? &((const uint16_t*)base[1])[initialBaseOffset] : NULL;

    int32_t channelLoadOffset[4] = {
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1),
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1)};

    const int32_t initialStoreOffset = (int32_t)((xStart << 1) * channelCount);
    int32_t channelStoreOffset[4] = {initialStoreOffset, initialStoreOffset + 1,
                                     initialStoreOffset + 2, initialStoreOffset + 3};

    const uint16_t* ditherBuffer = NULL;

    assert((channelCount > 0) && (channelCount <= 4));

    /* Prime pels with initial values. */
    for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
        const uint32_t channel = channelMap[channelIdx];

        if (channel == channelIdx) {
            const int32_t loadOffset = channelLoadOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = width * (channelCount / skip);
            channelWidth[channel] = localWidth;

            assert(skip > 0);
            channelIn[channel][0] = (const uint16_t*)in[0] + channelIdx;
            channelIn[channel][1] = (const uint16_t*)in[1] + channelIdx;

            getPelsU16(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0], kernelLength);
            getPelsU16(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1], kernelLength);

            channelLoadOffset[channel] += 1;
        }
    }

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows
     * for each channel */
    if (dither != NULL) {
        ditherBuffer = ldppDitherGetBuffer(dither, (size_t)channelCount * (xEnd - xStart) * 4);
    }

    for (uint32_t x = xStart; x < xEnd; ++x) {
        for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
            const uint32_t channel = channelMap[channelIdx];
            const int32_t loadOffset = channelLoadOffset[channel];
            const int32_t storeOffset = channelStoreOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = channelWidth[channel];

            memset(values, 0, sizeof(int32_t) * 4);

            /* Reverse filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[0] += (kernelRev[i] * pels[channel][0][i]);
                values[2] += (kernelRev[i] * pels[channel][1][i]);
            }

            /* Next input after reverse-phase as we are off pixel */
            getNextPelsU16(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0],
                           kernelLength);
            getNextPelsU16(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1],
                           kernelLength);

            /* Forward filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[1] += (kernelFwd[i] * pels[channel][0][i]);
                values[3] += (kernelFwd[i] * pels[channel][1][i]);
            }

            values[0] = shiftResultSaturated(values[0]);
            values[1] = shiftResultSaturated(values[1]);
            values[2] = shiftResultSaturated(values[2]);
            values[3] = shiftResultSaturated(values[3]);

            /* Apply predicted average */
            if (paEnabled1D) {
                const int32_t avg0 = *base0++ - ((values[0] + values[1] + 1) >> 1);
                const int32_t avg1 = *base1++ - ((values[2] + values[3] + 1) >> 1);

                values[0] += avg0;
                values[1] += avg0;
                values[2] += avg1;
                values[3] += avg1;
            } else if (paEnabled) {
                const int32_t avg = *base0++ - ((values[0] + values[1] + values[2] + values[3] + 2) >> 2);

                values[0] += avg;
                values[1] += avg;
                values[2] += avg;
                values[3] += avg;
            }

            /* Apply dithering */
            if (ditherBuffer) {
                ldppDitherApply(&values[0], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[1], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[2], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[3], &ditherBuffer, 0, dither->strength);
            }

            outU16[0][storeOffset] = saturateUN(values[0], maxValue);
            outU16[0][storeOffset + skip] = saturateUN(values[1], maxValue);
            outU16[1][storeOffset] = saturateUN(values[2], maxValue);
            outU16[1][storeOffset + skip] = saturateUN(values[3], maxValue);

            channelStoreOffset[channel] += (int32_t)(skip * 2);
            channelLoadOffset[channel] += 1;
        }
    }
}

static void horizontalU8ToU16BaseUN(LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2],
                                    const uint8_t* base[2], uint32_t width, uint32_t xStart,
                                    uint32_t xEnd, const LdeKernel* kernel, uint32_t channelCount,
                                    const uint32_t channelSkip[4], const uint32_t channelMap[4],
                                    const uint32_t inPelSize, const uint32_t inShift,
                                    const uint32_t basePelSize, const uint32_t baseShift, uint16_t maxValue)
{
    int16_t pels[4][2][8];
    uint16_t* outU16[2] = {(uint16_t*)out[0], (uint16_t*)out[1]};
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const bool paEnabled = (base[0] != NULL);
    const bool paEnabled1D = paEnabled && (base[1] != NULL);
    const uint8_t* channelIn[4][2] = {{0}};
    uint32_t channelWidth[4] = {0};
    const size_t initialBaseOffset = (size_t)xStart * channelCount * basePelSize;
    const uint8_t* base0 = (base[0] != NULL) ? &base[0][initialBaseOffset] : NULL;
    const uint8_t* base1 = (base[1] != NULL) ? &base[1][initialBaseOffset] : NULL;

    int32_t channelLoadOffset[4] = {
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1),
        (int32_t)xStart - (kernelLength >> 1), (int32_t)xStart - (kernelLength >> 1)};

    const int32_t initialStoreOffset = (int32_t)((xStart << 1) * channelCount);
    int32_t channelStoreOffset[4] = {initialStoreOffset, initialStoreOffset + 1,
                                     initialStoreOffset + 2, initialStoreOffset + 3};

    const uint16_t* ditherBuffer = NULL;

    assert((channelCount > 0) && (channelCount <= 4));

    /* Prime pels with initial values. */
    for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
        const uint32_t channel = channelMap[channelIdx];

        if (channel == channelIdx) {
            const int32_t loadOffset = channelLoadOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = width * (channelCount / skip);
            channelWidth[channel] = localWidth;

            assert(skip > 0);
            channelIn[channel][0] = in[0] + ((size_t)channelIdx * inPelSize);
            channelIn[channel][1] = in[1] + ((size_t)channelIdx * inPelSize);

            getPelsUN(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0],
                      kernelLength, inPelSize, inShift);
            getPelsUN(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1],
                      kernelLength, inPelSize, inShift);

            channelLoadOffset[channel] += 1;
        }
    }

    /* Prepare dither buffer containing enough values for 2 fully upscaled rows
     * for each channel */
    if (dither != NULL) {
        ditherBuffer = ldppDitherGetBuffer(dither, (xEnd - xStart) * (size_t)channelCount * 4);
    }

    for (uint32_t x = xStart; x < xEnd; ++x) {
        for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx) {
            const uint32_t channel = channelMap[channelIdx];
            const int32_t loadOffset = channelLoadOffset[channel];
            const int32_t storeOffset = channelStoreOffset[channel];
            const uint32_t skip = channelSkip[channel];
            const uint32_t localWidth = channelWidth[channel];

            memset(values, 0, sizeof(int32_t) * 4);

            /* Reverse filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[0] += (kernelRev[i] * pels[channel][0][i]);
                values[2] += (kernelRev[i] * pels[channel][1][i]);
            }

            /* Next input after reverse-phase as we are off pixel */
            getNextPelsUN(channelIn[channel][0], localWidth, skip, loadOffset, pels[channel][0],
                          kernelLength, inPelSize, inShift);
            getNextPelsUN(channelIn[channel][1], localWidth, skip, loadOffset, pels[channel][1],
                          kernelLength, inPelSize, inShift);

            /* Forward filter */
            for (int32_t i = 0; i < kernelLength; ++i) {
                values[1] += (kernelFwd[i] * pels[channel][0][i]);
                values[3] += (kernelFwd[i] * pels[channel][1][i]);
            }

            values[0] = shiftResultSaturated(values[0]);
            values[1] = shiftResultSaturated(values[1]);
            values[2] = shiftResultSaturated(values[2]);
            values[3] = shiftResultSaturated(values[3]);

            /* Apply predicted average */
            if (paEnabled1D) {
                int16_t base0Value = 0;
                int16_t base1Value = 0;

                if (basePelSize == 1) {
                    base0Value = (int16_t)((*base0++) << baseShift);
                    base1Value = (int16_t)((*base1++) << baseShift);
                } else {
                    base0Value = (int16_t)((*(int16_t*)base0) << baseShift);
                    base1Value = (int16_t)((*(int16_t*)base1) << baseShift);

                    base0 += basePelSize;
                    base1 += basePelSize;
                }

                const int32_t avg0 = base0Value - ((values[0] + values[1] + 1) >> 1);
                const int32_t avg1 = base1Value - ((values[2] + values[3] + 1) >> 1);

                values[0] += avg0;
                values[1] += avg0;
                values[2] += avg1;
                values[3] += avg1;
            } else if (paEnabled) {
                int16_t base0Value = 0;

                if (basePelSize == 1) {
                    base0Value = (int16_t)((*base0++) << baseShift);
                } else {
                    base0Value = (int16_t)((*(int16_t*)base0) << baseShift);
                    base0 += basePelSize;
                }

                const int32_t avg =
                    base0Value - ((values[0] + values[1] + values[2] + values[3] + 2) >> 2);

                values[0] += avg;
                values[1] += avg;
                values[2] += avg;
                values[3] += avg;
            }

            /* Apply dithering */
            if (ditherBuffer) {
                ldppDitherApply(&values[0], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[1], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[2], &ditherBuffer, 0, dither->strength);
                ldppDitherApply(&values[3], &ditherBuffer, 0, dither->strength);
            }

            outU16[0][storeOffset] = saturateUN(values[0], maxValue);
            outU16[0][storeOffset + skip] = saturateUN(values[1], maxValue);
            outU16[1][storeOffset] = saturateUN(values[2], maxValue);
            outU16[1][storeOffset + skip] = saturateUN(values[3], maxValue);

            channelStoreOffset[channel] += (int32_t)(skip * 2);
            channelLoadOffset[channel] += 1;
        }
    }
}

void horizontalUNPlanar(LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2],
                        const uint8_t* base[2], uint32_t width, uint32_t xStart, uint32_t xEnd,
                        const LdeKernel* kernel, uint16_t maxValue)
{
    const uint32_t channelSkip[4] = {1, 0, 0, 0};
    const uint32_t channelMap[4] = {0, 0, 0, 0};
    horizontalU16(dither, in, out, base, width, xStart, xEnd, kernel, 1, channelSkip, channelMap, maxValue);
}

/*!
 * Perform vertical upscaling of 2 columns at a time for unsigned 8-bit surfaces.
 *
 * \param in           Byte pointer to the input surface data pointer offset by the first column to
 *                     upscale from.
 * \param inStride     The pixel stride of the input surface.
 * \param out          Byte pointer to the output surface data pointer offset by the first column
 *                     to upscale to.
 * \param outStride    The pixel stride of the output surface.
 * \param y            The y coordinate to start upscaling from on the input surface.
 * \param rows         The number of rows in the column to upscale starting from y.
 * \param height       The pixel height of the input surface.
 * \param kernel       The kernel to perform convolution upscaling with.
 *
 * \note To reiterate, the `in` and `out` pointers are pointers to the allocated surface, offsset by
 *       the pixel column that you're upscaling from/to.
 *
 *       These pointers are then offset by (y * <>_stride). This is because the `in` surface is
 *       indexed like so:
 *           index_range = clamp([y - (kernel_length / 2) <-> y + rows + (kernel_length / 2)],
 *                               0,
 *                               height - 1);
 */
static void verticalU8(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                       uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    uint8_t pels[2][8];
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const uint32_t outSkip = 2 * outStride;
    uint8_t* out0 = out + ((size_t)y * outSkip);
    uint8_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);

    getPelsU8(in, height, inStride, loadOffset, pels[0], kernelLength);
    getPelsU8(in + 1, height, inStride, loadOffset, pels[1], kernelLength);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        memset(values, 0, sizeof(int32_t) * 4);

        /* Reverse filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[0] += (kernelRev[i] * pels[0][i]);
            values[1] += (kernelRev[i] * pels[1][i]);
        }

        /* Next input after reverse-phase as we are off pixel */
        getNextPelsU8(in, height, inStride, loadOffset, pels[0], kernelLength);
        getNextPelsU8(in + 1, height, inStride, loadOffset, pels[1], kernelLength);
        loadOffset += 1;

        /* Forward filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[2] += (kernelFwd[i] * pels[0][i]);
            values[3] += (kernelFwd[i] * pels[1][i]);
        }

        out0[0] = saturateU8(shiftResultUnsaturated(values[0]));
        out0[1] = saturateU8(shiftResultUnsaturated(values[1]));
        out1[0] = saturateU8(shiftResultUnsaturated(values[2]));
        out1[1] = saturateU8(shiftResultUnsaturated(values[3]));

        out0 += outSkip;
        out1 += outSkip;
    }
}

/*!
 * Perform vertical upscaling of 2 columns at a time for signed 16-bit surfaces.
 *
 * \param in           Byte pointer to the input surface data pointer offset by
 *                     the first column to upscale from.
 * \param inStride     The pixel stride of the input surface.
 * \param out          Byte pointer to the output surface data pointer offset by
 *                     the first column to upscale to.
 * \param outStride    The pixel stride of the output surface.
 * \param y            The y coordinate to start upscaling from on the input surface.
 * \param rows         The number of rows in the column to upscale starting from y.
 * \param height       The pixel height of the input surface.
 * \param kernel       The kernel to perform convolution upscaling with.
 *
 * \note See `upscale_vertical_u8` for more details.
 */
static void verticalS16(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                        uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    int16_t pels[2][8];
    const int16_t* inI16 = (const int16_t*)in;
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const uint32_t outSkip = 2 * outStride;
    int16_t* out0 = (int16_t*)out + ((size_t)y * outSkip);
    int16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);

    getPelsS16(inI16, height, inStride, loadOffset, pels[0], kernelLength);
    getPelsS16(inI16 + 1, height, inStride, loadOffset, pels[1], kernelLength);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        memset(values, 0, sizeof(int32_t) * 4);

        /* Reverse filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[0] += (kernelRev[i] * pels[0][i]);
            values[1] += (kernelRev[i] * pels[1][i]);
        }

        /* Next input after reverse-phase as we are off pixel */
        getNextPelsS16(inI16, height, inStride, loadOffset, pels[0], kernelLength);
        getNextPelsS16(inI16 + 1, height, inStride, loadOffset, pels[1], kernelLength);
        loadOffset += 1;

        /* Forward filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[2] += (kernelFwd[i] * pels[0][i]);
            values[3] += (kernelFwd[i] * pels[1][i]);
        }

        out0[0] = shiftResultSaturated(values[0]);
        out0[1] = shiftResultSaturated(values[1]);
        out1[0] = shiftResultSaturated(values[2]);
        out1[1] = shiftResultSaturated(values[3]);

        out0 += outSkip;
        out1 += outSkip;
    }
}

/*!
 * Perform vertical upscaling of 2 columns at a time for unsigned 16-bit surfaces.
 *
 * \param in           Byte pointer to the input surface data pointer offset by
 *                     the first column to upscale from.
 * \param inStride     The pixel stride of the input surface.
 * \param out          Byte pointer to the output surface data pointer offset by
 *                     the first column to upscale to.
 * \param outStride    The pixel stride of the output surface.
 * \param y            The y coordinate to start upscaling from on the input surface.
 * \param rows         The number of rows in the column to upscale starting from y.
 * \param height       The pixel height of the input surface.
 * \param kernel       The kernel to perform convolution upscaling with.
 * \param maxValue     The maximum value that can be stored.
 *
 * \note See `upscale_vertical_u8` for more details.
 */
static void verticalU16(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride, uint32_t y,
                        uint32_t rows, uint32_t height, const LdeKernel* kernel, uint16_t maxValue)
{
    uint16_t pels[2][8];
    const uint16_t* inU16 = (const uint16_t*)in;
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const uint32_t outSkip = 2 * outStride;
    uint16_t* out0 = (uint16_t*)out + ((size_t)y * outSkip);
    uint16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);

    getPelsU16(inU16, height, inStride, loadOffset, pels[0], kernelLength);
    getPelsU16(inU16 + 1, height, inStride, loadOffset, pels[1], kernelLength);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        memset(values, 0, sizeof(int32_t) * 4);

        /* Reverse filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[0] += (kernelRev[i] * pels[0][i]);
            values[1] += (kernelRev[i] * pels[1][i]);
        }

        /* Next input after reverse-phase as we are off pixel */
        getNextPelsU16(inU16, height, inStride, loadOffset, pels[0], kernelLength);
        getNextPelsU16(inU16 + 1, height, inStride, loadOffset, pels[1], kernelLength);
        loadOffset += 1;

        /* Forward filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[2] += (kernelFwd[i] * pels[0][i]);
            values[3] += (kernelFwd[i] * pels[1][i]);
        }

        out0[0] = saturateUN(shiftResultUnsaturated(values[0]), maxValue);
        out0[1] = saturateUN(shiftResultUnsaturated(values[1]), maxValue);
        out1[0] = saturateUN(shiftResultUnsaturated(values[2]), maxValue);
        out1[1] = saturateUN(shiftResultUnsaturated(values[3]), maxValue);

        out0 += outSkip;
        out1 += outSkip;
    }
}

/* Promoting input vertical upscale from N-bits to 16-bits. */
static void verticalUNToU16(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                            uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel,
                            const uint32_t inPelSize, const uint32_t inShift, const uint16_t maxValue)
{
    int16_t pels[2][8];
    const int16_t* kernelFwd = kernel->coeffs[0];
    const int16_t* kernelRev = kernel->coeffs[1];
    const int32_t kernelLength = (int32_t)kernel->length;
    int32_t values[4];
    const uint32_t outSkip = 2 * outStride;
    uint16_t* out0 = (uint16_t*)out + ((size_t)y * outSkip);
    uint16_t* out1 = out0 + outStride;
    int32_t loadOffset = (int32_t)y - (kernelLength / 2);

    getPelsUN(in, height, inStride, loadOffset, pels[0], kernelLength, inPelSize, inShift);
    getPelsUN(in + inPelSize, height, inStride, loadOffset, pels[1], kernelLength, inPelSize, inShift);
    loadOffset += 1;

    for (uint32_t rowIndex = 0; rowIndex < rows; ++rowIndex) {
        memset(values, 0, sizeof(int32_t) * 4);

        /* Reverse filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[0] += (kernelRev[i] * pels[0][i]);
            values[1] += (kernelRev[i] * pels[1][i]);
        }

        /* Next input after reverse-phase as we are off pixel */
        getNextPelsUN(in, height, inStride, loadOffset, pels[0], kernelLength, inPelSize, inShift);
        getNextPelsUN(in + inPelSize, height, inStride, loadOffset, pels[1], kernelLength,
                      inPelSize, inShift);
        loadOffset += 1;

        /* Forward filter */
        for (int32_t i = 0; i < kernelLength; ++i) {
            values[2] += (kernelFwd[i] * pels[0][i]);
            values[3] += (kernelFwd[i] * pels[1][i]);
        }

        out0[0] = saturateUN(shiftResultUnsaturated(values[0]), maxValue);
        out0[1] = saturateUN(shiftResultUnsaturated(values[1]), maxValue);
        out1[0] = saturateUN(shiftResultUnsaturated(values[2]), maxValue);
        out1[1] = saturateUN(shiftResultUnsaturated(values[3]), maxValue);

        out0 += outSkip;
        out1 += outSkip;
    }
}

void verticalU10(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride, uint32_t y,
                 uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalU16(in, inStride, out, outStride, y, rows, height, kernel, 1023);
}

void verticalU12(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride, uint32_t y,
                 uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalU16(in, inStride, out, outStride, y, rows, height, kernel, 4095);
}

void verticalU14(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride, uint32_t y,
                 uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalU16(in, inStride, out, outStride, y, rows, height, kernel, 16383);
}

void verticalU8ToU10(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 1, 2, 1023);
}

void verticalU8ToU12(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 1, 4, 4095);
}

void verticalU8ToU14(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                     uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 1, 6, 16383);
}

void verticalU10ToU12(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                      uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 2, 2, 4095);
}

void verticalU10ToU14(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                      uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 2, 4, 16383);
}

void verticalU12ToU14(const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride,
                      uint32_t y, uint32_t rows, uint32_t height, const LdeKernel* kernel)
{
    verticalUNToU16(in, inStride, out, outStride, y, rows, height, kernel, 2, 2, 16383);
}

/*------------------------------------------------------------------------------*/

/* clang-format off */

/* The following variables and macros provide the metadata used by the generator
 * macros. */

static const uint32_t kChannelSkipPlanar[4] = {1, 0, 0, 0};
static const uint32_t kChannelMapPlanar[4]  = {0, 0, 0, 0};

static const uint32_t kChannelSkipYUYV[4] = {2, 4, 2, 4};
static const uint32_t kChannelMapYUYV[4]  = {0, 1, 0, 3};

static const uint32_t kChannelSkipNV12[4] = {2, 2, 0, 0};
static const uint32_t kChannelMapNV12[4]  = {0, 1, 0, 0};

static const uint32_t kChannelSkipUYVY[4] = {4, 2, 4, 2};
static const uint32_t kChannelMapUYVY[4]  = {0, 1, 2, 1};

static const uint32_t kChannelSkipRGB[4] = {3, 3, 3, 0};
static const uint32_t kChannelMapRGB[4]  = {0, 1, 2, 0};

static const uint32_t kChannelSkipRGBA[4] = {4, 4, 4, 4};
static const uint32_t kChannelMapRGBA[4]  = {0, 1, 2, 3};

static const uint32_t kFormatBytes_U8  = 1;
static const uint32_t kFormatBytes_U10 = 2;
static const uint32_t kFormatBytes_U12 = 2;
static const uint32_t kFormatBytes_U14 = 2;

static const uint32_t kShift_U8_U10  = 2;
static const uint32_t kShift_U8_U12  = 4;
static const uint32_t kShift_U10_U12 = 2;
static const uint32_t kShift_U10_U10 = 0;
static const uint32_t kShift_U12_U12 = 0;
static const uint32_t kShift_U14_U14 = 0;
static const uint32_t kShift_U8_U14  = 6;
static const uint32_t kShift_U10_U14 = 4;
static const uint32_t kShift_U12_U14 = 2;

static const uint32_t kChannelCount_Planar = 1;
static const uint32_t kChannelCount_YUYV   = 4;
static const uint32_t kChannelCount_NV12   = 2;
static const uint32_t kChannelCount_UYVY   = 4;
static const uint32_t kChannelCount_RGB    = 3;
static const uint32_t kChannelCount_RGBA   = 4;

static const uint32_t kLumaShift_Planar = 0;
static const uint32_t kLumaShift_YUYV   = 1;
static const uint32_t kLumaShift_NV12   = 0;
static const uint32_t kLumaShift_UYVY   = 1;
static const uint32_t kLumaShift_RGB    = 0;
static const uint32_t kLumaShift_RGBA   = 0;

#define VN_HORI_MAX_VALUE_U8()
#define VN_HORI_MAX_VALUE_U10() , 1023
#define VN_HORI_MAX_VALUE_U12() , 4095
#define VN_HORI_MAX_VALUE_U14() , 16383
#define VN_HORI_MAX_VALUE_S16() , dstFP

static const uint16_t kMaxValuePromotion_U10 = 1023;
static const uint16_t kMaxValuePromotion_U12 = 4095;
static const uint16_t kMaxValuePromotion_U14 = 16383;

/*------------------------------------------------------------------------------*/

/* Helper macro for generating a new upscale_horizontal() function specialisation
 * as a functor.
 *
 * \param fn   The function to be invoked by this functor.
 * \param ilv  The interleaving type for determining the channel skip and mapping.
 * \param fp   The fixedpoint type - for all signed types s16 should be used. */
#define VN_GEN_HORI_FUNCTION(fn, ilv, fp)                                                      \
    void horizontal##fp##ilv(                                                                  \
        LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],        \
        uint32_t width, uint32_t xStart, uint32_t xEnd, const LdeKernel* kernel, const LdpFixedPoint dstFP) {              \
        fn(dither, in, out, base, width >> kLumaShift_##ilv,                                   \
           xStart >> kLumaShift_##ilv, xEnd >> kLumaShift_##ilv,                               \
           kernel, kChannelCount_##ilv, kChannelSkip##ilv,                                     \
           kChannelMap##ilv VN_HORI_MAX_VALUE_##fp()); }

/* Helper macro for generating a single upscale_horizontal() unsigned promotion
 * function specialisation as a functor.
 *
 * This macro generates a functor that calls upscale_horizontal_uN_u16_base_uN_impl
 * function with the appropriate arguments based upon interleaving type, the source
 * fixedpoint type, the destination fixedpoint type and the base fixedpoint type.
 *
 * \param ilv      The interleaving type for determing the channel skip and mapping.
 * \param srcFP   The source data fixedpoint type, must be one of u8, u10, u12.
 * \param dstFP   The destination data fixedpoint type, must be one of u10, u12.
 * \param baseFP  The base data fixedpoint type, must be one of u8, u10.
 */
#define VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, srcFP, dstFP, baseFP)                         \
    void horizontal##srcFP##To##dstFP##Base##baseFP##ilv(                                      \
        LdppDitherSlice* dither, const uint8_t* in[2], uint8_t* out[2], const uint8_t* base[2],        \
        uint32_t width, uint32_t xStart, uint32_t xEnd, const LdeKernel* kernel, const LdpFixedPoint dstFP) {              \
            horizontalU8ToU16BaseUN(                                                           \
            dither, in, out, base, width >> kLumaShift_##ilv,                                  \
            xStart >> kLumaShift_##ilv, xEnd >> kLumaShift_##ilv,                              \
            kernel, kChannelCount_##ilv, kChannelSkip##ilv,                                    \
            kChannelMap##ilv, kFormatBytes_##srcFP,                                            \
            kShift_##srcFP##_##dstFP, kFormatBytes_##baseFP,                                   \
            kShift_##baseFP##_##dstFP, kMaxValuePromotion_##dstFP); }

/* Helper macro for generating the various horizontal functor interleaving
 * non-converting combinations required for a given fixedpoint type.
 *
 * \param fn   The function to be invoked by the functors.
 * \param fp   The fixedpoint type - for all signed types s16 should be used.
 */
#define VN_GEN_HORI_FUNCS_FOR_FP(fn, fp) \
    VN_GEN_HORI_FUNCTION(fn, Planar, fp) \
    VN_GEN_HORI_FUNCTION(fn, NV12,   fp) \
    VN_GEN_HORI_FUNCTION(fn, YUYV,   fp) \
    VN_GEN_HORI_FUNCTION(fn, UYVY,   fp) \
    VN_GEN_HORI_FUNCTION(fn, RGB,    fp) \
    VN_GEN_HORI_FUNCTION(fn, RGBA,   fp)

/* Helper macro for generating the various functor fixedpoint converting
 * combinations required for a given interleaving type.
 *
 * \param ilv   The interleaving type to generate the functors for.
 */
#define VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(ilv)          \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U8,  U10, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U8,  U12, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U8,  U14, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U10, U10, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U12, U12, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U14, U14, U8);  \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U10, U12, U10); \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U10, U14, U10); \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U12, U12, U10); \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U14, U14, U10); \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U12, U14, U12); \
    VN_GEN_HORI_UNSIGNED_PROMOTION_FUNC(ilv, U14, U14, U12);

/* Generate non-converting functors calling the correct functions for each FP
 * representation. */
VN_GEN_HORI_FUNCS_FOR_FP(horizontalU8,  U8)
VN_GEN_HORI_FUNCS_FOR_FP(horizontalU16, U10)
VN_GEN_HORI_FUNCS_FOR_FP(horizontalU16, U12)
VN_GEN_HORI_FUNCS_FOR_FP(horizontalU16, U14)
VN_GEN_HORI_FUNCS_FOR_FP(horizontalS16, S16)

/* Generate the unsigned promotion functors for each interleaving type. */
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(Planar);
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(NV12);
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(YUYV);
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(UYVY);
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(RGB);
VN_GEN_HORI_UNSIGNED_PROMOTION_FOR_ILV(RGBA);

/*------------------------------------------------------------------------------*/

/* Helper macro for generating a single entry in the kHoriFuncTableUnsigned lookup
 * table for non-converting unsigned horizontal upscales.
 *
 * \param ilv   The interleaving type of the functions to register.
 */
#define VN_REG_HORI_FUNCS_UNSIGNED(ilv) \
    {                                   \
        horizontalU8##ilv,            \
        horizontalU10##ilv,           \
        horizontalU12##ilv,           \
        horizontalU14##ilv,           \
        horizontalS16##ilv,           \
        horizontalS16##ilv,           \
        horizontalS16##ilv,           \
        horizontalS16##ilv,           \
    }

/* Helper macro for generating a single top-level entry in the kHoriFuncTablePromotion
 * lookup table for unsigned promotion horizontal upscales.
 *
 * This table is relatively sparse as the current valid upscale operations are always
 * promoting, which eliminates a whole bunch of potential combinations - this will
 * eventually change to support demotions for full conformance.
 *
 * \param ilv   The interleaving type of the functions to register.
 */
#define VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(ilv)                                                             \
    {                                                                                                         \
        {                                                                                                     \
            {NULL, horizontalU8ToU10BaseU8##ilv, horizontalU8ToU12BaseU8##ilv, horizontalU8ToU14BaseU8##ilv}, \
            {NULL, horizontalU10ToU10BaseU8##ilv, NULL, NULL},                                                \
            {NULL, NULL, horizontalU12ToU12BaseU8##ilv, NULL},                                                \
            {NULL, NULL, NULL, horizontalU14ToU14BaseU8##ilv},                                                \
        },                                                                                                    \
        {                                                                                                     \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, horizontalU10ToU12BaseU10##ilv, horizontalU10ToU14BaseU10##ilv},                     \
            {NULL, NULL, horizontalU12ToU12BaseU10##ilv, NULL},                                               \
            {NULL, NULL, NULL, horizontalU14ToU14BaseU10##ilv},                                               \
        },                                                                                                    \
        {                                                                                                     \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, NULL, horizontalU12ToU14BaseU12##ilv},                                               \
            {NULL, NULL, NULL, horizontalU14ToU14BaseU12##ilv},                                               \
        },                                                                                                    \
        {                                                                                                     \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, NULL, NULL},                                                                         \
            {NULL, NULL, NULL, NULL},                                                                         \
        }                                                                                                     \
    }

/* kHoriFuncTableUnsigned[interleaving][fp] */
static const UpscaleHorizontalFunction kHorizontalFuncTableUnsigned[ILCount][LdpFPCount] = {
    VN_REG_HORI_FUNCS_UNSIGNED(Planar),
    VN_REG_HORI_FUNCS_UNSIGNED(YUYV),
    VN_REG_HORI_FUNCS_UNSIGNED(NV12),
    VN_REG_HORI_FUNCS_UNSIGNED(UYVY),
    VN_REG_HORI_FUNCS_UNSIGNED(RGB),
    VN_REG_HORI_FUNCS_UNSIGNED(RGBA),
};

/* kHoriFuncTableSigned[interleaving] */
static const UpscaleHorizontalFunction kHorizontalFuncTableSigned[ILCount] = {
    horizontalS16Planar,
    horizontalS16YUYV,
    horizontalS16NV12,
    horizontalS16UYVY,
    horizontalS16RGB,
    horizontalS16RGBA,
};

/* kHoriFuncTablePromotion[interleaving][baseFP][srcFP][dstFP] */
static const UpscaleHorizontalFunction kHorizontalFuncTablePromotion[ILCount][LdpFPUnsignedCount][LdpFPUnsignedCount][LdpFPUnsignedCount] = {
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(Planar),
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(YUYV),
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(NV12),
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(UYVY),
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(RGB),
    VN_REG_HORI_UNSIGNED_PROMOTION_FUNCS(RGBA),
};

/*------------------------------------------------------------------------------*/

/* kVerticalFunctionTable[srcFP][dstFP] */
static const UpscaleVerticalFunction kVerticalFunctionTable[LdpFPCount][LdpFPCount] = {
    /* U8        U10              U12               U14               S8.7         S10.5        S12.3        S14.1 */
    {verticalU8, verticalU8ToU10, verticalU8ToU12,  verticalU8ToU14,  NULL,        NULL,        NULL,        NULL},       /* U8    */
    {NULL,       verticalU10,     verticalU10ToU12, verticalU10ToU14, NULL,        NULL,        NULL,        NULL},       /* U10   */
    {NULL,       NULL,            verticalU12,      verticalU12ToU14, NULL,        NULL,        NULL,        NULL},       /* U12   */
    {NULL,       NULL,            NULL,             verticalU14,      NULL,        NULL,        NULL,        NULL},       /* U14   */
    {NULL,       NULL,            NULL,             NULL,             verticalS16, verticalS16, verticalS16, verticalS16},/* S8.7  */
    {NULL,       NULL,            NULL,             NULL,             verticalS16, verticalS16, verticalS16, verticalS16},/* S10.5 */
    {NULL,       NULL,            NULL,             NULL,             verticalS16, verticalS16, verticalS16, verticalS16},/* S12.3 */
    {NULL,       NULL,            NULL,             NULL,             verticalS16, verticalS16, verticalS16, verticalS16} /* S14.1 */
};

/*------------------------------------------------------------------------------*/

/* clang-format on */

UpscaleHorizontalFunction upscaleGetHorizontalFunction(Interleaving interleaving, LdpFixedPoint srcFP,
                                                       LdpFixedPoint dstFP, LdpFixedPoint baseFP)
{
    if (fixedPointIsSigned(srcFP)) {
        assert(fixedPointIsSigned(dstFP) && (!fixedPointIsValid(baseFP) || fixedPointIsSigned(baseFP)));

        /* For signed fixed-point types, the conversion is implicit as the radix
         * point shifts around - so there is no actual shifting required. */
        return kHorizontalFuncTableSigned[interleaving];
    }

    /* Non-converting upsample. */
    if ((srcFP == dstFP) &&                            /* No conversion between src && dst */
        ((dstFP == baseFP) || (baseFP == LdpFPCount))) /* No conversion between base & dst */
    {
        return kHorizontalFuncTableUnsigned[interleaving][srcFP];
    }

    /* Performing a conversion, if the base_fp is Count (i.e. PA is disabled),
     * then it doesn't matter which function we call - the implementation
     * explicitly checks for base pointers, a fallback to the src fp type
     * means a function should be present that will work. */
    if (!fixedPointIsValid(baseFP)) {
        baseFP = srcFP;
    }

    /* Converting upsample. Either because base or src needs converting to dst fp. */
    return kHorizontalFuncTablePromotion[interleaving][baseFP][srcFP][dstFP];
}

UpscaleVerticalFunction upscaleGetVerticalFunction(LdpFixedPoint srcFP, LdpFixedPoint dstFP)
{
    return kVerticalFunctionTable[srcFP][dstFP];
}

/*------------------------------------------------------------------------------*/
