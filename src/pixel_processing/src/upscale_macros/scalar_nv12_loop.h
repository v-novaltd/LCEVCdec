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

#if !defined(DIMENSION_1D)
#define DIMENSION_1D 1
#endif

#if !defined(DIMENSION_2D)
#define DIMENSION_2D 2
#endif

#if !defined(DIMENSION)
#error "DIMENSION must be defined before including this header"
#endif

#if DIMENSION != DIMENSION_1D && DIMENSION != DIMENSION_2D
#error "DIMENSION must be DIMENSION_1D or DIMENSION_2D"
#endif

#if IN_FIXED_POINT != FP_NV12 || OUT_FIXED_POINT != FP_NV12
#error "scalar_nv12_loop.h is for FP_NV12 -> FP_NV12 only"
#endif

{
    const bool ditherEnabled = (dither != NULL);
    const bool paEnabled = (params.applyPA != 0);

    const int32_t kernel[4] = {upscaleKernelGetPreparedCoefficient(&params, 0),
                               upscaleKernelGetPreparedCoefficient(&params, 1),
                               upscaleKernelGetPreparedCoefficient(&params, 2),
                               upscaleKernelGetPreparedCoefficient(&params, 3)};

    const int32_t srcWidthI = (int32_t)srcWidth;
    const int32_t rightEdgeSrcX = (srcWidthI >= 8) ? (srcWidthI - 8) : 0;

#if DIMENSION == DIMENSION_2D
    const int32_t dstHeight = (int32_t)(srcHeight << 1);
    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; ++srcY) {
#else
    for (int32_t srcY = (int32_t)yStart; srcY <= ((int32_t)yEnd) - 1; srcY += 2) {
#endif
        const uint16_t* ditherBuffer = NULL;
        if (ditherEnabled) {
            ditherBuffer = ldppDitherGetBuffer(dither, srcWidth << 2);
        }

#if DIMENSION == DIMENSION_2D
        const int32_t srcY0 = clampInt(srcY - 2, 0, (int32_t)srcHeight - 1);
        const int32_t srcY1 = clampInt(srcY - 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY2 = clampInt(srcY - 0, 0, (int32_t)srcHeight - 1);
        const int32_t srcY3 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);
        const int32_t srcY4 = clampInt(srcY + 2, 0, (int32_t)srcHeight - 1);

        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);
        const uint8_t* const srcRow2 = getRowU8(src, srcStride, srcY2);
        const uint8_t* const srcRow3 = getRowU8(src, srcStride, srcY3);
        const uint8_t* const srcRow4 = getRowU8(src, srcStride, srcY4);

        const int32_t dstY0 = clampInt((srcY * 2) + 0, 0, dstHeight - 1);
        const int32_t dstY1 = clampInt((srcY * 2) + 1, 0, dstHeight - 1);
        uint8_t* const dstRow0 = getRowU8(dst, dstStride, dstY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, dstY1);
#else
        const int32_t srcY0 = srcY;
        const int32_t srcY1 = clampInt(srcY + 1, 0, (int32_t)srcHeight - 1);

        const uint8_t* const srcRow0 = getRowU8(src, srcStride, srcY0);
        const uint8_t* const srcRow1 = getRowU8(src, srcStride, srcY1);

        uint8_t* const dstRow0 = getRowU8(dst, dstStride, srcY0);
        uint8_t* const dstRow1 = getRowU8(dst, dstStride, srcY1);
#endif

        for (int32_t srcX = 0; srcX <= srcWidthI; srcX += 8) {
            const bool rightEdge = (srcX >= rightEdgeSrcX);
            const int32_t blockSrcX = rightEdge ? rightEdgeSrcX : srcX;
            const int32_t dstX = blockSrcX * 2;

            int16_t src0U[8];
            int16_t src1U[8];
            int16_t src0V[8];
            int16_t src1V[8];
            scalarNV12LoadDeinterleaved8(srcRow0, srcWidthI, blockSrcX, src0U, src0V);
            scalarNV12LoadDeinterleaved8(srcRow1, srcWidthI, blockSrcX, src1U, src1V);

#if DIMENSION == DIMENSION_2D
            int16_t src2U[8];
            int16_t src3U[8];
            int16_t src4U[8];
            int16_t src2V[8];
            int16_t src3V[8];
            int16_t src4V[8];
            scalarNV12LoadDeinterleaved8(srcRow2, srcWidthI, blockSrcX, src2U, src2V);
            scalarNV12LoadDeinterleaved8(srcRow3, srcWidthI, blockSrcX, src3U, src3V);
            scalarNV12LoadDeinterleaved8(srcRow4, srcWidthI, blockSrcX, src4U, src4V);
#endif

            int16_t v0U[8];
            int16_t v1U[8];
            int16_t v0V[8];
            int16_t v1V[8];

#if DIMENSION == DIMENSION_2D
            scalarConvolveVerticalPair8(src0U, src1U, src2U, src3U, src4U, kernel, v0U, v1U);
            scalarConvolveVerticalPair8(src0V, src1V, src2V, src3V, src4V, kernel, v0V, v1V);
#else
            for (int32_t i = 0; i < 8; ++i) {
                v0U[i] = src0U[i];
                v1U[i] = src1U[i];
                v0V[i] = src0V[i];
                v1V[i] = src1V[i];
            }
#endif

            int16_t row0U[8];
            int16_t row1U[8];
            int16_t row0V[8];
            int16_t row1V[8];
            scalarConvolveHorizontal8(v0U, kernel, row0U);
            scalarConvolveHorizontal8(v0V, kernel, row0V);
            scalarConvolveHorizontal8(v1U, kernel, row1U);
            scalarConvolveHorizontal8(v1V, kernel, row1V);

            if (paEnabled) {
#if DIMENSION == DIMENSION_2D
                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t baseUX = clampInt(blockSrcX + (i << 1), 0, srcWidthI - 1);
                    const int32_t baseVX = clampInt(baseUX + 1, 0, srcWidthI - 1);
                    const int16_t baseU = scalarU8ToS16(srcRow2[baseUX]);
                    const int16_t baseV = scalarU8ToS16(srcRow2[baseVX]);
                    const int32_t pel = i << 1;

                    const int32_t meanU =
                        (row0U[pel + 0] + row0U[pel + 1] + row1U[pel + 0] + row1U[pel + 1] + 2) >> 2;
                    const int32_t meanV =
                        (row0V[pel + 0] + row0V[pel + 1] + row1V[pel + 0] + row1V[pel + 1] + 2) >> 2;
                    const int32_t adjustU = (int32_t)baseU - meanU;
                    const int32_t adjustV = (int32_t)baseV - meanV;

                    row0U[pel + 0] = saturateS16((int32_t)row0U[pel + 0] + adjustU);
                    row0U[pel + 1] = saturateS16((int32_t)row0U[pel + 1] + adjustU);
                    row1U[pel + 0] = saturateS16((int32_t)row1U[pel + 0] + adjustU);
                    row1U[pel + 1] = saturateS16((int32_t)row1U[pel + 1] + adjustU);

                    row0V[pel + 0] = saturateS16((int32_t)row0V[pel + 0] + adjustV);
                    row0V[pel + 1] = saturateS16((int32_t)row0V[pel + 1] + adjustV);
                    row1V[pel + 0] = saturateS16((int32_t)row1V[pel + 0] + adjustV);
                    row1V[pel + 1] = saturateS16((int32_t)row1V[pel + 1] + adjustV);
                }
#else
                for (int32_t i = 0; i < 4; ++i) {
                    const int32_t baseUX = clampInt(blockSrcX + (i << 1), 0, srcWidthI - 1);
                    const int32_t baseVX = clampInt(baseUX + 1, 0, srcWidthI - 1);
                    const int16_t base0U = scalarU8ToS16(srcRow0[baseUX]);
                    const int16_t base0V = scalarU8ToS16(srcRow0[baseVX]);
                    const int16_t base1U = scalarU8ToS16(srcRow1[baseUX]);
                    const int16_t base1V = scalarU8ToS16(srcRow1[baseVX]);
                    const int32_t pel = i << 1;

                    const int32_t mean0U = (row0U[pel + 0] + row0U[pel + 1] + 1) >> 1;
                    const int32_t mean0V = (row0V[pel + 0] + row0V[pel + 1] + 1) >> 1;
                    const int32_t mean1U = (row1U[pel + 0] + row1U[pel + 1] + 1) >> 1;
                    const int32_t mean1V = (row1V[pel + 0] + row1V[pel + 1] + 1) >> 1;
                    const int32_t adjust0U = (int32_t)base0U - mean0U;
                    const int32_t adjust0V = (int32_t)base0V - mean0V;
                    const int32_t adjust1U = (int32_t)base1U - mean1U;
                    const int32_t adjust1V = (int32_t)base1V - mean1V;

                    row0U[pel + 0] = saturateS16((int32_t)row0U[pel + 0] + adjust0U);
                    row0U[pel + 1] = saturateS16((int32_t)row0U[pel + 1] + adjust0U);
                    row0V[pel + 0] = saturateS16((int32_t)row0V[pel + 0] + adjust0V);
                    row0V[pel + 1] = saturateS16((int32_t)row0V[pel + 1] + adjust0V);

                    row1U[pel + 0] = saturateS16((int32_t)row1U[pel + 0] + adjust1U);
                    row1U[pel + 1] = saturateS16((int32_t)row1U[pel + 1] + adjust1U);
                    row1V[pel + 0] = saturateS16((int32_t)row1V[pel + 0] + adjust1V);
                    row1V[pel + 1] = saturateS16((int32_t)row1V[pel + 1] + adjust1V);
                }
#endif
            }

            if (ditherEnabled) {
                for (int32_t i = 0; i < 8; ++i) {
                    int32_t valueU0 = row0U[i];
                    ldppDitherApplyScalar(&valueU0, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row0U[i] = saturateS16(valueU0);

                    int32_t valueV0 = row0V[i];
                    ldppDitherApplyScalar(&valueV0, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row0V[i] = saturateS16(valueV0);
                }

                for (int32_t i = 0; i < 8; ++i) {
                    int32_t valueU1 = row1U[i];
                    ldppDitherApplyScalar(&valueU1, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row1U[i] = saturateS16(valueU1);

                    int32_t valueV1 = row1V[i];
                    ldppDitherApplyScalar(&valueV1, &ditherBuffer, (uint8_t)params.shift, dither->strength);
                    row1V[i] = saturateS16(valueV1);
                }
            }

            for (int32_t i = 0; i < 8; ++i) {
                const int32_t di = dstX + (i << 1);
                dstRow0[di + 0] = scalarS16ToU8(row0U[i]);
                dstRow0[di + 1] = scalarS16ToU8(row0V[i]);
                dstRow1[di + 0] = scalarS16ToU8(row1U[i]);
                dstRow1[di + 1] = scalarS16ToU8(row1V[i]);
            }
        }
    }
}

#undef OUT_FIXED_POINT
#undef IN_FIXED_POINT
