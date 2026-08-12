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
#define IN_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_SCALAR_IN_FIXED_POINT(VN_UPSCALE_LOOP)
#define OUT_FIXED_POINT VN_UPSCALE_LOOP_CONFIG_SCALAR_OUT_FIXED_POINT(VN_UPSCALE_LOOP)
#endif

#if !defined(IN_FIXED_POINT) || !defined(OUT_FIXED_POINT)
#error "Define VN_UPSCALE_LOOP before including this header"
#endif

#if IN_FIXED_POINT == FP_NV12
#error "NV12 is not implemented for scalar upscale yet"
#else

{
    VNUnused(srcHeight); // 1d doesn't need height
    const bool ditherEnabled = (dither != NULL);
    const bool paEnabled = (params.applyPA != 0);

    const int32_t kernel[4] = {upscaleKernelGetPreparedCoefficient(&params, 0),
                               upscaleKernelGetPreparedCoefficient(&params, 1),
                               upscaleKernelGetPreparedCoefficient(&params, 2),
                               upscaleKernelGetPreparedCoefficient(&params, 3)};

    const int32_t srcWidthI = (int32_t)srcWidth;
    const int32_t rightEdgeSrcX = (srcWidthI >= 4) ? (srcWidthI - 4) : 0;

    for (int32_t row = (int32_t)yStart; row < (int32_t)yEnd; ++row) {
        const uint16_t* ditherBuffer = NULL;
        if (ditherEnabled) {
            ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
        }

#if IN_FIXED_POINT == FP_U8
        const uint8_t* const srcRow = getRowU8(src, srcStride, row);
#else // N16
        const int16_t* const srcRow = getRowS16(src, srcStride, row);
#endif

#if OUT_FIXED_POINT == FP_U8
        uint8_t* const dstRow = getRowU8(dst, dstStride, row);
#else // N16
        int16_t* const dstRow = getRowS16(dst, dstStride, row);
#endif

        for (int32_t srcX = 0; srcX <= srcWidthI; srcX += 4) {
            const bool rightEdge = (srcX >= rightEdgeSrcX);
            const int32_t blockSrcX = rightEdge ? rightEdgeSrcX : srcX;
            const int32_t dstX = blockSrcX * 2;

            int16_t srcPel[8];
            for (int32_t i = 0; i < 8; ++i) {
                const int32_t sampleX = clampInt((blockSrcX - 2) + i, 0, srcWidthI - 1);

#if IN_FIXED_POINT == FP_U8
                srcPel[i] = scalarU8ToS16(srcRow[sampleX]);
#elif IN_FIXED_POINT == FP_U16
                srcPel[i] = scalarU16ToS16(srcRow[sampleX], (int16_t)params.shift);
#else // S16
                srcPel[i] = srcRow[sampleX];
#endif
            }

            int16_t row_16[8];
            for (int32_t i = 0; i < 4; ++i) {
                const int64_t rowOdd =
                    ((int64_t)srcPel[i + 0] * kernel[3]) + ((int64_t)srcPel[i + 1] * kernel[2]) +
                    ((int64_t)srcPel[i + 2] * kernel[1]) + (srcPel[i + 3] * kernel[0]);
                const int64_t rowEven =
                    ((int64_t)srcPel[i + 1] * kernel[0]) + ((int64_t)srcPel[i + 2] * kernel[1]) +
                    ((int64_t)srcPel[i + 3] * kernel[2]) + (srcPel[i + 4] * kernel[3]);

                row_16[(i << 1) + 0] = scalarShiftRoundS15(rowOdd);
                row_16[(i << 1) + 1] = scalarShiftRoundS15(rowEven);
            }

            if (paEnabled) {
                int16_t base[4];
                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t baseX = clampInt(blockSrcX + i, 0, srcWidthI - 1);
#if IN_FIXED_POINT == FP_U8
                    base[i] = scalarU8ToS16(srcRow[baseX]);
#elif IN_FIXED_POINT == FP_U16
                    base[i] = scalarU16ToS16(srcRow[baseX], (int16_t)params.shift);
#else
                    base[i] = srcRow[baseX];
#endif
                }

                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t pel = i << 1;
                    const int32_t mean2 = (row_16[pel + 0] + row_16[pel + 1] + 1) >> 1;
                    const int32_t adjust = base[i] - mean2;
                    row_16[pel + 0] = saturateS16((int32_t)row_16[pel + 0] + adjust);
                    row_16[pel + 1] = saturateS16((int32_t)row_16[pel + 1] + adjust);
                }
            }

            if (ditherEnabled) {
                for (int32_t i = 0; i < 8; ++i) {
                    int32_t value = row_16[i];
                    ldppDitherApplyScalar(&value, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row_16[i] = saturateS16(value);
                }
            }

#if OUT_FIXED_POINT == FP_U8
            for (int32_t i = 0; i < 8; ++i) {
                dstRow[dstX + i] = scalarS16ToU8(row_16[i]);
            }
#elif OUT_FIXED_POINT == FP_U16
            for (int32_t i = 0; i < 8; ++i) {
                dstRow[dstX + i] = scalarS16ToU16(row_16[i], (int16_t)params.offset,
                                                  (int16_t)params.shift, (int16_t)params.midpoint);
            }
#else // S16
            for (int32_t i = 0; i < 8; ++i) {
                dstRow[dstX + i] = row_16[i];
            }
#endif
        }
    }
}

#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT

#endif
