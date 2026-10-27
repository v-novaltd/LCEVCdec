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

uvec4 unpack8bit(uint packed)
{
    return uvec4(packed & 0xFF,         //
                 (packed >> 8) & 0xFF,  //
                 (packed >> 16) & 0xFF, //
                 (packed >> 24) & 0xFF);
}

uvec2 unpack16bit(uint packed) { return uvec2(packed & 0xFFFF, (packed >> 16) & 0xFFFF); }

ivec2 unpack16bitInternals(uint packed)
{
    const int ab = int(packed);
    return ivec2((ab << 16) >> 16, (ab >> 16));
}

uint pack8bit(uvec4 vals) { return vals.x | (vals.y << 8) | (vals.z << 16) | (vals.w << 24); }

uint pack16bit(uvec2 vals) { return vals.x | (vals.y << 16); }

uint pack16bitInternals(ivec2 vals) { return (vals.y << 16) | (vals.x & 0xFFFF); }

ivec4 toInternalFrom8bit(uvec4 vals)
{
    const int shift = 7;
    return ivec4(int((vals.x << shift) - 0x4000), //
                 int((vals.y << shift) - 0x4000), //
                 int((vals.z << shift) - 0x4000), //
                 int((vals.w << shift) - 0x4000));
}

ivec2 toInternalFrom16bit(uvec2 vals, int shift)
{
    return ivec2(int((vals.x << shift) - 0x4000), int((vals.y << shift) - 0x4000));
}

int clamp_s15(int v) { return clamp(v >> 14, -16384, 16383); }

int saturateS16(int v) { return clamp(v, -32768, 32767); }

ivec2 saturateS16(ivec2 v) { return ivec2(saturateS16(v.x), saturateS16(v.y)); }

uvec4 fromInternalTo8bit(ivec4 vals)
{
    const int shift = 7;
    const int halfv = ((1 << shift) / 2);
    return uvec4(uint(clamp(((vals.x + 0x4000 + halfv) >> shift), 0, 32767 >> shift)), //
                 uint(clamp(((vals.y + 0x4000 + halfv) >> shift), 0, 32767 >> shift)), //
                 uint(clamp(((vals.z + 0x4000 + halfv) >> shift), 0, 32767 >> shift)), //
                 uint(clamp(((vals.w + 0x4000 + halfv) >> shift), 0, 32767 >> shift)));
}

uvec2 fromInternalTo16bit(ivec2 vals, int shift)
{
    const int halfv = ((1 << shift) / 2);
    return uvec2(uint(clamp(((vals.x + 0x4000 + halfv) >> shift), 0, 32767 >> shift)),
                 uint(clamp(((vals.y + 0x4000 + halfv) >> shift), 0, 32767 >> shift)));
}
