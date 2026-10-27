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

#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in int fragBit8;
layout(location = 3) flat in int fragNv12;
layout(location = 4) flat in int fragScrWidth;

// clang-format off
layout(binding = 0) uniform UniformBufferObject {
    mat4 modelViewProjection;
    int bit8;
    int nv12;
    int srcWidth;
    int srcHeight;
    int ditherStrength;
    int ditherEnabled;
    int ditherShift;
    int ditherBufferSize;
    int limitedRange;
} ubo;
// clang-format on

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D texSamplerU;
layout(binding = 3) uniform sampler2D texSamplerV;

// clang-format off
layout(binding = 4) readonly buffer DitherEntropy {
    uint ditherEntropy[];
};

layout(binding = 5) readonly buffer DitherRowOffset {
    uint ditherRowOffset[];
};
// clang-format on

layout(location = 0) out vec4 outColor;

vec4 YUVtoRGBA(vec3 YUV)
{
    vec3 yuv = YUV;
    mat3 T;
    if (ubo.limitedRange != 0) {
        yuv.x -= 16.0 / 255.0;
        T = mat3(1.164, 1.164, 1.164, //
                 0.0, -0.392, 2.017,  //
                 1.596, -0.813, 0.0);
    } else {
        T = mat3(1.0, 1.0, 1.0,      //
                 0.0, -0.344, 1.770, //
                 1.403, -0.714, 0.0);
    }
    const vec3 offset = vec3(0.0, -0.5, -0.5);
    vec3 rgb = T * (yuv + offset);
    return vec4(rgb, 1.0);
}

int computeDither(int plane, int x, int y)
{
    if (ubo.ditherEnabled == 0 || ubo.ditherStrength == 0) {
        return 0;
    }

    const int height = max(ubo.srcHeight, 1);
    const int width = max(ubo.srcWidth, 1);
    const int clampedX = clamp(x, 0, width - 1);
    const int clampedY = clamp(y, 0, height - 1);

    const uint rowIndex = uint(plane * height + clampedY);
    const uint baseIndex = ditherRowOffset[rowIndex];
    const uint idx = baseIndex + uint(clampedX);
    const uint entropy = ditherEntropy[idx];

    const int strength = ubo.ditherStrength;
    return strength - int((entropy * uint(strength * 2 + 1)) >> 16);
}

void main()
{
    int scale = 1;
    if (fragBit8 == 0) {
        scale = 64; // 10 bit hack
    }

    float Y = scale * texture(texSampler, fragTexCoord).r;
    float U = scale * texture(texSamplerU, fragTexCoord).r;
    float V = scale * texture(texSamplerV, fragTexCoord).r;

    if (ubo.ditherEnabled != 0 && ubo.ditherStrength > 0) {
        const int x = int(gl_FragCoord.x);
        const int y = int(gl_FragCoord.y);
        const float sampleScale = (fragBit8 != 0) ? 255.0 : 1023.0;
        const float invSampleScale = 1.0 / sampleScale;

        const float ySample = clamp(Y * sampleScale + float(computeDither(0, x, y)), 0.0, sampleScale);
        const float uSample = clamp(U * sampleScale + float(computeDither(1, x, y)), 0.0, sampleScale);
        const float vSample = clamp(V * sampleScale + float(computeDither(2, x, y)), 0.0, sampleScale);

        Y = ySample * invSampleScale;
        U = uSample * invSampleScale;
        V = vSample * invSampleScale;
    }

    outColor = YUVtoRGBA(vec3(Y, U, V));
}
