/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

// Shared bindings, push constants and helpers for buffer-output conversion shaders.

#include "common.glsl"

layout(constant_id = SPECID_ADD_TEMPORAL) const bool addTemporal = false;
layout(constant_id = SPECID_SHARPEN) const bool sharpen = false;

// clang-format off
layout(std430, set = 0, binding = 0) buffer restrict readonly SrcBuffer {
    uint samples[];
} src;

layout(std430, set = 0, binding = 1) buffer restrict writeonly DstBuffer {
    uint samples[];
} dst;

layout(std430, set = 0, binding = 2) buffer restrict readonly TemporalBuffer {
    uint samples[];
} temporal;
// clang-format on

// Keep this prefix in lockstep with PushConstantsConversion. The trailing U-source and
// semiplanar-destination fields are used only by the fused NV12/NV21 and P010 shaders.
// clang-format off
layout(push_constant) uniform consts {
    int width;
    int height;
    int containerStrideIn;
    int containerOffsetIn;
    int containerStrideOut;
    int containerOffsetOut;
    int containerStrideV;
    int containerOffsetV;
    int shift;
    int sharpenStrength;
    int nv21Swap; // retained for layout parity; buffer-output shaders do not read it
    int containerStrideTemporal0;
    int containerOffsetTemporal0;
    int containerStrideTemporal1;
    int containerOffsetTemporal1;
    int containerStrideTemporal2;
    int containerOffsetTemporal2;
    int containerStrideU;
    int containerOffsetU;
    int containerStrideUVOut;
    int containerOffsetUVOut;
};
// clang-format on

ivec2 loadInternalPair(int packedX, int py, int stride, int offset, int temporalStride, int temporalOffset)
{
    const uint index = uint(offset + py * stride + packedX);
    ivec2 vals = unpack16bitInternals(src.samples[index]);
    if (addTemporal && temporalStride != 0) {
        const uint temporalIndex = uint(temporalOffset + py * temporalStride + packedX);
        vals = saturateS16(vals + unpack16bitInternals(temporal.samples[temporalIndex]));
    }
    return vals;
}

int loadInternalValue(int px, int py, int stride, int offset, int temporalStride, int temporalOffset)
{
    const ivec2 vals = loadInternalPair(px >> 1, py, stride, offset, temporalStride, temporalOffset);
    return ((px & 1) == 0) ? vals.x : vals.y;
}

int toOutputValue(int internalValue, int outputShift)
{
    const int rounding = (1 << outputShift) / 2;
    const int maxValue = 32767 >> outputShift;
    return clamp((internalValue + 0x4000 + rounding) >> outputShift, 0, maxValue);
}

int sharpenPixel(int px, int py, int outputShift)
{
    const int center = toOutputValue(loadInternalValue(px, py, containerStrideIn, containerOffsetIn,
                                                       containerStrideTemporal0, containerOffsetTemporal0),
                                     outputShift);
    if (px <= 0 || px + 1 >= width || py <= 0 || py + 1 >= height) {
        return center;
    }

    const int left = toOutputValue(loadInternalValue(px - 1, py, containerStrideIn, containerOffsetIn,
                                                     containerStrideTemporal0, containerOffsetTemporal0),
                                   outputShift);
    const int right =
        toOutputValue(loadInternalValue(px + 1, py, containerStrideIn, containerOffsetIn,
                                        containerStrideTemporal0, containerOffsetTemporal0),
                      outputShift);
    const int top = toOutputValue(loadInternalValue(px, py - 1, containerStrideIn, containerOffsetIn,
                                                    containerStrideTemporal0, containerOffsetTemporal0),
                                  outputShift);
    const int bottom =
        toOutputValue(loadInternalValue(px, py + 1, containerStrideIn, containerOffsetIn,
                                        containerStrideTemporal0, containerOffsetTemporal0),
                      outputShift);
    const int weight = (center << 2) - (left + right + top + bottom);

    // Preserve the full signed product: 14-bit output can overflow a 32-bit multiply here.
    int high;
    int low;
    imulExtended(sharpenStrength, weight, high, low);
    const uint lowBits = uint(low);
    const uint roundedLowBits = lowBits + 32768u;
    high += (roundedLowBits < lowBits) ? 1 : 0;
    const int coefficient = int((uint(high) << 16) | (roundedLowBits >> 16));
    return clamp(center + coefficient, 0, 32767 >> outputShift);
}
