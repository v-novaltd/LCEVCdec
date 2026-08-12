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
    const int32_t dstHeight = (int32_t)(srcHeight << 1);
    const bool ditherEnabled = (dither != NULL);
    const bool paEnabled = (params.applyPA != 0);

    const int32_t kernel[4] = {upscaleKernelGetPreparedCoefficient(&params, 0),
                               upscaleKernelGetPreparedCoefficient(&params, 1),
                               upscaleKernelGetPreparedCoefficient(&params, 2),
                               upscaleKernelGetPreparedCoefficient(&params, 3)};

    const int32_t srcWidthI = (int32_t)srcWidth;
    const int32_t rightEdgeSrcX = (srcWidthI >= 4) ? (srcWidthI - 4) : 0;

    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; ++srcY) {
        const uint16_t* ditherBuffer = NULL;
        if (ditherEnabled) {
            ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
        }

        const int32_t srcY0 = clampInt(srcY - 2, 0, (int32_t)srcHeight - 1);
        const int32_t srcY1 = clampInt(srcY - 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY2 = clampInt(srcY - 0, 0, (int32_t)srcHeight - 1);
        const int32_t srcY3 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY4 = clampInt(srcY + 2, 0, (int32_t)srcHeight - 1);

#if IN_FIXED_POINT == FP_U8
        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);
        const uint8_t* const srcRow2 = getRowU8(src, srcStride, srcY2);
        const uint8_t* const srcRow3 = getRowU8(src, srcStride, srcY3);
        const uint8_t* const srcRow4 = getRowU8(src, srcStride, srcY4);
#else // N16
        const int16_t* const srcRow0 = getRowS16(src, srcStride, srcY0);
        const int16_t* const srcRow1 = getRowS16(src, srcStride, srcY1);
        const int16_t* const srcRow2 = getRowS16(src, srcStride, srcY2);
        const int16_t* const srcRow3 = getRowS16(src, srcStride, srcY3);
        const int16_t* const srcRow4 = getRowS16(src, srcStride, srcY4);
#endif

        const int32_t dstY0 = clampInt((srcY * 2) + 0, 0, dstHeight - 1);
        const int32_t dstY1 = clampInt((srcY * 2) + 1, 0, dstHeight - 1);

#if OUT_FIXED_POINT == FP_U8
        uint8_t* const dstRow0 = getRowU8(dst, dstStride, dstY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, dstY1);
#else // N16
        int16_t* const dstRow0 = getRowS16(dst, dstStride, dstY0);
        int16_t* const dstRow1 = getRowS16(dst, dstStride, dstY1);
#endif

        for (int32_t srcX = 0; srcX <= srcWidthI; srcX += 4) {
            const bool rightEdge = (srcX >= rightEdgeSrcX);
            const int32_t blockSrcX = rightEdge ? rightEdgeSrcX : srcX;

            int16_t src0[8];
            int16_t src1[8];
            int16_t src2[8];
            int16_t src3[8];
            int16_t src4[8];

            const int32_t dstX = blockSrcX * 2;

            for (int32_t i = 0; i < 8; ++i) {
                const int32_t sampleX = clampInt((blockSrcX - 2) + i, 0, srcWidthI - 1);

#if IN_FIXED_POINT == FP_U8
                src0[i] = scalarU8ToS16(srcRow0[sampleX]);
                src1[i] = scalarU8ToS16(srcRow1[sampleX]);
                src2[i] = scalarU8ToS16(srcRow2[sampleX]);
                src3[i] = scalarU8ToS16(srcRow3[sampleX]);
                src4[i] = scalarU8ToS16(srcRow4[sampleX]);
#elif IN_FIXED_POINT == FP_U16
                src0[i] = scalarU16ToS16(srcRow0[sampleX], (int16_t)params.shift);
                src1[i] = scalarU16ToS16(srcRow1[sampleX], (int16_t)params.shift);
                src2[i] = scalarU16ToS16(srcRow2[sampleX], (int16_t)params.shift);
                src3[i] = scalarU16ToS16(srcRow3[sampleX], (int16_t)params.shift);
                src4[i] = scalarU16ToS16(srcRow4[sampleX], (int16_t)params.shift);
#else // S16
                src0[i] = srcRow0[sampleX];
                src1[i] = srcRow1[sampleX];
                src2[i] = srcRow2[sampleX];
                src3[i] = srcRow3[sampleX];
                src4[i] = srcRow4[sampleX];
#endif
            }

            int16_t v0[8];
            int16_t v1[8];
            for (int32_t i = 0; i < 8; ++i) {
                const int64_t acc0 = ((int64_t)src0[i] * kernel[3]) + ((int64_t)src1[i] * kernel[2]) +
                                     ((int64_t)src2[i] * kernel[1]) + (src3[i] * kernel[0]);
                const int64_t acc1 = ((int64_t)src1[i] * kernel[0]) + ((int64_t)src2[i] * kernel[1]) +
                                     ((int64_t)src3[i] * kernel[2]) + (src4[i] * kernel[3]);

                v0[i] = scalarShiftRoundS15(acc0);
                v1[i] = scalarShiftRoundS15(acc1);
            }

            int16_t row0_16[8];
            int16_t row1_16[8];
            for (int32_t i = 0; i < 4; ++i) {
                const int64_t row0Odd = ((int64_t)v0[i + 0] * kernel[3]) +
                                        ((int64_t)v0[i + 1] * kernel[2]) +
                                        ((int64_t)v0[i + 2] * kernel[1]) + (v0[i + 3] * kernel[0]);
                const int64_t row0Even = ((int64_t)v0[i + 1] * kernel[0]) +
                                         ((int64_t)v0[i + 2] * kernel[1]) +
                                         ((int64_t)v0[i + 3] * kernel[2]) + (v0[i + 4] * kernel[3]);

                const int64_t row1Odd = ((int64_t)v1[i + 0] * kernel[3]) +
                                        ((int64_t)v1[i + 1] * kernel[2]) +
                                        ((int64_t)v1[i + 2] * kernel[1]) + (v1[i + 3] * kernel[0]);
                const int64_t row1Even = ((int64_t)v1[i + 1] * kernel[0]) +
                                         ((int64_t)v1[i + 2] * kernel[1]) +
                                         ((int64_t)v1[i + 3] * kernel[2]) + (v1[i + 4] * kernel[3]);

                row0_16[(i << 1) + 0] = scalarShiftRoundS15(row0Odd);
                row0_16[(i << 1) + 1] = scalarShiftRoundS15(row0Even);
                row1_16[(i << 1) + 0] = scalarShiftRoundS15(row1Odd);
                row1_16[(i << 1) + 1] = scalarShiftRoundS15(row1Even);
            }

            if (paEnabled) {
                int16_t base[4];
                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t baseX = clampInt(blockSrcX + i, 0, srcWidthI - 1);
#if IN_FIXED_POINT == FP_U8
                    base[i] = scalarU8ToS16(srcRow2[baseX]);
#elif IN_FIXED_POINT == FP_U16
                    base[i] = scalarU16ToS16(srcRow2[baseX], (int16_t)params.shift);
#else
                    base[i] = srcRow2[baseX];
#endif
                }

                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t pel = i << 1;
                    const int32_t mean4 =
                        (row0_16[pel + 0] + row0_16[pel + 1] + row1_16[pel + 0] + row1_16[pel + 1] + 2) >> 2;
                    const int32_t adjust = base[i] - mean4;

                    row0_16[pel + 0] = saturateS16((int32_t)row0_16[pel + 0] + adjust);
                    row0_16[pel + 1] = saturateS16((int32_t)row0_16[pel + 1] + adjust);
                    row1_16[pel + 0] = saturateS16((int32_t)row1_16[pel + 0] + adjust);
                    row1_16[pel + 1] = saturateS16((int32_t)row1_16[pel + 1] + adjust);
                }
            }

            if (ditherEnabled) {
                for (int32_t i = 0; i < 8; ++i) {
                    int32_t value = row0_16[i];
                    ldppDitherApplyScalar(&value, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row0_16[i] = saturateS16(value);
                }

                for (int32_t i = 0; i < 8; ++i) {
                    int32_t value = row1_16[i];
                    ldppDitherApplyScalar(&value, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row1_16[i] = saturateS16(value);
                }
            }

#if OUT_FIXED_POINT == FP_U8
            for (int32_t i = 0; i < 8; ++i) {
                dstRow0[dstX + i] = scalarS16ToU8(row0_16[i]);
                dstRow1[dstX + i] = scalarS16ToU8(row1_16[i]);
            }
#elif OUT_FIXED_POINT == FP_U16
            for (int32_t i = 0; i < 8; ++i) {
                dstRow0[dstX + i] = scalarS16ToU16(row0_16[i], (int16_t)params.offset,
                                                   (int16_t)params.shift, (int16_t)params.midpoint);
                dstRow1[dstX + i] = scalarS16ToU16(row1_16[i], (int16_t)params.offset,
                                                   (int16_t)params.shift, (int16_t)params.midpoint);
            }
#else // S16
            for (int32_t i = 0; i < 8; ++i) {
                dstRow0[dstX + i] = row0_16[i];
                dstRow1[dstX + i] = row1_16[i];
            }
#endif
        }
    }
}

#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT

#endif
