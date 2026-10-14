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

// Shared bindings, push constants and helpers for the convert_out_image_*
// compute shaders.  Each shader still declares its own dst image binding
// (R8/R8G8 for NV12, R16/R16G16 for P010, RGBA8/RGB10A2 for the packed RGB
// variants) before including this file.

#include "common.glsl"

// Spcialization for adding temporal.
layout (constant_id = SPECID_ADD_TEMPORAL) const bool addTemporal = false;

// Source: internal 16-bit signed samples, packed two-per-uint via
// pack16bitInternals (see common.glsl).
layout(std430, set = 0, binding = 0) buffer restrict readonly SrcImageBuffer {
    uint samples[];
} src;

// Ttemporal residuals: same internal s16 packed format as src.
// Bound to src when addTemporal is false (read is spec-constant-eliminated).
layout(std430, set = 0, binding = 3) buffer restrict readonly TemporalBuffer {
    uint samples[];
} temporal;

// Push constants are shared by every image-output shader. The generic RGBA
// variants reuse the slots `shift` and `sharpenStrength` (used by planar buffer conversion)
// as chromaWidthShift / chromaHeightShift. Fixed 4:2:0 image shaders do not read
// those fields. `nv21Swap` is retained for host-struct
// layout parity; NV21 image output is selected by swapping source chroma planes.
layout(push_constant) uniform consts {
    int width;
    int height;
    int containerStrideY;
    int containerOffsetY;
    int containerStrideU;
    int containerOffsetU;
    int containerStrideV;
    int containerOffsetV;
    int chromaWidthShift;
    int chromaHeightShift;
    int nv21Swap; // unused by image shaders; present for struct layout parity
    int containerStrideTemporalY;
    int containerOffsetTemporalY;
    int containerStrideTemporalU;
    int containerOffsetTemporalU;
    int containerStrideTemporalV;
    int containerOffsetTemporalV;
};

// Read two adjacent signed internal samples from the source SSBO, folding in
// co-located temporal residuals with signed-16 saturation when enabled.
ivec2 loadInternalPair(int packedX, int py, int stride, int offset, int tStride, int tOffset)
{
    const uint index = uint(offset + packedX + py * stride);
    ivec2 vals = unpack16bitInternals(src.samples[index]);
    if (addTemporal && tStride != 0) {
        const uint tIndex = uint(tOffset + packedX + py * tStride);
        vals = saturateS16(vals + unpack16bitInternals(temporal.samples[tIndex]));
    }
    return vals;
}

// Read one signed internal sample from a packed pair.
int loadInternalValue(int px, int py, int stride, int offset, int tStride, int tOffset)
{
    const ivec2 vals = loadInternalPair(px >> 1, py, stride, offset, tStride, tOffset);
    return ((px & 1) == 0) ? vals.x : vals.y;
}

// Convert one internal 16-bit signed sample to a UNORM float in [0,1].
//
// `shift`  - downshift to land in the target unsigned range
//            (7 for 8-bit, 5 for 10-bit, 3 for 12-bit, 1 for 14-bit).
// `maxVal` - largest representable unsigned value in the target range
//            (255, 1023, 4095, 16383).
float internalToUnorm(int internalVal, int shift, int maxVal)
{
    const int halfv = (1 << shift) / 2;
    const int v = clamp(((internalVal + 0x4000 + halfv) >> shift), 0, maxVal);
    return float(v) / float(maxVal);
}

// Load one U/V pair for the RGBA image shaders. The returned components are
// centred around zero for YUV-to-RGB conversion.
vec2 loadChroma(int px, int py, int shift, int maxVal)
{
    const float u = internalToUnorm(loadInternalValue(px, py, containerStrideU, containerOffsetU,
                                                       containerStrideTemporalU, containerOffsetTemporalU),
                                    shift, maxVal);
    const float v = internalToUnorm(loadInternalValue(px, py, containerStrideV, containerOffsetV,
                                                       containerStrideTemporalV, containerOffsetTemporalV),
                                    shift, maxVal);
    return vec2(u, v) - vec2(0.5);
}


// BT.601 limited-range YUV -> RGB.  uf/vf must already have 0.5 subtracted.
vec3 yuvToRgb601(float yf, float uf, float vf)
{
    return clamp(vec3(yf + 1.403 * vf,
                      yf - 0.344 * uf - 0.714 * vf,
                      yf + 1.770 * uf),
                 vec3(0.0), vec3(1.0));
}
