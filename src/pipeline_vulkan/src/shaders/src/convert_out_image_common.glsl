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

// Push constants are shared by every image-output shader.  The image variants
// reuse the slots `shift` and `sharpenStrength` (used by convert_out.comp) as
// chromaWidthShift / chromaHeightShift, so the host can use a single
// PushConstantsConversion struct across all conversion pipelines.  `nv21Swap`
// is declared only to keep the temporal fields at the same byte offsets as the
// buffer shader / host struct; it is not read here.
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
    int nv21Swap; // unused here; present for struct parity with convert_out.comp
    int containerStrideTemporalY;
    int containerOffsetTemporalY;
    int containerStrideTemporalU;
    int containerOffsetTemporalU;
    int containerStrideTemporalV;
    int containerOffsetTemporalV;
};

// Read one signed internal sample from the source SSBO at (px,py) inside the
// plane described by (stride,offset), folding in the co-located temporal
// residual (saturating, exactly as add.comp) when enabled for this plane.
int loadInternalValue(int px, int py, int stride, int offset, int tStride, int tOffset)
{
    const uint index = uint(offset + (px >> 1) + py * stride);
    ivec2 vals = unpack16bitInternals(src.samples[index]);
    if (addTemporal && tStride != 0) {
        const uint tIndex = uint(tOffset + (px >> 1) + py * tStride);
        vals = saturateS16(vals + unpack16bitInternals(temporal.samples[tIndex]));
    }
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

// BT.601 limited-range YUV -> RGB.  uf/vf must already have 0.5 subtracted.
vec3 yuvToRgb601(float yf, float uf, float vf)
{
    return clamp(vec3(yf + 1.403 * vf,
                      yf - 0.344 * uf - 0.714 * vf,
                      yf + 1.770 * uf),
                 vec3(0.0), vec3(1.0));
}
