/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

// Inlined methods for PictureLayout
//
#ifndef VN_LCEVC_PIPELINE_DETAIL_PICTURE_LAYOUT_H
#define VN_LCEVC_PIPELINE_DETAIL_PICTURE_LAYOUT_H

#include <assert.h>
#include <LCEVC/pipeline/picture_layout.h>

#ifdef __cplusplus
extern "C"
{
#endif

static inline uint8_t ldpPictureLayoutPlanes(const LdpPictureLayout* layout)
{
    if (layout->layoutInfo->format == LdpColorFormatNV12_8) {
        return 2;
    }
    uint8_t total = 0;
    for (uint32_t comp = 0; comp < layout->layoutInfo->colorComponents;
         comp += layout->layoutInfo->interleave[comp]) {
        total++;
    }
    return total;
}

static inline uint8_t ldpPictureLayoutPlaneInterleave(const LdpPictureLayout* layout, uint8_t plane)
{
    // Find a component which is in this plane
    assert(plane < ldpPictureLayoutPlanes(layout));
    return layout->layoutInfo->interleave[ldpPictureLayoutComponentForPlane(layout, plane)];
}

static inline LdpColorFormat ldpPictureLayoutFormat(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->format;
}

static inline uint32_t ldpPictureLayoutWidth(const LdpPictureLayout* layout)
{
    return layout->width;
}

static inline uint32_t ldpPictureLayoutHeight(const LdpPictureLayout* layout)
{
    return layout->height;
}

static inline uint32_t ldpPictureLayoutPlaneWidth(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));

    return (layout->width >> layout->layoutInfo->planeWidthShift[plane]) *
           ldpPictureLayoutPlaneInterleave(layout, plane);
}

static inline uint32_t ldpPictureLayoutPlaneHeight(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));

    return layout->height >> layout->layoutInfo->planeHeightShift[plane];
}

static inline bool ldpPictureLayoutIsValid(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->format != LdpColorFormatUnknown &&
           !(layout->width & layout->layoutInfo->validWidthMask ||
             layout->height & layout->layoutInfo->validHeightMask);
}

static inline uint8_t ldpPictureLayoutColorComponents(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->colorComponents;
}

static inline uint32_t ldpPictureLayoutSize(const LdpPictureLayout* layout) { return layout->size; }

static inline uint32_t ldpPictureLayoutPlaneSize(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    return layout->rowStrides[plane] * ldpPictureLayoutPlaneHeight(layout, plane);
}

static inline uint32_t ldpPictureLayoutPlaneOffset(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    return layout->planeOffsets[plane];
}

static inline uint32_t ldpPictureLayoutComponentOffset(const LdpPictureLayout* layout, uint8_t component)
{
    assert(component < layout->layoutInfo->colorComponents);
    return layout->planeOffsets[ldpPictureLayoutPlaneForComponent(layout, component)] +
           layout->layoutInfo->offset[component];
}

static inline uint8_t ldpPictureLayoutComponentInterleave(const LdpPictureLayout* layout, uint8_t component)
{
    assert(component < layout->layoutInfo->colorComponents);
    return layout->layoutInfo->interleave[component];
}

static inline uint32_t ldpPictureLayoutRowOffset(const LdpPictureLayout* layout, uint8_t plane, uint32_t row)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    assert(row < layout->height);
    return layout->planeOffsets[plane] + row * layout->rowStrides[plane] +
           layout->layoutInfo->offset[ldpPictureLayoutComponentForPlane(layout, plane)];
}

static inline uint32_t ldpPictureLayoutRowStride(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    return layout->rowStrides[plane];
}

static inline uint8_t ldpPictureLayoutSampleSize(const LdpPictureLayout* layout)
{
    return (layout->layoutInfo->bits + 7) / 8;
}

static inline uint32_t ldpPictureLayoutSampleStride(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    return ldpPictureLayoutSampleSize(layout) * layout->layoutInfo->interleave[plane];
}

static inline uint32_t ldpPictureLayoutRowSize(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    // Width of plane, rounded up to component units
    return ldpPictureLayoutSampleSize(layout) *
           (layout->width >> layout->layoutInfo->planeWidthShift[plane]) *
           ldpPictureLayoutPlaneInterleave(layout, plane);
}

static inline uint8_t ldpPictureLayoutSampleBits(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->bits;
}

static inline LdpColorSpace ldpPictureLayoutColorSpace(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->colorSpace;
}

static inline bool ldpPictureLayoutAreRowsContiguous(const LdpPictureLayout* layout, uint32_t plane)
{
    assert(plane < ldpPictureLayoutPlanes(layout));

    return (ldpPictureLayoutRowSize(layout, plane) == layout->rowStrides[plane]) &&
           (layout->layoutInfo->interleave[plane] == 0);
}

static inline bool ldpPictureLayoutAreSamplesContiguous(const LdpPictureLayout* layout, uint32_t component)
{
    assert(component < layout->layoutInfo->colorComponents);
    return layout->layoutInfo->interleave[component] == 1;
}

static inline const char* ldpPictureLayoutSuffix(const LdpPictureLayout* layout)
{
    return layout->layoutInfo->suffix;
}

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIPELINE_DETAIL_PICTURE_LAYOUT_H
